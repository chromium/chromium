// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/video_capture_buffer_handle.h"

#include <ostream>

#include "base/notreached.h"

namespace media {

NullHandle::NullHandle() = default;

NullHandle::~NullHandle() = default;

size_t NullHandle::mapped_size() const {
  NOTREACHED() << "Unsupported operation";
}

uint8_t* NullHandle::data() const {
  NOTREACHED() << "Unsupported operation";
}

const uint8_t* NullHandle::const_data() const {
  NOTREACHED() << "Unsupported operation";
}

}  // namespace media
