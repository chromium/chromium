// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/mappable_shared_image_utils.h"

#include "base/functional/callback_helpers.h"
#include "media/capture/video/video_capture_gpu_channel_host.h"

namespace media {

VideoCaptureDevice::Client::ReserveResult AllocateNV12SharedImage(
    VideoCaptureDevice::Client* capture_client,
    const gfx::Size& buffer_size,
    scoped_refptr<gpu::ClientSharedImage>* out_shared_image,
    VideoCaptureDevice::Client::Buffer* out_capture_buffer) {
  CHECK(out_shared_image);
  CHECK(out_capture_buffer);

  // When GpuMemoryBuffer is used, the frame data is opaque to the CPU for most
  // of the time.  Currently the only supported underlying format is NV12.
  constexpr VideoPixelFormat kOpaqueVideoFormat = PIXEL_FORMAT_NV12;
  const int arbitrary_frame_feedback_id = 0;
  const auto reserve_result = capture_client->ReserveOutputBuffer(
      buffer_size, kOpaqueVideoFormat, arbitrary_frame_feedback_id,
      out_capture_buffer, /*require_new_buffer_id=*/nullptr,
      /*retire_old_buffer_id=*/nullptr);
  if (reserve_result != VideoCaptureDevice::Client::ReserveResult::kSucceeded) {
    return reserve_result;
  }

  auto sii =
      VideoCaptureGpuChannelHost::GetInstance().GetSharedImageInterface();
  if (!sii) {
    LOG(ERROR) << "Failed to get SharedImageInterface.";
    return VideoCaptureDevice::Client::ReserveResult::kAllocationFailed;
  }

  // Setting some default usage in order to get a mappable shared image for
  // windows. Note that the usage should be part of supported usages in
  // D3DImageBackingFactory.
  const auto si_usage = gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;
  auto multiplanar_si_format = viz::MultiPlaneFormat::kNV12;
#if BUILDFLAG(IS_OZONE)
  multiplanar_si_format.SetPrefersExternalSampler();
#endif
  *out_shared_image = sii->CreateSharedImage(
      {multiplanar_si_format, buffer_size, gfx::ColorSpace(),
       gpu::SharedImageUsageSet(si_usage), "AllocatedNV12SharedImage"},
      gpu::kNullSurfaceHandle,
      gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE,
      out_capture_buffer->handle_provider->GetGpuMemoryBufferHandle());
  if (!*out_shared_image) {
    LOG(ERROR) << "Failed to create a mappable shared image.";
    return VideoCaptureDevice::Client::ReserveResult::kAllocationFailed;
  }

  return reserve_result;
}

}  // namespace media
