// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_V4L2_UTILS_H_
#define MEDIA_GPU_V4L2_V4L2_UTILS_H_

#include <string>

#include <linux/videodev2.h>

namespace media {

// Returns a human readable description of |memory|.
const char* V4L2MemoryToString(v4l2_memory memory);

// Returns a human readable description of |format|.
std::string V4L2FormatToString(const struct v4l2_format& format);

// Returns a human readable description of |buffer|
std::string V4L2BufferToString(const struct v4l2_buffer& buffer);
}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_UTILS_H_
