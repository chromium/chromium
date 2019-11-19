// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/string_data_source.h"

#include <algorithm>

namespace mojo {

StringDataSource::StringDataSource(base::StringPiece data,
                                   AsyncWritingMode mode) {
  switch (mode) {
    case AsyncWritingMode::STRING_MAY_BE_INVALIDATED_BEFORE_COMPLETION:
      data_ = std::string(data.data(), data.size());
      data_view_ = base::span<const char>(data_.data(), data_.size());
      break;
    case AsyncWritingMode::STRING_STAYS_VALID_UNTIL_COMPLETION:
      data_view_ = base::span<const char>(data.data(), data.size());
      break;
  }
}

StringDataSource::~StringDataSource() = default;

uint64_t StringDataSource::GetLength() const {
  return data_view_.size();
}

DataPipeProducer::DataSource::ReadResult StringDataSource::Read(
    uint64_t offset,
    base::span<char> buffer) {
  ReadResult result;
  if (offset <= data_view_.size()) {
    size_t readable_size = data_view_.size() - offset;
    size_t writable_size = buffer.size();
    size_t copyable_size = std::min(readable_size, writable_size);
    for (size_t copied_size = 0; copied_size < copyable_size; ++copied_size)
      buffer[copied_size] = data_view_[offset + copied_size];
    result.bytes_read = copyable_size;
  } else {
    NOTREACHED();
    result.result = MOJO_RESULT_OUT_OF_RANGE;
  }
  return result;
}

}  // namespace mojo
