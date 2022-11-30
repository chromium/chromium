// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/linux/v4l2_capture_device_impl.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <unistd.h>

namespace media {

V4L2CaptureDeviceImpl::~V4L2CaptureDeviceImpl() = default;

int V4L2CaptureDeviceImpl::open(const char* device_name, int flags) {
  return ::open(device_name, flags);
}

int V4L2CaptureDeviceImpl::close(int fd) {
  return ::close(fd);
}

int V4L2CaptureDeviceImpl::ioctl(int fd, int request, void* argp) {
  return ::ioctl(fd, request, argp);
}

void* V4L2CaptureDeviceImpl::mmap(void* start,
                                  size_t length,
                                  int prot,
                                  int flags,
                                  int fd,
                                  off_t offset) {
  return ::mmap(start, length, prot, flags, fd, offset);
}

int V4L2CaptureDeviceImpl::munmap(void* start, size_t length) {
  return ::munmap(start, length);
}

int V4L2CaptureDeviceImpl::poll(struct pollfd* ufds,
                                unsigned int nfds,
                                int timeout) {
  return ::poll(ufds, nfds, timeout);
}

}  // namespace media
