// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/gpu_memory_buffer_factory_dxgi.h"

#include <vector>

#include "base/memory/unsafe_shared_memory_region.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/ipc/common/dxgi_helpers.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/buffer_usage_util.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_bindings.h"

namespace gpu {

GpuMemoryBufferFactoryDXGI::GpuMemoryBufferFactoryDXGI(
    scoped_refptr<base::SingleThreadTaskRunner> io_runner)
    : io_runner_(std::move(io_runner)) {
  DETACH_FROM_THREAD(thread_checker_);
}
GpuMemoryBufferFactoryDXGI::~GpuMemoryBufferFactoryDXGI() {
  // Ensure that IO-thread specific state is destroyed on the IO thread. Note
  // that it is valid to be *accessing* this state on the Viz thread here as the
  // owner of this instance must have guaranteed all calls to this class on the
  // IO thread are finished before destroying this object on the Viz thread (or
  // else those calls would inherently race with destroying this object). Note
  // also that that we need to be holding the ThreadChecker to access
  // `d3d11_device_`.
  DETACH_FROM_THREAD(thread_checker_);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (io_runner_) {
    io_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
               Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_texture) {},
            std::move(d3d11_device_), std::move(staging_texture_)));
  }
}

// TODO(crbug.com/40774668): Avoid the need for a separate D3D device here by
// sharing keyed mutex state between DXGI GMBs and D3D shared image backings.
Microsoft::WRL::ComPtr<ID3D11Device>
GpuMemoryBufferFactoryDXGI::GetOrCreateD3D11Device() {
  DCHECK(!io_runner_ || io_runner_->BelongsToCurrentThread());
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!d3d11_device_ || FAILED(d3d11_device_->GetDeviceRemovedReason())) {
    // Reset device if it was removed.
    d3d11_device_ = nullptr;
    // Use same adapter as ANGLE device.
    auto angle_d3d11_device = gl::QueryD3D11DeviceObjectFromANGLE();
    if (!angle_d3d11_device) {
      DLOG(ERROR) << "Failed to get ANGLE D3D11 device";
      return nullptr;
    }

    Microsoft::WRL::ComPtr<IDXGIDevice> angle_dxgi_device;
    HRESULT hr = angle_d3d11_device.As(&angle_dxgi_device);
    CHECK(SUCCEEDED(hr));

    Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter = nullptr;
    hr = FAILED(angle_dxgi_device->GetAdapter(&dxgi_adapter));
    if (FAILED(hr)) {
      DLOG(ERROR) << "GetAdapter failed with error 0x" << std::hex << hr;
      return nullptr;
    }

    // If adapter is not null, driver type must be D3D_DRIVER_TYPE_UNKNOWN
    // otherwise D3D11CreateDevice will return E_INVALIDARG.
    // See
    // https://docs.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-d3d11createdevice#return-value
    const D3D_DRIVER_TYPE driver_type =
        dxgi_adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE;

    // It's ok to use D3D11_CREATE_DEVICE_SINGLETHREADED because this device is
    // only ever used on the IO thread (verified by |thread_checker_|).
    const UINT flags = D3D11_CREATE_DEVICE_SINGLETHREADED;

    // Using D3D_FEATURE_LEVEL_11_1 is ok since we only support D3D11 when the
    // platform update containing DXGI 1.2 is present on Win7.
    const D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3,  D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1};

    hr = D3D11CreateDevice(dxgi_adapter.Get(), driver_type,
                           /*Software=*/nullptr, flags, feature_levels,
                           std::size(feature_levels), D3D11_SDK_VERSION,
                           &d3d11_device_, /*pFeatureLevel=*/nullptr,
                           /*ppImmediateContext=*/nullptr);
    if (FAILED(hr)) {
      DLOG(ERROR) << "D3D11CreateDevice failed with error 0x" << std::hex << hr;
      return nullptr;
    }

    const char* kDebugName = "GPUIPC_GpuMemoryBufferFactoryDXGI";
    d3d11_device_->SetPrivateData(WKPDID_D3DDebugObjectName, strlen(kDebugName),
                                  kDebugName);
  }
  DCHECK(d3d11_device_);
  return d3d11_device_;
}

gfx::GpuMemoryBufferHandle
GpuMemoryBufferFactoryDXGI::CreateNativeGmbHandleOnIO(
    const gfx::Size& size,
    viz::SharedImageFormat format,
    gfx::BufferUsage usage) {
  DCHECK(io_runner_);

  gfx::GpuMemoryBufferHandle result;
  base::WaitableEvent event;

  io_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](gfx::GpuMemoryBufferHandle* out_gmb_handle,
             base::WaitableEvent* waitable_event,
             GpuMemoryBufferFactoryDXGI* factory, const gfx::Size& size,
             viz::SharedImageFormat format, gfx::BufferUsage usage) {
            *out_gmb_handle =
                factory->CreateNativeGmbHandle(size, format, usage);

            waitable_event->Signal();
          },
          &result, &event, this, size, format, usage));

  event.Wait();

  return result;
}

gfx::GpuMemoryBufferHandle GpuMemoryBufferFactoryDXGI::CreateNativeGmbHandle(
    const gfx::Size& size,
    viz::SharedImageFormat format,
    gfx::BufferUsage usage) {
  if (io_runner_ && !io_runner_->BelongsToCurrentThread()) {
    // Thread-hop is required!
    return CreateNativeGmbHandleOnIO(size, format, usage);
  }

  TRACE_EVENT0("gpu", "GpuMemoryBufferFactoryDXGI::CreateNativeGmbHandle");

  gfx::GpuMemoryBufferHandle handle;
  auto d3d11_device = GetOrCreateD3D11Device();
  if (!d3d11_device) {
    return handle;
  }

  DXGI_FORMAT dxgi_format = gpu::ToDXGIFormat(format);
  if (dxgi_format == DXGI_FORMAT_UNKNOWN) {
    return handle;
  }

  auto buffer_size = viz::SharedMemorySizeForSharedImageFormat(format, size);
  if (!buffer_size) {
    return handle;
  }

  // We are binding as a shader resource and render target regardless of usage,
  // so make sure that the usage is one that we support.
  DCHECK(usage == gfx::BufferUsage::GPU_READ ||
         usage == gfx::BufferUsage::SCANOUT ||
         usage == gfx::BufferUsage::SCANOUT_CPU_READ_WRITE)
      << "Incorrect usage, usage=" << gfx::BufferUsageToString(usage);

  D3D11_TEXTURE2D_DESC desc = {
      static_cast<UINT>(size.width()),
      static_cast<UINT>(size.height()),
      1,
      1,
      dxgi_format,
      {1, 0},
      D3D11_USAGE_DEFAULT,
      D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
      0,
      D3D11_RESOURCE_MISC_SHARED_NTHANDLE |
          D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX};

  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture;

  if (FAILED(d3d11_device->CreateTexture2D(&desc, nullptr, &d3d11_texture))) {
    return handle;
  }

  Microsoft::WRL::ComPtr<IDXGIResource1> dxgi_resource;
  if (FAILED(d3d11_texture.As(&dxgi_resource))) {
    return handle;
  }

  HANDLE texture_handle;
  if (FAILED(dxgi_resource->CreateSharedHandle(
          nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
          nullptr, &texture_handle))) {
    return handle;
  }

  handle = gfx::GpuMemoryBufferHandle(
      gfx::DXGIHandle(base::win::ScopedHandle(texture_handle)));

  return handle;
}

}  // namespace gpu
