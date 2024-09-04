// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DXGI_SWAP_CHAIN_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DXGI_SWAP_CHAIN_IMAGE_BACKING_H_

#include <windows.h>

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <utility>

#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/shared_image/d3d_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gl/buildflags.h"

namespace gpu {

class D3DImageBacking;
class SharedContextState;
class SharedImageManager;
class MemoryTypeTracker;

class GPU_GLES2_EXPORT DXGISwapChainImageBacking
    : public ClearTrackingSharedImageBacking {
 public:
  static std::unique_ptr<DXGISwapChainImageBacking> Create(
      Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      DXGI_FORMAT internal_format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      gpu::SharedImageUsageSet usage,
      std::string debug_label);

  DXGISwapChainImageBacking(const DXGISwapChainImageBacking&) = delete;
  DXGISwapChainImageBacking& operator=(const DXGISwapChainImageBacking&) =
      delete;

  ~DXGISwapChainImageBacking() override;

  // SharedImageBacking implementation.
  SharedImageBackingType GetType() const override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;

 protected:
  std::unique_ptr<OverlayImageRepresentation> ProduceOverlay(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;

  std::unique_ptr<SkiaGaneshImageRepresentation> ProduceSkiaGanesh(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;

  std::unique_ptr<SkiaGraphiteImageRepresentation> ProduceSkiaGraphite(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;

 private:
  DXGISwapChainImageBacking(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      gpu::SharedImageUsageSet usage,
      std::string debug_label,
      Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
      Microsoft::WRL::ComPtr<IDXGISwapChain1> dxgi_swap_chain,
      int buffers_need_alpha_initialization_count);

  friend class DXGISwapChainOverlayImageRepresentation;
  bool Present(bool should_synchronize_present_with_vblank);
  std::optional<gl::DCLayerOverlayImage> GetDCLayerOverlayImage() {
    return std::make_optional<gl::DCLayerOverlayImage>(size(),
                                                       dxgi_swap_chain_);
  }

  friend class SkiaGLImageRepresentationDXGISwapChain;
  // Called by the Skia representation to indicate where it intends to draw.
  bool DidBeginWriteAccess(const gfx::Rect& swap_rect);

  friend class DawnRepresentationDXGISwapChain;
  wgpu::Texture BeginAccessDawn(const wgpu::Device& device,
                                wgpu::TextureUsage usage,
                                wgpu::TextureUsage internal_usage,
                                const gfx::Rect& update_rect);
  void EndAccessDawn(const wgpu::Device& device, wgpu::Texture texture);

  std::optional<gfx::Rect> pending_swap_rect_;

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;
  Microsoft::WRL::ComPtr<IDXGISwapChain1> dxgi_swap_chain_;

  // Holds a gles2::TexturePassthrough and corresponding egl image.
  scoped_refptr<D3DImageBacking::GLTextureHolder> gl_texture_holder_;

  // SharedTextureMemory is created from DXGISwapChain's backbuffer texture.
  // This |shared_texture_memory_| wraps the ComPtr<ID3D11Texture> instead of
  // creating from a share HANDLE.
  wgpu::SharedTextureMemory shared_texture_memory_;

  // Count of buffers in |dxgi_swap_chain_| that need to have their alpha
  // channels be cleared to opaque before use. If positive at the start of write
  // access, this count will be decremented and the back buffer cleared.
  // This value should be initialized to 0 or the swap chain's buffer count. It
  // assumes DXGI swap chains cycle through its buffers in order, so after
  // calling present |buffer_count - 1| times, |GetBuffer(0)| will have referred
  // to every buffer once.
  int buffers_need_alpha_initialization_count_ = 0;

  bool first_swap_ = true;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DXGI_SWAP_CHAIN_IMAGE_BACKING_H_
