// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/win/gpu_memory_buffer_tracker_win.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/raw_span.h"
#include "base/notreached.h"
#include "base/trace_event/trace_event.h"
#include "base/unguessable_token.h"
#include "base/win/scoped_handle.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/ipc/common/dxgi_helpers.h"
#include "media/capture/video/video_capture_buffer_handle.h"
#include "media/capture/video/win/d3d11_utils.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"

namespace media {

namespace {

class DXGIGMBTrackerHandle : public media::VideoCaptureBufferHandle {
 public:
  explicit DXGIGMBTrackerHandle(base::span<uint8_t> data,
                                HANDLE dxgi_handle,
                                ID3D11Device* d3d11_device)
      : data_(data), dxgi_handle_(dxgi_handle), d3d11_device_(d3d11_device) {}

  size_t mapped_size() const final { return data_.size(); }
  base::span<uint8_t> data() final { return data_; }
  base::span<const uint8_t> const_data() const final { return data_; }

  ~DXGIGMBTrackerHandle() override {
    gpu::CopyShMemToDXGIBuffer(data_, dxgi_handle_, d3d11_device_);
  }

 private:
  base::raw_span<uint8_t> data_;
  HANDLE dxgi_handle_;
  raw_ptr<ID3D11Device> d3d11_device_;
};

}  // namespace

GpuMemoryBufferTrackerWin::GpuMemoryBufferTrackerWin(
    scoped_refptr<DXGIDeviceManager> dxgi_device_manager)
    : dxgi_device_manager_(std::move(dxgi_device_manager)),
      d3d_device_(dxgi_device_manager_->GetDevice()) {}

GpuMemoryBufferTrackerWin::GpuMemoryBufferTrackerWin(
    gfx::GpuMemoryBufferHandle gmb_handle,
    scoped_refptr<DXGIDeviceManager> dxgi_device_manager)
    : dxgi_device_manager_(std::move(dxgi_device_manager)),
      d3d_device_(dxgi_device_manager_->GetDevice()),
      external_dxgi_handle_(std::move(gmb_handle)),
      is_external_dxgi_handle_(true) {}

GpuMemoryBufferTrackerWin::~GpuMemoryBufferTrackerWin() = default;

bool GpuMemoryBufferTrackerWin::Init(const gfx::Size& dimensions,
                                     VideoPixelFormat format,
                                     const mojom::PlaneStridesPtr& strides,
                                     const gfx::ColorSpace& color_space) {
  // Only support NV12
  if (format != PIXEL_FORMAT_NV12) {
    NOTREACHED() << "Unsupported VideoPixelFormat " << format;
  }

  if (is_external_dxgi_handle_) {
    return CreateBufferInternal(std::move(external_dxgi_handle_),
                                std::move(dimensions));
  }

  base::win::ScopedHandle scoped_handle = CreateNV12Texture(
      d3d_device_.Get(), dimensions, "Camera_MemoryBufferTracker");
  if (!scoped_handle.is_valid()) {
    return false;
  }

  gfx::DXGIHandle dxgi_handle = gfx::DXGIHandle(std::move(scoped_handle));
  if (!dxgi_handle.IsValid()) {
    return false;
  }

  gfx::GpuMemoryBufferHandle gmb_handle(std::move(dxgi_handle));
  return CreateBufferInternal(std::move(gmb_handle), std::move(dimensions));
}

bool GpuMemoryBufferTrackerWin::IsSameGpuMemoryBuffer(
    const gfx::GpuMemoryBufferHandle& handle) const {
  // This function is used for reusing external buffer.
  if (!is_external_dxgi_handle_) {
    return false;
  }
  // On Windows, we need use 'dxgi_token' to decide whether the two handles
  // point to same gmb instead of handle directly since handle could be
  // duplicated.
  return dxgi_handle_.token() == handle.dxgi_handle().token();
}

bool GpuMemoryBufferTrackerWin::CreateBufferInternal(
    gfx::GpuMemoryBufferHandle buffer_handle,
    const gfx::Size& dimensions) {
  if (!buffer_handle.dxgi_handle().IsValid()) {
    LOG(ERROR) << "dxgi_handle is not valid.";
    return false;
  }

  dxgi_handle_ = std::move(buffer_handle).dxgi_handle();
  dimensions_ = dimensions;
  stride_ = viz::SharedMemoryRowSizeForSharedImageFormat(
                viz::MultiPlaneFormat::kNV12, /*plane=*/0, dimensions_.width())
                .value();

  region_ = base::UnsafeSharedMemoryRegion::Create(GetMemorySizeInBytes());
  mapping_ = region_.Map();

  return true;
}

bool GpuMemoryBufferTrackerWin::IsD3DDeviceChanged() {
  return HasD3DDeviceChanged(dxgi_device_manager_.get(), d3d_device_.Get());
}

bool GpuMemoryBufferTrackerWin::IsReusableForFormat(
    const gfx::Size& dimensions,
    VideoPixelFormat format,
    const mojom::PlaneStridesPtr& strides,
    const gfx::ColorSpace& color_space) {
  // External buffer is never reused.
  return !IsD3DDeviceChanged() && (format == PIXEL_FORMAT_NV12) &&
         (dimensions == dimensions_) && !is_external_dxgi_handle_;
}

std::unique_ptr<VideoCaptureBufferHandle>
GpuMemoryBufferTrackerWin::GetMemoryMappedAccess() {
  return std::make_unique<DXGIGMBTrackerHandle>(
      mapping_.GetMemoryAsSpan<uint8_t>(), dxgi_handle_.buffer_handle(),
      d3d_device_.Get());
}

base::UnsafeSharedMemoryRegion
GpuMemoryBufferTrackerWin::DuplicateAsUnsafeRegion() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "GpuMemoryBufferTrackerWin::DuplicateAsUnsafeRegion");

  if (!dxgi_handle_.IsValid()) {
    return base::UnsafeSharedMemoryRegion();
  }

  CHECK(region_.IsValid());
  CHECK(mapping_.IsValid());

  if (!gpu::CopyDXGIBufferToShMem(dxgi_handle_.buffer_handle(),
                                  mapping_.GetMemoryAsSpan<uint8_t>(),
                                  d3d_device_.Get(), &staging_texture_)) {
    DLOG(ERROR) << "Couldn't copy DXGI buffer to shmem";
    return base::UnsafeSharedMemoryRegion();
  }

  return region_.Duplicate();
}

gfx::GpuMemoryBufferHandle
GpuMemoryBufferTrackerWin::GetGpuMemoryBufferHandle() {
  if (IsD3DDeviceChanged()) {
    return gfx::GpuMemoryBufferHandle();
  }
  gfx::GpuMemoryBufferHandle handle(
      dxgi_handle_.CloneWithRegion(region_.Duplicate()));
  handle.offset = 0;
  handle.stride = stride_;
  return handle;
}

VideoCaptureBufferType GpuMemoryBufferTrackerWin::GetBufferType() {
  return VideoCaptureBufferType::kGpuMemoryBuffer;
}

void GpuMemoryBufferTrackerWin::OnHeldByConsumersChanged(
    bool is_held_by_consumers) {
  if (!is_held_by_consumers) {
    imf_buffer_ = nullptr;
  }
}

void GpuMemoryBufferTrackerWin::UpdateExternalData(
    media::CapturedExternalVideoBuffer buffer) {
  imf_buffer_ = std::move(buffer.imf_buffer);
}

uint32_t GpuMemoryBufferTrackerWin::GetMemorySizeInBytes() {
  DCHECK(dxgi_handle_.IsValid());
  return (dimensions_.width() * dimensions_.height() * 3) / 2;
}

}  // namespace media
