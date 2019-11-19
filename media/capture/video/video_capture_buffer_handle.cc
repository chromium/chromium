// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/video_capture_buffer_handle.h"

#include "base/logging.h"

namespace media {

NullHandle::NullHandle() = default;

NullHandle::~NullHandle() = default;

size_t NullHandle::mapped_size() const {
  NOTREACHED() << "Unsupported operation";
  return 0;
}

uint8_t* NullHandle::data() const {
  NOTREACHED() << "Unsupported operation";
  return nullptr;
}

const uint8_t* NullHandle::const_data() const {
  NOTREACHED() << "Unsupported operation";
  return nullptr;
}

}  // namespace media
