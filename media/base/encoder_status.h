// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ENCODER_STATUS_H_
#define MEDIA_BASE_ENCODER_STATUS_H_

#include "media/base/status.h"

namespace media {

struct EncoderStatusTraits {
  enum class Codes : StatusCodeType {
    kOk = 0,
    kEncoderInitializeNeverCompleted = 1,
    kEncoderInitializeTwice = 2,
    kEncoderFailedEncode = 3,
    kEncoderUnsupportedProfile = 4,
    kEncoderUnsupportedCodec = 5,
    kEncoderUnsupportedConfig = 6,
    kEncoderInitializationError = 7,
    kEncoderFailedFlush = 8,
    kEncoderMojoConnectionError = 9,
    kUnsupportedFrameFormat = 10,
    kScalingError = 11,
    kFormatConversionError = 12,
    kEncoderHardwareDriverError = 13,
    kEncoderIllegalState = 14,
    kSystemAPICallError = 15,
    kInvalidInputFrame = 16,
    kInvalidOutputBuffer = 17,
  };
  static constexpr StatusGroupType Group() { return "EncoderStatus"; }
};

using EncoderStatus = TypedStatus<EncoderStatusTraits>;

}  // namespace media

#endif  // MEDIA_BASE_ENCODER_STATUS_H_
