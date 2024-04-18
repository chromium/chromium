// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/gzipper.h"

#include "base/containers/span.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "third_party/zlib/google/compression_utils.h"
#include "third_party/zlib/google/compression_utils_portable.h"

namespace data_decoder {

namespace {

mojo_base::BigBuffer StringToBuffer(const std::string& string) {
  return base::as_bytes(base::make_span(string));
}

}  // namespace

Gzipper::Gzipper() = default;
Gzipper::~Gzipper() = default;

void Gzipper::Deflate(mojo_base::BigBuffer data, DeflateCallback callback) {
  uLongf compressed_data_size = compressBound(data.size());
  std::vector<uint8_t> compressed_data(compressed_data_size);
  if (zlib_internal::CompressHelper(
          zlib_internal::ZRAW, compressed_data.data(), &compressed_data_size,
          reinterpret_cast<const Bytef*>(data.data()), data.size(),
          Z_DEFAULT_COMPRESSION,
          /*malloc_fn=*/nullptr, /*free_fn=*/nullptr) != Z_OK) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  compressed_data.resize(compressed_data_size);
  std::move(callback).Run(std::move(compressed_data));
}

void Gzipper::Inflate(mojo_base::BigBuffer data,
                      uint64_t max_uncompressed_size,
                      InflateCallback callback) {
  uLongf uncompressed_size = static_cast<uLongf>(max_uncompressed_size);
  std::vector<uint8_t> output(max_uncompressed_size);
  if (zlib_internal::UncompressHelper(zlib_internal::ZRAW, output.data(),
                                      &uncompressed_size, data.data(),
                                      data.size()) != Z_OK) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  output.resize(uncompressed_size);
  std::move(callback).Run(std::move(output));
}

void Gzipper::Compress(mojo_base::BigBuffer data, CompressCallback callback) {
  // mojo_base::BigBuffer does not support resizing the backing storage. Output
  // the result into a std::string and copy its contents into a BigBuffer.
  std::string output;
  if (!compression::GzipCompress(data, &output)) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::move(callback).Run(StringToBuffer(output));
}

void Gzipper::Uncompress(mojo_base::BigBuffer compressed_data,
                         UncompressCallback callback) {
  mojo_base::BigBuffer output(
      compression::GetUncompressedSize(compressed_data));
  if (!compression::GzipUncompress(compressed_data, output)) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  std::move(callback).Run(std::move(output));
}

}  // namespace data_decoder
