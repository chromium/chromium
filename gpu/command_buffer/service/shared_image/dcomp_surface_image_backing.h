// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DCOMP_SURFACE_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DCOMP_SURFACE_IMAGE_BACKING_H_

#include <d3d11.h>
#include <dcomp.h>
#include <windows.h>
#include <wrl/client.h>

#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/service/shared_image/dcomp_surface_image_representation.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "ui/gfx/geometry/rect.h"
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
      uint32_t usage);

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
  // |BeginDraw| and |EndDraw| on scoped write access.
  // Currently, only one writer to a DCompSurfaceImageBacking is allowed
  // globally due to DComp surface limitations (which can be resolved with
  // Suspend/ResumeDraw) and usage of the GL FB0 (which can be resolved by
  // binding the DComp texture to a renderbuffer instead of a pbuffer).
  std::unique_ptr<SkiaImageRepresentation> ProduceSkia(
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
      uint32_t usage,
      Microsoft::WRL::ComPtr<IDCompositionSurface> dcomp_surface);

  // For DCompSurfaceOverlayImageRepresentation implementation.
  friend class DCompSurfaceOverlayImageRepresentation;
  OverlayImageRepresentation::DCompLayerContent GetDCompLayerContent() const {
    return OverlayImageRepresentation::DCompLayerContent(dcomp_surface_,
                                                         dcomp_surface_serial_);
  }

  // For DCompSurfaceSkiaImageRepresentation implementation.
  friend class DCompSurfaceSkiaImageRepresentation;
  sk_sp<SkSurface> BeginDraw(SharedContextState* context_state,
                             int final_msaa_count,
                             const SkSurfaceProps& surface_props,
                             const gfx::Rect& update_rect);
  bool EndDraw();

  // Used to restore the surface that was current before BeginDraw at EndDraw.
  absl::optional<ui::ScopedMakeCurrent> scoped_make_current_;

  // GLSurface that binds |dcomp_surface_|'s draw texture to GL FB0 between
  // |BeginDraw| and |EndDraw|.
  class D3DTextureGLSurfaceEGL;
  scoped_refptr<D3DTextureGLSurfaceEGL> gl_surface_;

  Microsoft::WRL::ComPtr<IDCompositionSurface> dcomp_surface_;

  // This is a number that increments once for every EndDraw on a surface, and
  // is used to determine when the contents have changed so Commit() needs to
  // be called on the device.
  uint64_t dcomp_surface_serial_ = 0;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DCOMP_SURFACE_IMAGE_BACKING_H_
