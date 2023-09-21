// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_LINUX_V4L2_CAPTURE_DEVICE_H_
#define MEDIA_CAPTURE_VIDEO_LINUX_V4L2_CAPTURE_DEVICE_H_

#include <fcntl.h>
#include <poll.h>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "media/capture/capture_export.h"

namespace media {

// Interface for abstracting out the V4L2 API. This allows using a mock or fake
// implementation in testing.
class CAPTURE_EXPORT V4L2CaptureDevice
    : public base::RefCountedThreadSafe<V4L2CaptureDevice> {
 public:
  virtual int open(const char* device_name, int flags) = 0;
  virtual int close(int fd) = 0;
  virtual int ioctl(int fd, int request, void* argp) = 0;
  virtual void* mmap(void* start,
                     size_t length,
                     int prot,
                     int flags,
                     int fd,
                     off_t offset) = 0;

  virtual int munmap(void* start, size_t length) = 0;
  virtual int poll(struct pollfd* ufds, unsigned int nfds, int timeout) = 0;

 protected:
  virtual ~V4L2CaptureDevice() {}

 private:
  friend class base::RefCountedThreadSafe<V4L2CaptureDevice>;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_LINUX_V4L2_CAPTURE_DEVICE_H_
