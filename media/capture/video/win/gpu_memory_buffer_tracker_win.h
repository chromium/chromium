// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_WIN_GPU_MEMORY_BUFFER_TRACKER_WIN_H_
#define MEDIA_CAPTURE_VIDEO_WIN_GPU_MEMORY_BUFFER_TRACKER_WIN_H_

#include "gpu/ipc/common/gpu_memory_buffer_impl_dxgi.h"
#include "media/base/win/dxgi_device_manager.h"
#include "media/capture/video/video_capture_buffer_tracker.h"

#include <d3d11.h>
#include <wrl.h>

namespace gfx {
class Size;
}  // namespace gfx

namespace media {

// Tracker specifics for Windows GpuMemoryBuffer.
// This class is not thread-safe.
class CAPTURE_EXPORT GpuMemoryBufferTrackerWin final
    : public VideoCaptureBufferTracker {
 public:
  explicit GpuMemoryBufferTrackerWin(
      scoped_refptr<DXGIDeviceManager> dxgi_device_manager);
  GpuMemoryBufferTrackerWin(
      gfx::GpuMemoryBufferHandle gmb_handle,
      scoped_refptr<DXGIDeviceManager> dxgi_device_manager);

  GpuMemoryBufferTrackerWin(const GpuMemoryBufferTrackerWin&) = delete;
  GpuMemoryBufferTrackerWin& operator=(const GpuMemoryBufferTrackerWin&) =
      delete;

  ~GpuMemoryBufferTrackerWin() override;

  // Implementation of VideoCaptureBufferTracker:
  bool Init(const gfx::Size& dimensions,
            VideoPixelFormat format,
            const mojom::PlaneStridesPtr& strides) override;
  bool IsSameGpuMemoryBuffer(
      const gfx::GpuMemoryBufferHandle& handle) const override;
  bool IsReusableForFormat(const gfx::Size& dimensions,
                           VideoPixelFormat format,
                           const mojom::PlaneStridesPtr& strides) override;

  uint32_t GetMemorySizeInBytes() override;

  std::unique_ptr<VideoCaptureBufferHandle> GetMemoryMappedAccess() override;

  base::UnsafeSharedMemoryRegion DuplicateAsUnsafeRegion() override;
  gfx::GpuMemoryBufferHandle GetGpuMemoryBufferHandle() override;

  VideoCaptureBufferType GetBufferType() override;

  void OnHeldByConsumersChanged(bool is_held_by_consumers) override;
  void UpdateExternalData(CapturedExternalVideoBuffer buffer) override;

 private:
  bool CreateBufferInternal(gfx::GpuMemoryBufferHandle buffer_handle,
                            const gfx::Size& dimensions);
  bool IsD3DDeviceChanged();

  std::unique_ptr<gpu::GpuMemoryBufferImplDXGI> buffer_;
  scoped_refptr<DXGIDeviceManager> dxgi_device_manager_;
  Microsoft::WRL::ComPtr<ID3D11Device> d3d_device_;
  base::UnsafeSharedMemoryRegion region_;
  base::WritableSharedMemoryMapping mapping_;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_texture_;
  // |external_dxgi_handle_| is valid until Init() call.
  gfx::GpuMemoryBufferHandle external_dxgi_handle_;
  // The lifetime of the D3D texture is controlled by IMFBuffer. When the buffer
  // lifetime is released, the Windows capture pipeline assumes the application
  // has finished reading from the texture and the capture pipeline is, thus,
  // free to use the texture in a subsequent write operation. We use ComPtr to
  // hold the IMFBuffer and correctly reuse external texture.
  Microsoft::WRL::ComPtr<IMFMediaBuffer> imf_buffer_;
  // If |is_external_dxgi_handle_| is true, the handle originally isn't created
  // by chromium. Currently it indicates the producer of handle is
  // MFVideoCaptureEngine.
  bool is_external_dxgi_handle_ = false;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_WIN_GPU_MEMORY_BUFFER_TRACKER_WIN_H_
