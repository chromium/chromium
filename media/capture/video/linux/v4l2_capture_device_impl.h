// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_LINUX_V4L2_CAPTURE_DEVICE_IMPL_H_
#define MEDIA_CAPTURE_VIDEO_LINUX_V4L2_CAPTURE_DEVICE_IMPL_H_

#include <fcntl.h>
#include <poll.h>

#include "media/capture/capture_export.h"
#include "media/capture/video/linux/v4l2_capture_device.h"

namespace media {

// Implementation of V4L2CaptureDevice interface that delegates to the actual
// V4L2 APIs.
class CAPTURE_EXPORT V4L2CaptureDeviceImpl : public V4L2CaptureDevice {
 public:
  int open(const char* device_name, int flags) override;
  int close(int fd) override;
  int ioctl(int fd, int request, void* argp) override;
  void* mmap(void* start,
             size_t length,
             int prot,
             int flags,
             int fd,
             off_t offset) override;

  int munmap(void* start, size_t length) override;
  int poll(struct pollfd* ufds, unsigned int nfds, int timeout) override;

 private:
  ~V4L2CaptureDeviceImpl() override;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_LINUX_V4L2_CAPTURE_DEVICE_IMPL_H_
