// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/string_data_source.h"

#include <algorithm>

#include "base/notreached.h"

namespace mojo {

StringDataSource::StringDataSource(base::span<const char> data,
                                   AsyncWritingMode mode) {
  switch (mode) {
    case AsyncWritingMode::STRING_MAY_BE_INVALIDATED_BEFORE_COMPLETION:
      data_ = std::string(data.data(), data.size());
      data_view_ = base::span(data_);
      break;
    case AsyncWritingMode::STRING_STAYS_VALID_UNTIL_COMPLETION:
      data_view_ = data;
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
  CHECK(offset <= data_view_.size());
  size_t copyable_size = std::min(
      base::checked_cast<size_t>(data_view_.size() - offset), buffer.size());
  buffer.first(copyable_size)
      .copy_from_nonoverlapping(data_view_.subspan(
          base::checked_cast<size_t>(offset), copyable_size));
  return ReadResult{.bytes_read = copyable_size, .result = MOJO_RESULT_OK};
}

}  // namespace mojo
