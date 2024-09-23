// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/gpu_memory_buffer_utils.h"

#include "base/functional/callback_helpers.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace media {

ScopedNV12GpuMemoryBufferMapping::ScopedNV12GpuMemoryBufferMapping(
    std::unique_ptr<gfx::GpuMemoryBuffer> gmb)
    : gmb_(std::move(gmb)) {
  gmb_->Map();
}

ScopedNV12GpuMemoryBufferMapping::~ScopedNV12GpuMemoryBufferMapping() {
  gmb_->Unmap();
}

uint8_t* ScopedNV12GpuMemoryBufferMapping::y_plane() {
  return static_cast<uint8_t*>(gmb_->memory(0));
}

uint8_t* ScopedNV12GpuMemoryBufferMapping::uv_plane() {
  return static_cast<uint8_t*>(gmb_->memory(1));
}

size_t ScopedNV12GpuMemoryBufferMapping::y_stride() {
  return gmb_->stride(0);
}

size_t ScopedNV12GpuMemoryBufferMapping::uv_stride() {
  return gmb_->stride(1);
}

VideoCaptureDevice::Client::ReserveResult AllocateNV12GpuMemoryBuffer(
    VideoCaptureDevice::Client* capture_client,
    const gfx::Size& buffer_size,
    gpu::GpuMemoryBufferSupport* gmb_support,
    std::unique_ptr<gfx::GpuMemoryBuffer>* out_gpu_memory_buffer,
    VideoCaptureDevice::Client::Buffer* out_capture_buffer) {
  CHECK(out_gpu_memory_buffer);
  CHECK(out_capture_buffer);

  // When GpuMemoryBuffer is used, the frame data is opaque to the CPU for most
  // of the time.  Currently the only supported underlying format is NV12.
  constexpr VideoPixelFormat kOpaqueVideoFormat = PIXEL_FORMAT_NV12;
  constexpr gfx::BufferFormat kOpaqueGfxFormat =
      gfx::BufferFormat::YUV_420_BIPLANAR;

  const int arbitrary_frame_feedback_id = 0;
  const auto reserve_result = capture_client->ReserveOutputBuffer(
      buffer_size, kOpaqueVideoFormat, arbitrary_frame_feedback_id,
      out_capture_buffer, /*require_new_buffer_id=*/nullptr,
      /*retire_old_buffer_id=*/nullptr);
  if (reserve_result != VideoCaptureDevice::Client::ReserveResult::kSucceeded) {
    return reserve_result;
  }

  *out_gpu_memory_buffer = gmb_support->CreateGpuMemoryBufferImplFromHandle(
      out_capture_buffer->handle_provider->GetGpuMemoryBufferHandle(),
      buffer_size, kOpaqueGfxFormat,
      gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE,
      base::NullCallback());
  return reserve_result;
}

}  // namespace media
