// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DXGI_SWAP_CHAIN_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DXGI_SWAP_CHAIN_IMAGE_BACKING_H_

#include <d3d11.h>
#include <dxgi1_2.h>
#include <windows.h>
#include <wrl/client.h>
#include <utility>

#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/shared_image/d3d_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_fence.h"

namespace gpu {

class SharedImageManager;
class MemoryTypeTracker;

class GPU_GLES2_EXPORT DXGISwapChainImageBacking
    : public ClearTrackingSharedImageBacking {
 public:
  static std::unique_ptr<DXGISwapChainImageBacking> Create(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      DXGI_FORMAT internal_format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage);

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

  std::unique_ptr<SkiaImageRepresentation> ProduceSkia(
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
      uint32_t usage,
      Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
      Microsoft::WRL::ComPtr<IDXGISwapChain1> dxgi_swap_chain);

  friend class DXGISwapChainOverlayImageRepresentation;
  bool Present(bool should_synchronize_present_with_vblank);
  absl::optional<gl::DCLayerOverlayImage> GetDCLayerOverlayImage() {
    return absl::make_optional<gl::DCLayerOverlayImage>(size(),
                                                        dxgi_swap_chain_);
  }

  friend class SkiaGLImageRepresentationDXGISwapChain;
  // Called by the Skia representation to indicate where it intends to draw.
  void AddSwapRect(const gfx::Rect& swap_rect);
  absl::optional<gfx::Rect> pending_swap_rect_;

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;
  Microsoft::WRL::ComPtr<IDXGISwapChain1> dxgi_swap_chain_;

  // Holds a gles2::TexturePassthrough and corresponding egl image.
  scoped_refptr<D3DImageBacking::GLTextureHolder> gl_texture_holder_;

  bool first_swap_ = true;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DXGI_SWAP_CHAIN_IMAGE_BACKING_H_
