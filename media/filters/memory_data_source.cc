// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/memory_data_source.h"

#include <algorithm>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/functional/callback.h"

namespace media {

MemoryDataSource::MemoryDataSource(std::string data)
    : data_string_(std::move(data)), data_(base::as_byte_span(data_string_)) {}

MemoryDataSource::MemoryDataSource(base::span<const uint8_t> data)
    : data_(data) {}

MemoryDataSource::~MemoryDataSource() = default;

void MemoryDataSource::Read(int64_t position,
                            base::span<uint8_t> data,
                            DataSource::ReadCB read_cb) {
  DCHECK(read_cb);

  if (is_stopped_ || position < 0 ||
      static_cast<size_t>(position) > data_.size()) {
    std::move(read_cb).Run(kReadError);
    return;
  }
  const auto source = data_.subspan(static_cast<size_t>(position));

  // Cap size within bounds.
  const size_t clamped_size = std::min(data.size(), source.size());

  if (clamped_size > 0) {
    data.copy_prefix_from(source.first(clamped_size));
  }

  std::move(read_cb).Run(clamped_size);
}

void MemoryDataSource::Stop() {
  is_stopped_ = true;
}

void MemoryDataSource::Abort() {}

bool MemoryDataSource::GetSize(int64_t* size_out) {
  *size_out = data_.size();
  return true;
}

bool MemoryDataSource::IsStreaming() {
  return false;
}

void MemoryDataSource::SetBitrate(int bitrate) {}

bool MemoryDataSource::PassedTimingAllowOriginCheck() {
  // There are no HTTP responses, so this can safely return true.
  return true;
}

bool MemoryDataSource::WouldTaintOrigin() {
  // There are no HTTP responses, so this can safely return false.
  return false;
}

}  // namespace media
