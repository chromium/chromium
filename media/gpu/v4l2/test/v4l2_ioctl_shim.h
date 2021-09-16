// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_TEST_V4L2_IOCTL_SHIM_H_
#define MEDIA_GPU_V4L2_TEST_V4L2_IOCTL_SHIM_H_

#include <linux/videodev2.h>

#include "base/files/memory_mapped_file.h"
#include "media/filters/vp9_parser.h"

namespace media {

namespace v4l2_test {

// V4L2IoctlShim is a shallow wrapper which wraps V4L2 ioctl requests
// with error checking and maintains the lifetime of a file descriptor
// for decode/media device.
class V4L2IoctlShim {
 public:
  V4L2IoctlShim();
  V4L2IoctlShim(const V4L2IoctlShim&) = delete;
  V4L2IoctlShim& operator=(const V4L2IoctlShim&) = delete;
  ~V4L2IoctlShim();

  // Verifies |v4l_fd| supports |compressed_format| for OUTPUT queues
  // and |uncompressed_format| for CAPTURE queues, respectively.
  bool VerifyCapabilities(uint32_t compressed_format,
                          uint32_t uncompressed_format) const
      WARN_UNUSED_RESULT;

 private:
  // Queries |v4l_fd| to see if it can use the specified |fourcc| format
  // for the given buffer |type|.
  bool QueryFormat(enum v4l2_buf_type type,
                   uint32_t fourcc) const WARN_UNUSED_RESULT;

  // Uses a specialized function template to execute V4L2 ioctl request
  // for |request_code| and returns output in |argp| for specific data type T.
  // If a specialized version is not found, then general template function
  // will be executed to report an error.
  template <typename T>
  bool Ioctl(int request_code, T* argp) const WARN_UNUSED_RESULT;

  const base::File ioctl_fd_;
};

}  // namespace v4l2_test
}  // namespace media

#endif  // MEDIA_GPU_V4L2_TEST_V4L2_IOCTL_SHIM_H_