// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/filter/gzip_header.h"

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <array>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "third_party/zlib/zlib.h"

namespace net {

// gzip magic header.
static constexpr std::array<uint8_t, 2> kGzipHeaderBytes = {0x1f, 0x8b};

GZipHeader::GZipHeader() {
  Reset();
}

GZipHeader::~GZipHeader() = default;

void GZipHeader::Reset() {
  state_        = IN_HEADER_ID1;
  flags_        = 0;
  extra_length_ = 0;
}

GZipHeader::Status GZipHeader::ReadMore(base::span<const uint8_t> inbuf,
                                        size_t& header_end) {
  auto pos = inbuf.begin();

  while (pos != inbuf.end()) {
    switch ( state_ ) {
      case IN_HEADER_ID1:
        if (*pos != kGzipHeaderBytes[0]) {
          return INVALID_HEADER;
        }
        pos++;
        state_++;
        break;
      case IN_HEADER_ID2:
        if (*pos != kGzipHeaderBytes[1]) {
          return INVALID_HEADER;
        }
        pos++;
        state_++;
        break;
      case IN_HEADER_CM:
        if ( *pos != Z_DEFLATED )  return INVALID_HEADER;
        pos++;
        state_++;
        break;
      case IN_HEADER_FLG:
        flags_ = (*pos) & (FLAG_FHCRC | FLAG_FEXTRA |
                           FLAG_FNAME | FLAG_FCOMMENT);
        pos++;
        state_++;
        break;

      case IN_HEADER_MTIME_BYTE_0:
        pos++;
        state_++;
        break;
      case IN_HEADER_MTIME_BYTE_1:
        pos++;
        state_++;
        break;
      case IN_HEADER_MTIME_BYTE_2:
        pos++;
        state_++;
        break;
      case IN_HEADER_MTIME_BYTE_3:
        pos++;
        state_++;
        break;

      case IN_HEADER_XFL:
        pos++;
        state_++;
        break;

      case IN_HEADER_OS:
        pos++;
        state_++;
        break;

      case IN_XLEN_BYTE_0:
        if ( !(flags_ & FLAG_FEXTRA) ) {
          state_ = IN_FNAME;
          break;
        }
        // We have a two-byte little-endian length, followed by a
        // field of that length.
        extra_length_ = *pos;
        pos++;
        state_++;
        break;
      case IN_XLEN_BYTE_1:
        extra_length_ += *pos << 8;
        pos++;
        state_++;
        // We intentionally fall through, because if we have a
        // zero-length FEXTRA, we want to check to notice that we're
        // done reading the FEXTRA before we exit this loop...
        [[fallthrough]];

      case IN_FEXTRA: {
        // Grab the rest of the bytes in the extra field, or as many
        // of them as are actually present so far.
        const uint16_t num_extra_bytes = static_cast<uint16_t>(std::min(
            static_cast<ptrdiff_t>(extra_length_), (inbuf.end() - pos)));
        pos += num_extra_bytes;
        extra_length_ -= num_extra_bytes;
        if ( extra_length_ == 0 ) {
          state_ = IN_FNAME;   // advance when we've seen extra_length_ bytes
          flags_ &= ~FLAG_FEXTRA;   // we're done with the FEXTRA stuff
        }
        break;
      }

      case IN_FNAME:
        if ( !(flags_ & FLAG_FNAME) ) {
          state_ = IN_FCOMMENT;
          break;
        }
        // See if we can find the end of the null-byte-terminated FNAME field.
        pos = std::find(pos, inbuf.end(), 0u);
        // If the null was found, the end of the FNAME has been reached, and
        // need to advance to the next character.
        if (pos != inbuf.end()) {
          pos++;                  // Advance past the null-byte.
          flags_ &= ~FLAG_FNAME;  // We're done with the FNAME stuff.
          state_ = IN_FCOMMENT;
        }
        // Otherwise, everything so far is part of the FNAME, and still have to
        // continue looking for the null byte, so nothing else to do until more
        // data is received.
        break;

      case IN_FCOMMENT:
        if ( !(flags_ & FLAG_FCOMMENT) ) {
          state_ = IN_FHCRC_BYTE_0;
          break;
        }
        // See if we can find the end of the null-byte-terminated FCOMMENT
        // field.
        pos = std::find(pos, inbuf.end(), 0u);
        // If the null was found, the end of the FNAME has been reached, and
        // need to advance to the next character.
        if (pos != inbuf.end()) {
          pos++;                     // Advance past the null-byte.
          flags_ &= ~FLAG_FCOMMENT;  // We're done with the FCOMMENT stuff.
          state_ = IN_FHCRC_BYTE_0;
        }
        // Otherwise, everything so far is part of the FCOMMENT, and still have
        // to continue looking for the null byte, so nothing else to do until
        // more data is received.
        break;

      case IN_FHCRC_BYTE_0:
        if ( !(flags_ & FLAG_FHCRC) ) {
          state_ = IN_DONE;
          break;
        }
        pos++;
        state_++;
        break;

      case IN_FHCRC_BYTE_1:
        pos++;
        flags_ &= ~FLAG_FHCRC;   // we're done with the FHCRC stuff
        state_++;
        break;

      case IN_DONE:
        header_end = pos - inbuf.begin();
        return COMPLETE_HEADER;
    }
  }

  if ( (state_ > IN_HEADER_OS) && (flags_ == 0) ) {
    header_end = pos - inbuf.begin();
    return COMPLETE_HEADER;
  } else {
    return INCOMPLETE_HEADER;
  }
}

bool GZipHeader::HasGZipHeader(base::span<const uint8_t> inbuf) {
  size_t ignored_header_end = 0u;
  return GZipHeader().ReadMore(inbuf, ignored_header_end) == COMPLETE_HEADER;
}

}  // namespace net
