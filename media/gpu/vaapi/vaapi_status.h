// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_STATUS_H_
#define MEDIA_GPU_VAAPI_VAAPI_STATUS_H_

#include "media/base/status.h"

namespace media {

enum class VaapiStatusCode : StatusCodeType {
  kOk = 0,
  kBadContext = 1,
  kNoBuffer = 2,
  kNoBufferHandle = 3,
  kNoPixmap = 4,
  kNoImage = 5,
  kNoSurface = 6,
  kFailedToInitializeImage = 7,
  kFailedToBindTexture = 8,
  kFailedToBindImage = 9,
  kUnsupportedFormat = 10,
  kFailedToExportImage = 11,
  kBadImageSize = 12,
  kNoTexture = 13,
  kUnsupportedProfile = 14,
};

struct VaapiStatusTraits {
  using Codes = VaapiStatusCode;
  static constexpr StatusGroupType Group() { return "VaapiStatusCode"; }
};
using VaapiStatus = TypedStatus<VaapiStatusTraits>;

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_STATUS_H_
