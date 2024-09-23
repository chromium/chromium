// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/encoder_status.h"

namespace media {

const char* EncoderStatusCodeToString(const EncoderStatus& error_status) {
  switch (error_status.code()) {
    case EncoderStatus::Codes::kEncoderInitializeNeverCompleted:
      return "The encoder initialization has never completed successfully.";
    case EncoderStatus::Codes::kEncoderInitializeTwice:
      return "The encoder has been initialized more than once.";
    case EncoderStatus::Codes::kEncoderFailedEncode:
      return "Encoding failed.";
    case EncoderStatus::Codes::kEncoderUnsupportedProfile:
      return "The given codec profile is not supported by the encoder.";
    case EncoderStatus::Codes::kEncoderUnsupportedCodec:
      return "The given codec is not supported by the encoder.";
    case EncoderStatus::Codes::kEncoderUnsupportedConfig:
      return "The given encoder configuration is not supported by the encoder.";
    case EncoderStatus::Codes::kEncoderInitializationError:
      return "Encoder initialization failed.";
    case EncoderStatus::Codes::kEncoderFailedFlush:
      return "Flushing for encoded data failed.";
    case EncoderStatus::Codes::kEncoderMojoConnectionError:
      return "Internal Error.";
    case EncoderStatus::Codes::kUnsupportedFrameFormat:
      return "The format of the given frame is not supported by the encoder.";
    case EncoderStatus::Codes::kScalingError:
      return "Scaling the given frame failed.";
    case EncoderStatus::Codes::kFormatConversionError:
      return "Converting the format of the given frame failed.";
    case EncoderStatus::Codes::kEncoderHardwareDriverError:
      return "Hardware driver failed.";
    case EncoderStatus::Codes::kEncoderIllegalState:
      return "The encoder is in an illegal state.";
    case EncoderStatus::Codes::kSystemAPICallError:
      return "The system API call failed.";
    case EncoderStatus::Codes::kInvalidInputFrame:
      return "Invalid input frame.";
    case EncoderStatus::Codes::kInvalidOutputBuffer:
      return "Internal memory error.";
    case EncoderStatus::Codes::kBitstreamConversionError:
      return "Failure in converting H264/HEVC AnnexB to H264/HEVC bit stream.";
    case EncoderStatus::Codes::kOutOfMemoryError:
      return "Allocating a buffer failed.";
    case EncoderStatus::Codes::kEncoderAccelerationSupportMissing:
      return "No hardware encoder is available.";
    case EncoderStatus::Codes::kOk:
      NOTREACHED_NORETURN();
  }
}

}  // namespace media
