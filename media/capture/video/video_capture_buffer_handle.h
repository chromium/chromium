// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_BUFFER_HANDLE_H_
#define MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_BUFFER_HANDLE_H_

#include <stddef.h>
#include <stdint.h>

#include "media/capture/capture_export.h"

namespace media {

// Abstraction of a pool's buffer data buffer and size for clients.
class CAPTURE_EXPORT VideoCaptureBufferHandle {
 public:
  virtual ~VideoCaptureBufferHandle() {}
  virtual size_t mapped_size() const = 0;
  virtual uint8_t* data() const = 0;
  virtual const uint8_t* const_data() const = 0;
};

// This is a null implementation used by GpuMemoryBufferTracker, as
// GpuMemoryBuffer should not be accessed on the CPU outside of the Chrome OS
// VideoCaptureDevice implementation.
class CAPTURE_EXPORT NullHandle final : public VideoCaptureBufferHandle {
 public:
  NullHandle();
  ~NullHandle() override;
  size_t mapped_size() const override;
  uint8_t* data() const override;
  const uint8_t* const_data() const override;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_BUFFER_HANDLE_H_
