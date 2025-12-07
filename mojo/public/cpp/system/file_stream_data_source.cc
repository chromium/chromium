// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/file_stream_data_source.h"

#include <algorithm>
#include <limits>

#include "base/numerics/safe_conversions.h"
#include "mojo/public/cpp/system/file_data_source.h"

namespace mojo {

FileStreamDataSource::FileStreamDataSource(base::File file, int64_t length)
    : file_(std::move(file)), length_(length) {}

FileStreamDataSource::~FileStreamDataSource() = default;

uint64_t FileStreamDataSource::GetLength() const {
  return length_;
}

DataPipeProducer::DataSource::ReadResult FileStreamDataSource::Read(
    uint64_t offset,
    base::span<char> buffer) {
  ReadResult result;
  if (offset != current_offset_) {
    result.result = MOJO_RESULT_INVALID_ARGUMENT;
    return result;
  }

  uint64_t readable_size = length_ - offset;
  uint64_t read_size =
      std::min(uint64_t{std::numeric_limits<int>::max()},
               std::min(uint64_t{buffer.size()}, readable_size));

  std::optional<size_t> bytes_read =
      file_.ReadAtCurrentPos(base::as_writable_bytes(buffer).first(
          base::checked_cast<size_t>(read_size)));

  if (!bytes_read.has_value()) {
    result.bytes_read = 0;
    result.result =
        FileDataSource::ConvertFileErrorToMojoResult(file_.GetLastFileError());
  } else {
    result.bytes_read = bytes_read.value();
    current_offset_ += bytes_read.value();
  }
  return result;
}

}  // namespace mojo
