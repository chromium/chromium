// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/linux/scoped_v4l2_device_fd.h"

namespace media {

ScopedV4L2DeviceFD::ScopedV4L2DeviceFD(V4L2CaptureDevice* v4l2)
    : device_fd_(kInvalidId), v4l2_(v4l2) {}

ScopedV4L2DeviceFD::ScopedV4L2DeviceFD(V4L2CaptureDevice* v4l2, int device_fd)
    : device_fd_(device_fd), v4l2_(v4l2) {}

ScopedV4L2DeviceFD::~ScopedV4L2DeviceFD() {
  if (is_valid())
    reset();
}

int ScopedV4L2DeviceFD::get() const {
  return device_fd_;
}

void ScopedV4L2DeviceFD::reset(int fd /*= kInvalidId*/) {
  if (is_valid())
    v4l2_->close(device_fd_);
  device_fd_ = fd;
}

bool ScopedV4L2DeviceFD::is_valid() const {
  return device_fd_ != kInvalidId;
}

}  // namespace media
