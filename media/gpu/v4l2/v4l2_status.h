// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_V4L2_STATUS_H_
#define MEDIA_GPU_V4L2_V4L2_STATUS_H_

#include "media/base/status.h"

namespace media {

enum class V4L2StatusCodes : StatusCodeType {
  kOk = 0,
  kNoDevice = 1,
  kFailedToStopStreamQueue = 2,
  kNoProfile = 3,
  kMaxDecoderInstanceCount = 4,
  kNoDriverSupportForFourcc = 5,
  kFailedFileCapabilitiesCheck = 6,
  kFailedResourceAllocation = 7,
  kBadFormat = 8,
  kFailedToStartStreamQueue = 9,
  kFailedToDestroyQueueBuffers = 10,
};

struct V4L2StatusTraits {
  using Codes = V4L2StatusCodes;
  static constexpr StatusGroupType Group() { return "V4L2StatusCode"; }
};
using V4L2Status = TypedStatus<V4L2StatusTraits>;

}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_STATUS_H_
