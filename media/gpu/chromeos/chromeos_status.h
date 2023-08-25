// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_CHROMEOS_STATUS_H_
#define MEDIA_GPU_CHROMEOS_CHROMEOS_STATUS_H_

#include "media/base/status.h"

namespace media {

enum class ChromeosStatusCode : StatusCodeType {
  kOk = 0,
  kFourccUnknownFormat = 1,
  kProtectedContentUnsupported = 2,
  kFailedToCreateVideoFrame = 3,
  kFailedToGetFrameLayout = 4,
  kNoDecoderOutputFormatCandidates = 5,
  kFailedToCreateImageProcessor = 6,
  kResetRequired = 7,
  kInvalidLayoutSize = 8,
  kFailedToChangeResolution = 9,
  kInsufficientFramePoolSize = 10,
  kUnableToAllocateSecureBuffer = 11,
  kSecureBufferPoolEmpty = 12,
};

struct CroStatusTraits {
  using Codes = ChromeosStatusCode;
  static constexpr StatusGroupType Group() { return "ChromeosStatusCode"; }
};
using CroStatus = TypedStatus<CroStatusTraits>;

}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_CHROMEOS_STATUS_H_
