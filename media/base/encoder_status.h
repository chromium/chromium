// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ENCODER_STATUS_H_
#define MEDIA_BASE_ENCODER_STATUS_H_

#include "media/base/status.h"

namespace media {

struct EncoderStatusTraits {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. Please keep the consistency with
  // EncoderStatus in tools/metrics/histograms/enums.xml.
  enum class Codes : StatusCodeType {
    // No failure happens.
    kOk = 0,
    // The encoder initialization has never completed successfully.
    kEncoderInitializeNeverCompleted = 1,
    // The encoder has been initialized more than once.
    kEncoderInitializeTwice = 2,
    // Failure in encoding process.
    kEncoderFailedEncode = 3,
    // The given codec profile is not supported by the encoder.
    kEncoderUnsupportedProfile = 4,
    // The given codec is not supported by the encoder.
    kEncoderUnsupportedCodec = 5,
    // The given encoder configuration is not supported by the encoder.
    kEncoderUnsupportedConfig = 6,
    // Failure in the encoder initialization.
    kEncoderInitializationError = 7,
    // Failure in flushing process.
    kEncoderFailedFlush = 8,
    // Failure in mojo connection.
    kEncoderMojoConnectionError = 9,
    // The format of the given frame is not supported by the encoder.
    kUnsupportedFrameFormat = 10,
    // Failure in scaling the given frame.
    kScalingError = 11,
    // Failure in converting the format of the given frame.
    kFormatConversionError = 12,
    // Failure due to a hardware driver.
    kEncoderHardwareDriverError = 13,
    // The encoder is in the illegal state.
    kEncoderIllegalState = 14,
    // The system API (e.g. Linux system call) fails.
    kSystemAPICallError = 15,
    // The given frame is invalid, e.g., storage type and visible rectangle.
    kInvalidInputFrame = 16,
    // The given output buffer or its id  is invalid.
    kInvalidOutputBuffer = 17,
    // Failure in converting H264/HEVC AnnexB to H264/HEVC bitstream.
    kBitstreamConversionError = 18,
    // Failure in allocating a buffer.
    kOutOfMemoryError = 19,
    // No hardware encoder is available.
    kEncoderAccelerationSupportMissing = 20,
    kMaxValue = kEncoderAccelerationSupportMissing,
  };
  static constexpr StatusGroupType Group() { return "EncoderStatus"; }
};

using EncoderStatus = TypedStatus<EncoderStatusTraits>;

MEDIA_EXPORT const char* EncoderStatusCodeToString(
    const EncoderStatus& error_status);
}  // namespace media

#endif  // MEDIA_BASE_ENCODER_STATUS_H_
