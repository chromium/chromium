// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/filter/filter_source_stream_test_util.h"

#include <cstring>

#include "base/check_op.h"
#include "third_party/zlib/zlib.h"

namespace net {

// Compress |source| with length |source_len|. Write output into |dest|, and
// output length into |dest_len|. If |gzip_framing| is true, header will be
// added.
void CompressGzip(const char* source,
                  size_t source_len,
                  char* dest,
                  size_t* dest_len,
                  bool gzip_framing) {
  size_t dest_left = *dest_len;
  z_stream zlib_stream;
  memset(&zlib_stream, 0, sizeof(zlib_stream));
  int code;
  if (gzip_framing) {
    const int kMemLevel = 8;  // the default, see deflateInit2(3)
    code = deflateInit2(&zlib_stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                        -MAX_WBITS, kMemLevel, Z_DEFAULT_STRATEGY);
  } else {
    code = deflateInit(&zlib_stream, Z_DEFAULT_COMPRESSION);
  }
  DCHECK_EQ(Z_OK, code);

  // If compressing with gzip framing, prepend a gzip header. See RFC 1952 2.2
  // and 2.3 for more information.
  if (gzip_framing) {
    const unsigned char gzip_header[] = {
        0x1f,
        0x8b,  // magic number
        0x08,  // CM 0x08 == "deflate"
        0x00,  // FLG 0x00 == nothing
        0x00, 0x00, 0x00,
        0x00,  // MTIME 0x00000000 == no mtime
        0x00,  // XFL 0x00 == nothing
        0xff,  // OS 0xff == unknown
    };
    DCHECK_GE(dest_left, sizeof(gzip_header));
    memcpy(dest, gzip_header, sizeof(gzip_header));
    dest += sizeof(gzip_header);
    dest_left -= sizeof(gzip_header);
  }

  zlib_stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(source));
  zlib_stream.avail_in = source_len;
  zlib_stream.next_out = reinterpret_cast<Bytef*>(dest);
  zlib_stream.avail_out = dest_left;

  code = deflate(&zlib_stream, Z_FINISH);
  DCHECK_EQ(Z_STREAM_END, code);
  dest_left = zlib_stream.avail_out;

  deflateEnd(&zlib_stream);
  *dest_len -= dest_left;
}

}  // namespace net
