// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_D3D_IMAGE_BACKING_FACTORY_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_D3D_IMAGE_BACKING_FACTORY_H_

#include <d3d11.h>
#include <dxgi1_2.h>
#include <windows.h>
#include <wrl/client.h>

#include <memory>

#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing_factory.h"
#include "gpu/gpu_gles2_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
class Size;
class ColorSpace;
}  // namespace gfx

namespace gpu {
class DXGISharedHandleManager;
class SharedImageBacking;
struct Mailbox;

class GPU_GLES2_EXPORT D3DImageBackingFactory
    : public SharedImageBackingFactory {
 public:
  D3DImageBackingFactory(
      Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
      scoped_refptr<DXGISharedHandleManager> dxgi_shared_handle_manager);

  D3DImageBackingFactory(const D3DImageBackingFactory&) = delete;
  D3DImageBackingFactory& operator=(const D3DImageBackingFactory&) = delete;

  ~D3DImageBackingFactory() override;

  // Returns true if D3D shared images are supported and this factory should be
  // used. Generally this means Skia-GL, passthrough decoder, and ANGLE-D3D11.
  static bool IsD3DSharedImageSupported(const GpuPreferences& gpu_preferences);

  // Returns true if DXGI swap chain shared images for overlays are supported.
  static bool IsSwapChainSupported();

  // Clears |d3d11_texture| to |color| on the immediate context.
  static bool ClearTextureToColor(ID3D11Device* d3d11_device,
                                  ID3D11Texture2D* d3d11_texture,
                                  const SkColor4f& color);

  // Clears the current back buffer to |color| on the immediate context.
  static bool ClearBackBufferToColor(ID3D11Device* d3d11_device,
                                     IDXGISwapChain1* swap_chain,
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

  // Creates IDXGI Swap Chain and exposes front and back buffers as Shared Image
  // mailboxes.
  SwapChainBackings CreateSwapChain(const Mailbox& front_buffer_mailbox,
                                    const Mailbox& back_buffer_mailbox,
                                    viz::SharedImageFormat format,
                                    const gfx::Size& size,
                                    const gfx::ColorSpace& color_space,
                                    GrSurfaceOrigin surface_origin,
                                    SkAlphaType alpha_type,
                                    uint32_t usage);

  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      SurfaceHandle surface_handle,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      std::string debug_label,
      bool is_thread_safe) override;
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      std::string debug_label,
      base::span<const uint8_t> pixel_data) override;
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      std::string debug_label,
      gfx::GpuMemoryBufferHandle handle) override;
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      gfx::GpuMemoryBufferHandle handle,
      gfx::BufferFormat format,
      gfx::BufferPlane plane,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      std::string debug_label) override;

  bool IsSupported(uint32_t usage,
                   viz::SharedImageFormat format,
                   const gfx::Size& size,
                   bool thread_safe,
                   gfx::GpuMemoryBufferType gmb_type,
                   GrContextType gr_context_type,
                   base::span<const uint8_t> pixel_data) override;

  Microsoft::WRL::ComPtr<ID3D11Device> GetDeviceForTesting() const {
    return d3d11_device_;
  }

 private:
  // `format` can be single planar format, multiplanar format (with
  // BufferPlane::DEFAULT) or legacy multiplanar format converted to single
  // planar for per plane access eg. BufferFormat::YUV_420_BIPLANAR converted
  // to RED_8 (for BufferPlane::Y), RG_88 (for BufferPlane::UV). It does not
  // support external sampler use cases.
  std::unique_ptr<SharedImageBacking> CreateSharedImageGMBs(
      const Mailbox& mailbox,
      gfx::GpuMemoryBufferHandle handle,
      viz::SharedImageFormat format,
      gfx::BufferPlane plane,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage);
  bool UseMapOnDefaultTextures();
  bool SupportsBGRA8UnormStorage();

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;
  absl::optional<bool> map_on_default_textures_;
  absl::optional<bool> supports_bgra8unorm_storage_;

  scoped_refptr<DXGISharedHandleManager> dxgi_shared_handle_manager_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_D3D_IMAGE_BACKING_FACTORY_H_
