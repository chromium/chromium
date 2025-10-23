// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/in_memory_url_protocol.h"

#include "media/ffmpeg/ffmpeg_common.h"

namespace media {

InMemoryUrlProtocol::InMemoryUrlProtocol(base::span<const uint8_t> data,
                                         bool streaming)
    : data_(data), position_(0), streaming_(streaming) {}

InMemoryUrlProtocol::~InMemoryUrlProtocol() = default;

int InMemoryUrlProtocol::Read(base::span<uint8_t> data) {
  if (data.empty()) {
    return 0;
  }
  if (position_ >= base::checked_cast<int64_t>(data_.size())) {
    return AVERROR_EOF;
  }

  const auto source = data_.subspan(base::checked_cast<size_t>(position_));
  if (data.size() > source.size()) {
    data = data.first(source.size());
  }
  if (!data.empty()) {
    data.copy_from(source.first(data.size()));
    position_ += data.size();
  }
  return data.size();
}

bool InMemoryUrlProtocol::GetPosition(int64_t* position_out) {
  if (!position_out)
    return false;

  *position_out = position_;
  return true;
}

bool InMemoryUrlProtocol::SetPosition(int64_t position) {
  if (position < 0 || position > base::checked_cast<int64_t>(data_.size())) {
    return false;
  }
  position_ = position;
  return true;
}

bool InMemoryUrlProtocol::GetSize(int64_t* size_out) {
  if (!size_out)
    return false;

  *size_out = data_.size();
  return true;
}

bool InMemoryUrlProtocol::IsStreaming() {
  return streaming_;
}

}  // namespace media
