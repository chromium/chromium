// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DCOMP_SURFACE_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DCOMP_SURFACE_IMAGE_BACKING_H_

#include <windows.h>

#include <d3d11.h>
#include <dcomp.h>
#include <wrl/client.h>

#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/service/shared_image/dcomp_surface_image_representation.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/scoped_make_current.h"

namespace gpu {

class SharedImageManager;
class MemoryTypeTracker;

// Implementation of SharedImageBacking that holds a DComp surface.
class GPU_GLES2_EXPORT DCompSurfaceImageBacking
    : public ClearTrackingSharedImageBacking {
 public:
  static std::unique_ptr<DCompSurfaceImageBacking> Create(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      DXGI_FORMAT internal_format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      gpu::SharedImageUsageSet usage,
      std::string debug_label);

  DCompSurfaceImageBacking(const DCompSurfaceImageBacking&) = delete;
  DCompSurfaceImageBacking& operator=(const DCompSurfaceImageBacking&) = delete;

  ~DCompSurfaceImageBacking() override;

  // SharedImageBacking implementation.
  SharedImageBackingType GetType() const override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;

 protected:
  // Produce a lightweight wrapper that can retrieve the |dcomp_surface_| from
  // this backing.
  std::unique_ptr<OverlayImageRepresentation> ProduceOverlay(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;

  // Produce a write-only representation that calls back into this backing's
  // |BeginDrawGanesh| and |EndDrawGanesh| on scoped write access.
  // Currently, only one writer to a DCompSurfaceImageBacking is allowed
  // globally due to DComp surface limitations (which can be resolved with
  // Suspend/ResumeDraw) and usage of the GL FB0 (which can be resolved by
  // binding the DComp texture to a renderbuffer instead of a pbuffer).
  std::unique_ptr<SkiaGaneshImageRepresentation> ProduceSkiaGanesh(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;

  // Produce a write-only representation that calls back into this backing's
  // |BeginDrawGraphite| and |EndDrawGraphite| on scoped write access.
  // Currently, only one writer to a DCompSurfaceImageBacking is allowed
  // globally due to DComp surface limitations (which can be resolved with
  // Suspend/ResumeDraw).
  std::unique_ptr<SkiaGraphiteImageRepresentation> ProduceSkiaGraphite(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;

 private:
  DCompSurfaceImageBacking(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      gpu::SharedImageUsageSet usage,
      std::string debug_label,
      Microsoft::WRL::ComPtr<IDCompositionSurface> dcomp_surface);

  // For DCompSurfaceOverlayImageRepresentation implementation.
  friend class DCompSurfaceOverlayImageRepresentation;
  std::optional<gl::DCLayerOverlayImage> GetDCLayerOverlayImage() {
    return std::make_optional<gl::DCLayerOverlayImage>(size(), dcomp_surface_,
                                                       dcomp_surface_serial_);
  }

  Microsoft::WRL::ComPtr<ID3D11Texture2D> BeginDraw(
      const gfx::Rect& update_rect,
      gfx::Point& update_offset_out);
  void EndDraw();

  // For DCompSurfaceSkiaGaneshImageRepresentation implementation.
  friend class DCompSurfaceSkiaGaneshImageRepresentation;
  sk_sp<SkSurface> BeginDrawGanesh(SharedContextState* context_state,
                                   int final_msaa_count,
                                   const SkSurfaceProps& surface_props,
                                   const gfx::Rect& update_rect);
  void EndDrawGanesh();

  // For DCompSurfaceDawnImageRepresentation implementation.
  friend class DCompSurfaceDawnImageRepresentation;
  wgpu::Texture BeginDrawDawn(const wgpu::Device& device,
                              const wgpu::TextureUsage usage,
                              const wgpu::TextureUsage internal_usage,
                              const gfx::Rect& update_rect);
  void EndDrawDawn(const wgpu::Device& device, wgpu::Texture texture);

  // Used to restore the surface that was current before BeginDraw at EndDraw.
  std::optional<ui::ScopedMakeCurrent> scoped_make_current_;

  // GLSurface that binds |dcomp_surface_|'s draw texture to GL FB0 between
  // |BeginDrawGanesh| and |EndDrawGanesh|.
  class D3DTextureGLSurfaceEGL;
  scoped_refptr<D3DTextureGLSurfaceEGL> gl_surface_;

  Microsoft::WRL::ComPtr<IDCompositionSurface> dcomp_surface_;

  // The texture returned from |dcomp_surface_|'s BeginDraw.
  Microsoft::WRL::ComPtr<ID3D11Texture2D> dcomp_surface_draw_texture_;

  // The intermediate texture that is wrapped into wgpu::Texture and used for
  // drawing. The content will be copied back to |dcomp_surface_draw_texture_|
  // at EndDraw.
  Microsoft::WRL::ComPtr<ID3D11Texture2D> dcomp_surface_draw_texture_copy_;

  // The update_rect passed to |dcomp_surface_|'s BeginDraw.
  gfx::Rect update_rect_;

  // The update_offset returned from |dcomp_surface_|'s BeginDraw.
  gfx::Point dcomp_update_offset_;

  // SharedTextureMemory is created from |dcomp_surface_|'s draw texture between
  // |BeginDrawGraphite| and |EndDrawGraphite|. This |shared_texture_memory_|
  // wraps the ComPtr<ID3D11Texture> instead of creating from a share HANDLE.
  wgpu::SharedTextureMemory shared_texture_memory_;

  // This is a number that increments once for every EndDraw on a surface, and
  // is used to determine when the contents have changed so Commit() needs to
  // be called on the device.
  uint64_t dcomp_surface_serial_ = 0;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DCOMP_SURFACE_IMAGE_BACKING_H_
