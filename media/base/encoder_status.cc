// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/encoder_status.h"

#include "base/notreached.h"

namespace media {

// static
std::string EncoderStatusTraits::ReadableCodeName(Codes code) {
  switch (code) {
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
    case EncoderStatus::Codes::kOutOfPlatformEncoders:
      return "The system ran out of platform encoders.";
    case EncoderStatus::Codes::kBadReferenceBuffer:
      return "Invalid reference buffer index is specified.";
    case EncoderStatus::Codes::kGPUCommandBufferNotAvailable:
      return "Provided GPU command buffer is null.";
    case EncoderStatus::Codes::kMissingGLDisplay:
      return "Failed to get the EGL display.";
    case EncoderStatus::Codes::kGLSurfaceInitializationFailed:
      return "Failed to initialize the GL surface.";
    case EncoderStatus::Codes::kGLContextCreationFailed:
      return "Failed to create the GL context.";
    case EncoderStatus::Codes::kGLMakeCurrentFailed:
      return "Failed to make the GL context current.";
    case EncoderStatus::Codes::kUnsupportedGLFeature:
      return "Required GL features or extensions are missing.";
    case EncoderStatus::Codes::kGLInitializationError:
      return "A GL error occurred during initialization.";
    case EncoderStatus::Codes::kD3D12CommandAllocatorResetFailed:
      return "D3D12: Failed to reset command allocator.";
    case EncoderStatus::Codes::kD3D12CommandListResetFailed:
      return "D3D12: Failed to reset command list.";
    case EncoderStatus::Codes::kD3D12CommandListCloseFailed:
      return "D3D12: Failed to close command list.";
    case EncoderStatus::Codes::kD3D12FenceWaitFailed:
      return "D3D12: Failed to wait for fence.";
    case EncoderStatus::Codes::kD3D12ResourceMapFailed:
      return "D3D12: Failed to map resource.";
    case EncoderStatus::Codes::kD3D12CreateCommittedResourceFailed:
      return "D3D12: Failed to create committed resource.";
    case EncoderStatus::Codes::kD3D12OpenSharedHandleFailed:
      return "D3D12: Failed to open shared handle.";
    case EncoderStatus::Codes::kD3D12InvalidVideoEncoderMetadata:
      return "D3D12: Invalid video encoder metadata.";
    case EncoderStatus::Codes::kD3D12CheckFeatureSupportFailed:
      return "D3D12: Failed to check feature support.";
    case EncoderStatus::Codes::kD3D12VideoProcessorProcessFramesFailed:
      return "D3D12: Video processor process frames failed.";
    case EncoderStatus::Codes::kD3D12CreateFenceFailed:
      return "D3D12: Failed to create fence.";
    case EncoderStatus::Codes::kD3D12CreateCopyQueueFailed:
      return "D3D12: Failed to create copy queue.";
    case EncoderStatus::Codes::kSharedImageResolveFailed:
      return "Failed to resolve SharedImage.";
    case EncoderStatus::Codes::kOk:
      NOTREACHED();
  }
}

}  // namespace media
