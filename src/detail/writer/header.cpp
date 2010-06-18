/******************************************************************************
 * $Id$
 *
 * Project:  libLAS - http://liblas.org - A BSD library for LAS format data.
 * Purpose:  LAS header writer implementation for C++ libLAS 
 * Author:   Howard Butler, hobu.inc@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2010, Howard Butler
 *
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following 
 * conditions are met:
 * 
 *     * Redistributions of source code must retain the above copyright 
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright 
 *       notice, this list of conditions and the following disclaimer in 
 *       the documentation and/or other materials provided 
 *       with the distribution.
 *     * Neither the name of the Martin Isenburg or Iowa Department 
 *       of Natural Resources nor the names of its contributors may be 
 *       used to endorse or promote products derived from this software 
 *       without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, 
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS 
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED 
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, 
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT 
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 ****************************************************************************/

#include <liblas/detail/writer/header.hpp>
#include <liblas/detail/utility.hpp>
#include <liblas/lasheader.hpp>
#include <liblas/laspoint.hpp>
#include <liblas/lasspatialreference.hpp>

#include <cassert>
#include <cstdlib> // std::size_t
#include <fstream>
#include <iosfwd>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>


namespace liblas { namespace detail { namespace writer {

Header::Header(std::ostream& ofs, liblas::uint32_t& count, liblas::Header const& header) :
    Base(ofs, count)
{
    m_header = header;
}

void Header::write()
{

    uint8_t n1 = 0;
    uint16_t n2 = 0;
    uint32_t n4 = 0;

    // Rewrite the georeference VLR entries if they exist
    m_header.SetGeoreference();
    
    // Figure out how many points we already have.  

    // Seek to the beginning
    GetStream().seekp(0, std::ios::beg);
    std::ios::pos_type beginning = GetStream().tellp();

    // Seek to the end
    GetStream().seekp(0, std::ios::end);
    std::ios::pos_type end = GetStream().tellp();
    // std::ios::off_type size = end - beginning;
     
    std::ios::off_type count = (end - static_cast<std::ios::off_type>(m_header.GetDataOffset())) / 
                                 static_cast<std::ios::off_type>(m_header.GetDataRecordLength());
    
    // This test should only be true if we were opened in both 
    // std::ios::in *and* std::ios::out, otherwise it should return false 
    // and we won't adjust the point count.
    
    if ((beginning != end) && (end != static_cast<std::ios::pos_type>(0))) {
        liblas::uint32_t& cnt =  GetPointCount();
        cnt = static_cast<liblas::uint32_t>(count);
        SetPointCount(cnt);

        // Position to the beginning of the file to start writing the header
        GetStream().seekp(0, std::ios::beg);
    }

    // 1. File Signature
    std::string const filesig(m_header.GetFileSignature());
    assert(filesig.size() == 4);
    detail::write_n(GetStream(), filesig, 4);
    
    
    // 2. File SourceId / Reserved
    if (m_header.GetVersionMinor()  ==  0) {
        n4 = m_header.GetReserved();
        detail::write_n(GetStream(), n4, sizeof(n4));         
    } else if (m_header.GetVersionMinor()  >  0) {
        n2 = m_header.GetFileSourceId();
        detail::write_n(GetStream(), n2, sizeof(n2));                
        n2 = m_header.GetReserved();
        detail::write_n(GetStream(), n2, sizeof(n2));        
    } 


    // 3-6. GUID data
    uint32_t d1 = 0;
    uint16_t d2 = 0;
    uint16_t d3 = 0;
    uint8_t d4[8] = { 0 };
    liblas::guid g = m_header.GetProjectId();
    g.output_data(d1, d2, d3, d4);
    detail::write_n(GetStream(), d1, sizeof(d1));
    detail::write_n(GetStream(), d2, sizeof(d2));
    detail::write_n(GetStream(), d3, sizeof(d3));
    detail::write_n(GetStream(), d4, sizeof(d4));
    
    // 7. Version major
    n1 = m_header.GetVersionMajor();
    assert(1 == n1);
    detail::write_n(GetStream(), n1, sizeof(n1));
    
    // 8. Version minor
    n1 = m_header.GetVersionMinor();
    detail::write_n(GetStream(), n1, sizeof(n1));

    // 9. System ID
    std::string sysid(m_header.GetSystemId(true));
    assert(sysid.size() == 32);
    detail::write_n(GetStream(), sysid, 32);
    
    // 10. Generating Software ID
    std::string softid(m_header.GetSoftwareId(true));
    assert(softid.size() == 32);
    detail::write_n(GetStream(), softid, 32);

    // 11. Flight Date Julian
    n2 = m_header.GetCreationDOY();
    detail::write_n(GetStream(), n2, sizeof(n2));

    // 12. Year
    n2 = m_header.GetCreationYear();
    detail::write_n(GetStream(), n2, sizeof(n2));

    // 13. Header Size
    n2 = m_header.GetHeaderSize();
    assert(227 <= n2);
    detail::write_n(GetStream(), n2, sizeof(n2));

    // 14. Offset to data
    // For 1.0 data, we must also add pad bytes to the end of the 
    // m_header.  This means resetting the dataoffset +=2 
    if (m_header.GetVersionMinor()  ==  0) {
        int32_t difference = m_header.GetDataOffset() - m_header.GetHeaderSize();
        if (    (m_header.GetDataOffset() != m_header.GetHeaderSize()) &&
                (difference > 0) && ( difference >= 2)
            ) 
            {
                n4 = m_header.GetDataOffset();
            } 
        else if (difference == 0) 
        {
            n4 = m_header.GetDataOffset() + 2;
            m_header.SetDataOffset(n4);
        } 
        else if (difference < 0) 
        {
            throw std::out_of_range("DataOffset is smaller than HeaderSize!");
        } 
        else 
        {
            n4 = m_header.GetDataOffset() + 2;
            m_header.SetDataOffset(n4);
        }
        // n4 = m_header.GetDataOffset() + 2;
    } else {
        n4 = m_header.GetDataOffset();        
    }
    detail::write_n(GetStream(), n4, sizeof(n4));

    // 15. Number of variable length records
    n4 = m_header.GetRecordsCount();
    detail::write_n(GetStream(), n4, sizeof(n4));

    // 16. Point Data Format ID
    n1 = static_cast<uint8_t>(m_header.GetDataFormatId());
    detail::write_n(GetStream(), n1, sizeof(n1));

    // 17. Point Data Record Length
    n2 = m_header.GetDataRecordLength();
    detail::write_n(GetStream(), n2, sizeof(n2));

    // 18. Number of point records
    // This value is updated if necessary, see UpdateHeader function.
    n4 = m_header.GetPointRecordsCount();
    detail::write_n(GetStream(), n4, sizeof(n4));

    // 19. Number of points by return
    std::vector<uint32_t>::size_type const srbyr = 5;
    std::vector<uint32_t> const& vpbr = m_header.GetPointRecordsByReturnCount();
    assert(vpbr.size() <= srbyr);
    uint32_t pbr[srbyr] = { 0 };
    std::copy(vpbr.begin(), vpbr.end(), pbr);
    detail::write_n(GetStream(), pbr, sizeof(pbr));

    // 20-22. Scale factors
    detail::write_n(GetStream(), m_header.GetScaleX(), sizeof(double));
    detail::write_n(GetStream(), m_header.GetScaleY(), sizeof(double));
    detail::write_n(GetStream(), m_header.GetScaleZ(), sizeof(double));

    // 23-25. Offsets
    detail::write_n(GetStream(), m_header.GetOffsetX(), sizeof(double));
    detail::write_n(GetStream(), m_header.GetOffsetY(), sizeof(double));
    detail::write_n(GetStream(), m_header.GetOffsetZ(), sizeof(double));

    // 26-27. Max/Min X
    detail::write_n(GetStream(), m_header.GetMaxX(), sizeof(double));
    detail::write_n(GetStream(), m_header.GetMinX(), sizeof(double));

    // 28-29. Max/Min Y
    detail::write_n(GetStream(), m_header.GetMaxY(), sizeof(double));
    detail::write_n(GetStream(), m_header.GetMinY(), sizeof(double));

    // 30-31. Max/Min Z
    detail::write_n(GetStream(), m_header.GetMaxZ(), sizeof(double));
    detail::write_n(GetStream(), m_header.GetMinZ(), sizeof(double));

    // If WriteVLR returns a value, it is because the header's 
    // offset is not large enough to contain the VLRs.  The value 
    // it returns is the number of bytes we must increase the header
    // by in order for it to contain the VLRs.
    
    int32_t difference = WriteVLRs();
    if (difference < 0) {
        m_header.SetDataOffset(m_header.GetDataOffset() + abs(difference) );
        WriteVLRs();
        
        // Make sure to rewrite the dataoffset in the header portion now that
        // we've changed it.
        std::streamsize const current_pos = GetStream().tellp();
        std::streamsize const offset_pos = 96; 
        GetStream().seekp(offset_pos, std::ios::beg);
        detail::write_n(GetStream(), m_header.GetDataOffset() , sizeof(m_header.GetDataOffset()));
        GetStream().seekp(current_pos, std::ios::beg);      
    }

    // Write the 1.0 pad signature if we need to.
    WriteLAS10PadSignature(); 
           
    // If we already have points, we're going to put it at the end of the file.  
    // If we don't have any points,  we're going to leave it where it is.
    if (GetPointCount() != 0)
        GetStream().seekp(0, std::ios::end);
    
}

int32_t Header::WriteVLRs() 
{
    // If this function returns a value, it is the size that the header's 
    // data offset must be increased by in order for the VLRs to fit in 
    // the header.  
    GetStream().seekp(m_header.GetHeaderSize(), std::ios::beg);

    // if the VLRs won't fit because the data offset is too 
    // small, we need to throw an error.
    uint32_t vlr_total_size = 0;
        
    // Calculate a new data offset size
    for (uint32_t i = 0; i < m_header.GetRecordsCount(); ++i)
    {
        VariableRecord vlr = m_header.GetVLR(i);
        vlr_total_size += vlr.GetTotalSize();
    }
    
    int32_t difference = m_header.GetDataOffset() - (vlr_total_size + m_header.GetHeaderSize());

    if (difference < 0) 
    {
        return difference;
    }
    
    for (uint32_t i = 0; i < m_header.GetRecordsCount(); ++i)
    {
        VariableRecord vlr = m_header.GetVLR(i);

        detail::write_n(GetStream(), vlr.GetReserved(), sizeof(uint16_t));
        detail::write_n(GetStream(), vlr.GetUserId(true).c_str(), 16);
        detail::write_n(GetStream(), vlr.GetRecordId(), sizeof(uint16_t));
        detail::write_n(GetStream(), vlr.GetRecordLength(), sizeof(uint16_t));
        detail::write_n(GetStream(), vlr.GetDescription(true).c_str(), 32);
        std::vector<uint8_t> const& data = vlr.GetData();
        std::streamsize const size = static_cast<std::streamsize>(data.size());
        detail::write_n(GetStream(), data.front(), size);
    }

    // if we had more room than we need for the VLRs, we need to pad that with 
    // 0's.  We must also not forget to add the 1.0 pad bytes to the end of this
    // but the impl should be the one doing that, not us.
    if (difference > 0) {
        detail::write_n(GetStream(), "\0", difference);
    }

    return 0;
}

void Header::WriteLAS10PadSignature()
{
    // Only write pad signature bytes for LAS 1.0 files.  Any other files 
    // will not get the pad bytes and we are *not* allowing anyone to 
    // override this either - hobu
    
    if (m_header.GetVersionMinor() > 0) {
        return;
    }
    
    // step back two bytes to write the pad bytes.  We should have already
    // determined by this point if a) they will fit b) they won't overwrite 
    // exiting real data 
    GetStream().seekp(m_header.GetDataOffset() - 2, std::ios::beg);
    
    // Write the pad bytes.
    uint8_t const sgn1 = 0xCC;
    uint8_t const sgn2 = 0xDD;
    detail::write_n(GetStream(), sgn1, sizeof(uint8_t));
    detail::write_n(GetStream(), sgn2, sizeof(uint8_t));
}


}}} // namespace liblas::detail::writer
