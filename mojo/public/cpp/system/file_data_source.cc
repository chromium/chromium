// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/file_data_source.h"

#include <algorithm>
#include <limits>

#include "base/numerics/safe_conversions.h"

namespace mojo {

namespace {

uint64_t CalculateEndOffset(base::File* file, MojoResult* result) {
  if (!file->IsValid())
    return 0u;
  int64_t length = file->GetLength();
  if (length < 0) {
    *result =
        FileDataSource::ConvertFileErrorToMojoResult(file->GetLastFileError());
    return 0u;
  }
  return length;
}

}  // namespace

// static
MojoResult FileDataSource::ConvertFileErrorToMojoResult(
    base::File::Error error) {
  switch (error) {
    case base::File::FILE_OK:
      return MOJO_RESULT_OK;
    case base::File::FILE_ERROR_NOT_FOUND:
      return MOJO_RESULT_NOT_FOUND;
    case base::File::FILE_ERROR_SECURITY:
    case base::File::FILE_ERROR_ACCESS_DENIED:
      return MOJO_RESULT_PERMISSION_DENIED;
    case base::File::FILE_ERROR_TOO_MANY_OPENED:
    case base::File::FILE_ERROR_NO_MEMORY:
      return MOJO_RESULT_RESOURCE_EXHAUSTED;
    case base::File::FILE_ERROR_ABORT:
      return MOJO_RESULT_ABORTED;
    default:
      return MOJO_RESULT_UNKNOWN;
  }
}

FileDataSource::FileDataSource(base::File file)
    : file_(std::move(file)),
      error_(ConvertFileErrorToMojoResult(file_.error_details())),
      start_offset_(0u),
      end_offset_(CalculateEndOffset(&file_, &error_)) {}

FileDataSource::~FileDataSource() = default;

void FileDataSource::SetRange(uint64_t start, uint64_t end) {
  if (start > end) {
    start_offset_ = 0;
    end_offset_ = 0;
    if (error_ == MOJO_RESULT_OK)
      error_ = MOJO_RESULT_INVALID_ARGUMENT;
  } else {
    start_offset_ = start;
    end_offset_ = end;
  }
}

uint64_t FileDataSource::GetLength() const {
  return end_offset_ - start_offset_;
}

DataPipeProducer::DataSource::ReadResult FileDataSource::Read(
    uint64_t offset,
    base::span<char> buffer) {
  ReadResult result;
  if (error_ != MOJO_RESULT_OK)
    result.result = error_;
  else if (GetLength() < offset)
    result.result = MOJO_RESULT_INVALID_ARGUMENT;

  uint64_t readable_size = GetLength() - offset;
  uint64_t read_size =
      std::min(static_cast<uint64_t>(std::numeric_limits<int>::max()),
               std::min(static_cast<uint64_t>(buffer.size()), readable_size));
  // |read_offset| should not overflow if 'GetLength() < offset' is true.
  // Otherwise, MOJO_RESULT_INVALID_ARGUMENT should be already set.
  uint64_t read_offset = start_offset_ + offset;
  if (read_offset > std::numeric_limits<int64_t>::max())
    result.result = MOJO_RESULT_INVALID_ARGUMENT;

  if (result.result != MOJO_RESULT_OK)
    return result;

  std::optional<size_t> bytes_read =
      file_.Read(static_cast<int64_t>(read_offset),
                 base::as_writable_bytes(buffer).first(
                     base::checked_cast<size_t>(read_size)));

  if (!bytes_read.has_value()) {
    result.bytes_read = 0;
    result.result = ConvertFileErrorToMojoResult(file_.GetLastFileError());
  } else {
    result.bytes_read = bytes_read.value();
  }
  return result;
}

}  // namespace mojo
