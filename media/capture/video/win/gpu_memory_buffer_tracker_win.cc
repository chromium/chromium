// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/win/gpu_memory_buffer_tracker_win.h"

#include <dxgi1_2.h>

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/raw_span.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/trace_event/trace_event.h"
#include "base/unguessable_token.h"
#include "base/win/scoped_handle.h"
#include "gpu/ipc/common/dxgi_helpers.h"
#include "media/base/win/mf_helpers.h"
#include "media/capture/video/video_capture_buffer_handle.h"
#include "ui/gfx/geometry/size.h"

namespace media {

namespace {

class DXGIGMBTrackerHandle : public media::VideoCaptureBufferHandle {
 public:
  explicit DXGIGMBTrackerHandle(base::span<uint8_t> data,
                                HANDLE dxgi_handle,
                                ID3D11Device* d3d11_device)
      : data_(data), dxgi_handle_(dxgi_handle), d3d11_device_(d3d11_device) {}

  size_t mapped_size() const final { return data_.size(); }
  uint8_t* data() const final { return data_.data(); }
  const uint8_t* const_data() const final { return data_.data(); }

  ~DXGIGMBTrackerHandle() override {
    gpu::CopyShMemToDXGIBuffer(data_, dxgi_handle_, d3d11_device_);
  }

 private:
  base::raw_span<uint8_t> data_;
  HANDLE dxgi_handle_;
  raw_ptr<ID3D11Device> d3d11_device_;
};

base::win::ScopedHandle CreateNV12Texture(ID3D11Device* d3d11_device,
                                          const gfx::Size& size) {
  const DXGI_FORMAT dxgi_format = DXGI_FORMAT_NV12;
  D3D11_TEXTURE2D_DESC desc = {
      .Width = static_cast<UINT>(size.width()),
      .Height = static_cast<UINT>(size.height()),
      .MipLevels = 1,
      .ArraySize = 1,
      .Format = dxgi_format,
      .SampleDesc = {1, 0},
      .Usage = D3D11_USAGE_DEFAULT,
      .BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
      .CPUAccessFlags = 0,
      .MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE |
                   D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX};

  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture;

  HRESULT hr = d3d11_device->CreateTexture2D(&desc, nullptr, &d3d11_texture);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to create D3D11 texture: "
                << logging::SystemErrorCodeToString(hr);
    return base::win::ScopedHandle();
  }
  hr = SetDebugName(d3d11_texture.Get(), "Camera_MemoryBufferTracker");
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to label D3D11 texture: "
                << logging::SystemErrorCodeToString(hr);
    return base::win::ScopedHandle();
  }

  Microsoft::WRL::ComPtr<IDXGIResource1> dxgi_resource;
  hr = d3d11_texture.As(&dxgi_resource);
  CHECK(SUCCEEDED(hr));

  HANDLE texture_handle;
  hr = dxgi_resource->CreateSharedHandle(
      nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr,
      &texture_handle);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to create shared D3D11 texture handle: "
                << logging::SystemErrorCodeToString(hr);
    return base::win::ScopedHandle();
  }
  return base::win::ScopedHandle(texture_handle);
}

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
                                     const mojom::PlaneStridesPtr& strides) {
  // Only support NV12
  if (format != PIXEL_FORMAT_NV12) {
    NOTREACHED_IN_MIGRATION() << "Unsupported VideoPixelFormat " << format;
    return false;
  }

  if (is_external_dxgi_handle_) {
    return CreateBufferInternal(std::move(external_dxgi_handle_),
                                std::move(dimensions));
  }

  gfx::GpuMemoryBufferHandle gmb_handle;
  gmb_handle.dxgi_handle = CreateNV12Texture(d3d_device_.Get(), dimensions);
  gmb_handle.dxgi_token = gfx::DXGIHandleToken();
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
  // duplicated, please see GpuMemoryBufferImplDXGI::CloneHandle.
  return buffer_->GetToken() == handle.dxgi_token;
}

bool GpuMemoryBufferTrackerWin::CreateBufferInternal(
    gfx::GpuMemoryBufferHandle buffer_handle,
    const gfx::Size& dimensions) {
  if (!buffer_handle.dxgi_handle.IsValid()) {
    LOG(ERROR) << "dxgi_handle is not valid.";
    return false;
  }

  buffer_ = gpu::GpuMemoryBufferImplDXGI::CreateFromHandle(
      std::move(buffer_handle), std::move(dimensions),
      gfx::BufferFormat::YUV_420_BIPLANAR, gfx::BufferUsage::GPU_READ,
      gpu::GpuMemoryBufferImpl::DestructionCallback(), nullptr, nullptr);
  if (!buffer_) {
    NOTREACHED_IN_MIGRATION() << "Failed to create GPU memory buffer";
    return false;
  }

  region_ = base::UnsafeSharedMemoryRegion::Create(GetMemorySizeInBytes());
  mapping_ = region_.Map();

  return true;
}

bool GpuMemoryBufferTrackerWin::IsD3DDeviceChanged() {
  // Check for and handle device loss by recreating the texture
  Microsoft::WRL::ComPtr<ID3D11Device> recreated_d3d_device;
  HRESULT hr = dxgi_device_manager_->CheckDeviceRemovedAndGetDevice(
      &recreated_d3d_device);
  if (FAILED(hr)) {
    LOG(ERROR) << "Detected device loss: "
               << logging::SystemErrorCodeToString(hr);
    base::UmaHistogramSparse("Media.VideoCapture.Win.D3DDeviceRemovedReason",
                             hr);
  }
  return recreated_d3d_device != d3d_device_;
}

bool GpuMemoryBufferTrackerWin::IsReusableForFormat(
    const gfx::Size& dimensions,
    VideoPixelFormat format,
    const mojom::PlaneStridesPtr& strides) {
  // External buffer is never reused.
  return !IsD3DDeviceChanged() && (format == PIXEL_FORMAT_NV12) &&
         (dimensions == buffer_->GetSize()) && !is_external_dxgi_handle_;
}

std::unique_ptr<VideoCaptureBufferHandle>
GpuMemoryBufferTrackerWin::GetMemoryMappedAccess() {
  return std::make_unique<DXGIGMBTrackerHandle>(
      mapping_.GetMemoryAsSpan<uint8_t>(), buffer_->GetHandle(),
      d3d_device_.Get());
}

base::UnsafeSharedMemoryRegion
GpuMemoryBufferTrackerWin::DuplicateAsUnsafeRegion() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "GpuMemoryBufferTrackerWin::DuplicateAsUnsafeRegion");

  if (!buffer_) {
    return base::UnsafeSharedMemoryRegion();
  }

  CHECK(region_.IsValid());
  CHECK(mapping_.IsValid());

  if (!gpu::CopyDXGIBufferToShMem(buffer_->GetHandle(),
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
  auto handle = buffer_->CloneHandle();
  handle.region = region_.Duplicate();
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
  DCHECK(buffer_);
  return (buffer_->GetSize().width() * buffer_->GetSize().height() * 3) / 2;
}

}  // namespace media
