// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_WIN_D3D11_SHARED_IMAGE_BUFFER_TRACKER_H_
#define MEDIA_CAPTURE_VIDEO_WIN_D3D11_SHARED_IMAGE_BUFFER_TRACKER_H_

#include <d3d11.h>
#include <wrl.h>

#include <atomic>
#include <cstdint>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "media/base/win/dxgi_device_manager.h"
#include "media/capture/capture_export.h"
#include "media/capture/video/video_capture_buffer_tracker.h"
#include "media/capture/video/video_capture_gpu_channel_host.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"

namespace gpu {
class ClientSharedImage;
}

namespace media {

// A capture buffer slot backed by an NV12 D3D11 texture that is wrapped in a
// gpu::ClientSharedImage. The producer (the MediaFoundation capture path)
// copies captured frames into the texture through the DXGI handle returned by
// GetGpuMemoryBufferHandle(); consumers receive the shared image through
// GetVideoBufferHandle(). Unlike SharedImageBufferTracker, slots of this
// tracker are reusable across frames with matching format and dimensions,
// similarly to how video decoders reuse picture buffers.
//
// Owned by VideoCaptureBufferPoolImpl, which creates it through
// VideoCaptureBufferTrackerFactoryImpl::CreateTracker() and releases it when
// the pool evicts the slot. On GPU context loss the tracker marks itself
// invalid so the pool stops reusing it; it is released when the pool is full.
class CAPTURE_EXPORT D3D11SharedImageBufferTracker final
    : public VideoCaptureBufferTracker,
      public VideoCaptureGpuContextLostObserver {
 public:
  explicit D3D11SharedImageBufferTracker(
      scoped_refptr<DXGIDeviceManager> dxgi_device_manager);
  ~D3D11SharedImageBufferTracker() override;

  D3D11SharedImageBufferTracker(const D3D11SharedImageBufferTracker&) = delete;
  D3D11SharedImageBufferTracker& operator=(
      const D3D11SharedImageBufferTracker&) = delete;

  // VideoCaptureBufferTracker implementation.
  bool Init(const gfx::Size& dimensions,
            VideoPixelFormat format,
            const mojom::PlaneStridesPtr& strides,
            const gfx::ColorSpace& color_space) override;
  bool IsReusableForFormat(const gfx::Size& dimensions,
                           VideoPixelFormat format,
                           const mojom::PlaneStridesPtr& strides,
                           const gfx::ColorSpace& color_space) override;

  uint32_t GetMemorySizeInBytes() override;

  std::unique_ptr<VideoCaptureBufferHandle> GetMemoryMappedAccess() override;

  base::UnsafeSharedMemoryRegion DuplicateAsUnsafeRegion() override;
  gfx::GpuMemoryBufferHandle GetGpuMemoryBufferHandle() override;
  media::mojom::VideoBufferHandlePtr GetVideoBufferHandle() override;

  VideoCaptureBufferType GetBufferType() override;

  // VideoCaptureGpuContextLostObserver implementation.
  void OnContextLost() override;

 private:
  // Returns true if the capture device was replaced since this slot was
  // created, in which case the slot is also marked invalid so the pool stops
  // re-probing D3D for it on every reservation.
  bool IsD3DDeviceChanged();

  // Drops every resource the slot holds and marks it invalid.
  void ResetSlot();

  // Set on the capture sequence, but read from whichever sequence delivers
  // OnContextLost().
  std::atomic_bool is_valid_{false};
  scoped_refptr<DXGIDeviceManager> dxgi_device_manager_;
  Microsoft::WRL::ComPtr<ID3D11Device> d3d_device_;
  gfx::DXGIHandle dxgi_handle_;
  gfx::Size dimensions_;
  gfx::ColorSpace color_space_;

  // Row size of the premapped region's first plane, as reported to consumers
  // through GetGpuMemoryBufferHandle().
  uint32_t stride_ = 0;
  scoped_refptr<gpu::ClientSharedImage> shared_image_;
  base::UnsafeSharedMemoryRegion premapped_region_;
  base::WritableSharedMemoryMapping premapped_mapping_;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_texture_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_WIN_D3D11_SHARED_IMAGE_BUFFER_TRACKER_H_
