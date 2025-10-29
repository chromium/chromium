// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_D3D_IMAGE_BACKING_FACTORY_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_D3D_IMAGE_BACKING_FACTORY_H_

#include <windows.h>

#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <memory>
#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/gpu_gles2_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
class Size;
class ColorSpace;
}  // namespace gfx

namespace gpu {
class DawnContextProvider;
class DXGISharedHandleManager;
class SharedImageBacking;
struct Mailbox;

class GPU_GLES2_EXPORT D3DImageBackingFactory
    : public SharedImageBackingFactory {
 public:
  D3DImageBackingFactory(
      Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
      scoped_refptr<DXGISharedHandleManager> dxgi_shared_handle_manager,
      const GLFormatCaps& gl_format_caps,
      const GpuDriverBugWorkarounds& workarounds = GpuDriverBugWorkarounds(),
      bool enable_webnn_only_d3d_factory = false);

  D3DImageBackingFactory(const D3DImageBackingFactory&) = delete;
  D3DImageBackingFactory& operator=(const D3DImageBackingFactory&) = delete;

  ~D3DImageBackingFactory() override;

  // `io_runner` is needed in order to create GpuMemoryBufferHandles on the
  // correct thread. GpuServiceImpl calls into this class on the IO thread when
  // processing IPC requests, so we need to ensure that other callers are able
  // to thread-hop to that runner when creating GMBHandles (so far, the other
  // caller for whom it matters is `FrameSinkVideoCapturerImpl` on Windows).
  static gfx::GpuMemoryBufferHandle CreateGpuMemoryBufferHandle(
      scoped_refptr<base::SingleThreadTaskRunner> io_runner,
      const gfx::Size& size,
      viz::SharedImageFormat format,
      gfx::BufferUsage usage);

  static bool CopyNativeBufferToSharedMemoryAsync(
      gfx::GpuMemoryBufferHandle buffer_handle,
      base::UnsafeSharedMemoryRegion shared_memory);

  // Returns true if D3D shared images are supported and this factory should be
  // used. Generally this means Skia-GL, passthrough decoder, and ANGLE-D3D11.
  static bool IsD3DSharedImageSupported(ID3D11Device* d3d11_device,
                                        const GpuPreferences& gpu_preferences);

  // Returns true if DXGI swap chain shared images for overlays are supported.
  static bool IsSwapChainSupported(
      const GpuPreferences& gpu_preferences,
      DawnContextProvider* dawn_context_provider = nullptr);

  // Clears the current back buffer to |color| on the immediate context.
  static bool ClearBackBufferToColor(IDXGISwapChain1* swap_chain,
                                     const SkColor4f& color);

  struct GPU_GLES2_EXPORT SwapChainBackings {
    SwapChainBackings(std::unique_ptr<SharedImageBacking> front_buffer,
                      std::unique_ptr<SharedImageBacking> back_buffer);

    SwapChainBackings(const SwapChainBackings&) = delete;
    SwapChainBackings& operator=(const SwapChainBackings&) = delete;

    ~SwapChainBackings();
    SwapChainBackings(SwapChainBackings&&);
    SwapChainBackings& operator=(SwapChainBackings&&);

    std::unique_ptr<SharedImageBacking> front_buffer;
    std::unique_ptr<SharedImageBacking> back_buffer;
  };

  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      SurfaceHandle surface_handle,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      SharedImageUsageSet usage,
      std::string debug_label,
      bool is_thread_safe) override;
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      SharedImageUsageSet usage,
      std::string debug_label,
      bool is_thread_safe,
      base::span<const uint8_t> pixel_data) override;
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      SharedImageUsageSet usage,
      std::string debug_label,
      bool is_thread_safe,
      gfx::GpuMemoryBufferHandle handle) override;

  bool IsSupported(SharedImageUsageSet usage,
                   viz::SharedImageFormat format,
                   const gfx::Size& size,
                   bool thread_safe,
                   gfx::GpuMemoryBufferType gmb_type,
                   GrContextType gr_context_type,
                   base::span<const uint8_t> pixel_data) override;
  SharedImageBackingType GetBackingType() override;
  Microsoft::WRL::ComPtr<ID3D11Device> GetDeviceForTesting() const {
    return d3d11_device_;
  }

 private:
  static gfx::GpuMemoryBufferHandle CreateGpuMemoryBufferHandleOnIO(
      scoped_refptr<base::SingleThreadTaskRunner> io_runner,
      const gfx::Size& size,
      viz::SharedImageFormat format,
      gfx::BufferUsage usage);

  std::unique_ptr<SharedImageBacking> CreateSharedBufferD3D12(
      const Mailbox& mailbox,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      SharedImageUsageSet usage,
      std::string debug_label);

  bool CreateSwapChainInternal(
      Microsoft::WRL::ComPtr<IDXGISwapChain1>& swap_chain,
      Microsoft::WRL::ComPtr<ID3D11Texture2D>& back_buffer_texture,
      Microsoft::WRL::ComPtr<ID3D11Texture2D>& front_buffer_texture,
      viz::SharedImageFormat format,
      const gfx::Size& size);

  bool SupportsBGRA8UnormStorage();

  // D3D11 device used for creating textures. This is also Skia's D3D11 device.
  // Can be different from |angle_d3d11_device_| when using Graphite.
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;

  // A D3D12 device is currently used for creation of buffer resources to be
  // used with WebNN and WebGPU.
  Microsoft::WRL::ComPtr<ID3D12Device> d3d12_device_;

  std::optional<bool> supports_bgra8unorm_storage_;

  scoped_refptr<DXGISharedHandleManager> dxgi_shared_handle_manager_;

  // D3D11 device used by ANGLE. Can be different from |d3d11_device_| when
  // using Graphite.
  Microsoft::WRL::ComPtr<ID3D11Device> angle_d3d11_device_;

  // Stores the maximum size dimension supported by an nv12 texture.
  int max_nv12_dim_supported_ = 0;

  // Stores whether NV12 format is supported by the D3D device.
  bool d3d11_supports_nv12_;

  // Capabilities needed for getting the correct GL format for creating GL
  // textures.
  const GLFormatCaps gl_format_caps_;

  // True if using UpdateSubresource1() in UploadFromMemory() is allowed.
  const bool use_update_subresource1_;

  // Allow D3D factory for WebNN even if support is disabled.
  const bool enable_webnn_only_d3d_factory_ = false;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_D3D_IMAGE_BACKING_FACTORY_H_
