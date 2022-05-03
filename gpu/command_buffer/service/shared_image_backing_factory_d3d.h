// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_D3D_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_D3D_H_

#include <d3d11.h>
#include <dxgi1_2.h>
#include <windows.h>
#include <wrl/client.h>

#include <memory>

#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/service/shared_image_backing_factory.h"
#include "gpu/gpu_gles2_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace gfx {
class Size;
class ColorSpace;
}  // namespace gfx

namespace gpu {
class DXGISharedHandleManager;
class SharedImageBacking;
struct Mailbox;

class GPU_GLES2_EXPORT SharedImageBackingFactoryD3D
    : public SharedImageBackingFactory {
 public:
  SharedImageBackingFactoryD3D(
      Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
      scoped_refptr<DXGISharedHandleManager> dxgi_shared_handle_manager);

  SharedImageBackingFactoryD3D(const SharedImageBackingFactoryD3D&) = delete;
  SharedImageBackingFactoryD3D& operator=(const SharedImageBackingFactoryD3D&) =
      delete;

  ~SharedImageBackingFactoryD3D() override;

  // Returns true if D3D shared images are supported and this factory should be
  // used. Generally this means Skia-GL, passthrough decoder, and ANGLE-D3D11.
  static bool IsD3DSharedImageSupported(const GpuPreferences& gpu_preferences);

  // Returns true if DXGI swap chain shared images for overlays are supported.
  static bool IsSwapChainSupported();

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

  // Creates IDXGI Swap Chain and exposes front and back buffers as Shared Image
  // mailboxes.
  SwapChainBackings CreateSwapChain(const Mailbox& front_buffer_mailbox,
                                    const Mailbox& back_buffer_mailbox,
                                    viz::ResourceFormat format,
                                    const gfx::Size& size,
                                    const gfx::ColorSpace& color_space,
                                    GrSurfaceOrigin surface_origin,
                                    SkAlphaType alpha_type,
                                    uint32_t usage);

  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      SurfaceHandle surface_handle,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      bool is_thread_safe) override;
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      base::span<const uint8_t> pixel_data) override;
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      int client_id,
      gfx::GpuMemoryBufferHandle handle,
      gfx::BufferFormat format,
      gfx::BufferPlane plane,
      SurfaceHandle surface_handle,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage) override;
  std::vector<std::unique_ptr<SharedImageBacking>> CreateSharedImageVideoPlanes(
      base::span<const Mailbox> mailboxes,
      gfx::GpuMemoryBufferHandle handle,
      gfx::BufferFormat format,
      const gfx::Size& size,
      uint32_t usage) override;

  bool IsSupported(uint32_t usage,
                   viz::ResourceFormat format,
                   bool thread_safe,
                   gfx::GpuMemoryBufferType gmb_type,
                   GrContextType gr_context_type,
                   bool* allow_legacy_mailbox,
                   bool is_pixel_used) override;

  // Returns true if the specified GpuMemoryBufferType can be imported using
  // this factory.
  bool CanImportGpuMemoryBuffer(gfx::GpuMemoryBufferType memory_buffer_type,
                                viz::ResourceFormat format);

  Microsoft::WRL::ComPtr<ID3D11Device> GetDeviceForTesting() const {
    return d3d11_device_;
  }

 private:
  bool UseMapOnDefaultTextures();

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;
  absl::optional<bool> map_on_default_textures_;

  scoped_refptr<DXGISharedHandleManager> dxgi_shared_handle_manager_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_D3D_H_
