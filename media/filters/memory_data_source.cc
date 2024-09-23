// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/memory_data_source.h"

#include <algorithm>

#include "base/check.h"
#include "base/functional/callback.h"

namespace media {

MemoryDataSource::MemoryDataSource(std::string data)
    : data_string_(std::move(data)),
      data_(reinterpret_cast<const uint8_t*>(data_string_.data())),
      size_(data_string_.size()) {}

MemoryDataSource::MemoryDataSource(const uint8_t* data, size_t size)
    : data_(data), size_(size) {}

MemoryDataSource::~MemoryDataSource() = default;

void MemoryDataSource::Read(int64_t position,
                            int size,
                            uint8_t* data,
                            DataSource::ReadCB read_cb) {
  DCHECK(read_cb);

  if (is_stopped_ || size < 0 || position < 0 ||
      static_cast<size_t>(position) > size_) {
    std::move(read_cb).Run(kReadError);
    return;
  }

  // Cap size within bounds.
  size_t clamped_size = std::min(static_cast<size_t>(size),
                                 size_ - static_cast<size_t>(position));

  if (clamped_size > 0) {
    DCHECK(data);
    memcpy(data, data_ + base::checked_cast<size_t>(position), clamped_size);
  }

  std::move(read_cb).Run(clamped_size);
}

void MemoryDataSource::Stop() {
  is_stopped_ = true;
}

void MemoryDataSource::Abort() {}

bool MemoryDataSource::GetSize(int64_t* size_out) {
  *size_out = size_;
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
