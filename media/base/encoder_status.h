// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ENCODER_STATUS_H_
#define MEDIA_BASE_ENCODER_STATUS_H_

#include "media/base/status.h"

namespace media {

struct MEDIA_EXPORT EncoderStatusTraits {
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
    // The system ran out of platform encoders.
    kOutOfPlatformEncoders = 21,
    // The client provided a non-existing reference buffer.
    kBadReferenceBuffer = 22,
    // Provided GPU command buffer is null
    kGPUCommandBufferNotAvailable = 23,
    // Failed to get the EGL display.
    kMissingGLDisplay = 24,
    // Failed to initialize the GL surface.
    kGLSurfaceInitializationFailed = 25,
    // Failed to create the GL context.
    kGLContextCreationFailed = 26,
    // Failed to make the GL context current.
    kGLMakeCurrentFailed = 27,
    // Required GL features or extensions are missing.
    kUnsupportedGLFeature = 28,
    // A GL error occurred during initialization.
    kGLInitializationError = 29,
    // Failure in calling ID3D12CommandAllocator::Reset().
    kD3D12CommandAllocatorResetFailed = 30,
    // Failure in calling ID3D12VideoEncodeCommandList::Reset().
    kD3D12CommandListResetFailed = 31,
    // Failure in calling ID3D12VideoEncodeCommandList::Close().
    kD3D12CommandListCloseFailed = 32,
    // Failure in waiting for a D3D12 fence.
    kD3D12FenceWaitFailed = 33,
    // Failure in calling ID3D12Resource::Map().
    kD3D12ResourceMapFailed = 34,
    // Failure in calling ID3D12Device::CreateCommittedResource().
    kD3D12CreateCommittedResourceFailed = 35,
    // Failure in calling ID3D12Device::OpenSharedHandle().
    kD3D12OpenSharedHandleFailed = 36,
    // Failure in validating D3D12 video encoder metadata.
    kD3D12InvalidVideoEncoderMetadata = 37,
    // Failure in calling ID3D12VideoDevice::CheckFeatureSupport().
    kD3D12CheckFeatureSupportFailed = 38,
    // Failure in calling ID3D12VideoProcessCommandList::ProcessFrames().
    kD3D12VideoProcessorProcessFramesFailed = 39,
    // Failure in calling ID3D12Device::CreateFence().
    kD3D12CreateFenceFailed = 40,
    // Failure in creating D3D12CopyCommandQueueWrapper.
    kD3D12CreateCopyQueueFailed = 41,
    // Failure in resolving a SharedImage.
    kSharedImageResolveFailed = 42,

    kMaxValue = kSharedImageResolveFailed,
  };

  static constexpr StatusGroupType Group() { return "EncoderStatus"; }
  static std::string ReadableCodeName(Codes code);
};

using EncoderStatus = TypedStatus<EncoderStatusTraits>;

}  // namespace media

#endif  // MEDIA_BASE_ENCODER_STATUS_H_
