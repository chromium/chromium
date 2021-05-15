// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/gzipper.h"

#include "base/containers/span.h"
#include "base/strings/string_piece_forward.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "third_party/zlib/google/compression_utils.h"

namespace data_decoder {

namespace {

mojo_base::BigBuffer StringToBuffer(const std::string& string) {
  return base::as_bytes(base::make_span(string));
}

}  // namespace

Gzipper::Gzipper() = default;
Gzipper::~Gzipper() = default;

void Gzipper::Compress(mojo_base::BigBuffer data, CompressCallback callback) {
  // mojo_base::BigBuffer does not support resizing the backing storage. Output
  // the result into a std::string and copy its contents into a BigBuffer.
  std::string output;
  if (!compression::GzipCompress(data, &output)) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  std::move(callback).Run(StringToBuffer(output));
}

void Gzipper::Uncompress(mojo_base::BigBuffer compressed_data,
                         UncompressCallback callback) {
  mojo_base::BigBuffer output(
      compression::GetUncompressedSize(compressed_data));
  if (!compression::GzipUncompress(compressed_data, output)) {
    std::move(callback).Run(absl::nullopt);
    return;
  }
  std::move(callback).Run(std::move(output));
}

}  // namespace data_decoder
