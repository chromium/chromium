// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/filter/filter_source_stream_test_util.h"

#include <stdint.h>

#include <cstring>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "net/base/io_buffer.h"
#include "third_party/zlib/zlib.h"

namespace net {
namespace {

// Gzip file header (RFC 1952).
constexpr uint8_t kGzipHeader[] = {
    0x1f,
    0x8b,  // magic number
    0x08,  // CM 0x08 == "deflate"
    0x00,  // FLG 0x00 == nothing
    0x00, 0x00, 0x00,
    0x00,  // MTIME 0x00000000 == no mtime
    0x00,  // XFL 0x00 == nothing
    0xff,  // OS 0xff == unknown
};

// Size of the buffer used for compression chunks.
const size_t kBufferSize = 4096;

// Concatenates multiple segments into a single vector.
std::vector<uint8_t> Concat(
    const std::vector<base::span<const uint8_t>>& segments) {
  size_t total_size = 0;
  // Calculate the total size needed.
  for (const auto& segment : segments) {
    total_size += segment.size();
  }
  std::vector<uint8_t> result;
  // Reserve space to avoid reallocations.
  result.reserve(total_size);
  // Append data from each span.
  for (const auto& segment : segments) {
    result.insert(result.end(), segment.begin(), segment.end());
  }
  return result;
}

}  // namespace

// Compresses `source` using deflate, optionally with gzip framing.
std::vector<uint8_t> CompressGzip(std::string_view source, bool gzip_framing) {
  // Vector to store the resulting compressed data segments.
  std::vector<base::span<const uint8_t>> segments;

  // Initialize the zlib stream.
  z_stream zlib_stream = {};
  int code;
  if (gzip_framing) {
    const int kMemLevel = 8;  // the default, see deflateInit2(3)
    code = deflateInit2(&zlib_stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                        -MAX_WBITS, kMemLevel, Z_DEFAULT_STRATEGY);
    // If compressing with gzip framing, prepend a gzip header. See RFC 1952 2.2
    // and 2.3 for more information.
    segments.push_back(base::span(kGzipHeader));
  } else {
    // Initialize for raw deflate.
    code = deflateInit(&zlib_stream, Z_DEFAULT_COMPRESSION);
  }
  CHECK_EQ(code, Z_OK);

  // Set the input data for the zlib stream.
  zlib_stream.next_in =
      reinterpret_cast<Bytef*>(const_cast<char*>(source.data()));
  zlib_stream.avail_in = source.size();

  // Buffers to hold the output of the compression.
  std::vector<scoped_refptr<net::GrowableIOBuffer>> buffers;

  // Compress the data in chunks.
  while (code != Z_STREAM_END) {
    // Create a new buffer for the compressed output.
    auto buffer = base::MakeRefCounted<net::GrowableIOBuffer>();
    buffers.push_back(buffer);
    buffer->SetCapacity(kBufferSize);

    // Set the output buffer for zlib.
    zlib_stream.next_out = reinterpret_cast<Bytef*>(buffer->data());
    zlib_stream.avail_out = kBufferSize;

    // Perform the compression, flushing with Z_FINISH at the end.
    code = deflate(&zlib_stream, Z_FINISH);
    CHECK(code == Z_STREAM_END || code == Z_OK) << "code " << code;
    CHECK_LE(zlib_stream.avail_out, kBufferSize);
    // Store the compressed data from the buffer.
    segments.push_back(buffer->first(kBufferSize - zlib_stream.avail_out));
  }

  // Clean up the zlib stream.
  code = deflateEnd(&zlib_stream);
  CHECK_EQ(code, Z_OK);

  // Combine all the compressed data segments.
  return Concat(segments);
}

}  // namespace net
