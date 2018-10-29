// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_REPRESENTATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_REPRESENTATION_H_

#include "base/callback_helpers.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/gpu_gles2_export.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

class GrContext;

namespace gpu {
namespace gles2 {
class Texture;
class TexturePassthrough;
}  // namespace gles2

// A representation of a SharedImageBacking for use with a specific use case /
// api.
class GPU_GLES2_EXPORT SharedImageRepresentation {
 public:
  SharedImageRepresentation(SharedImageManager* manager,
                            SharedImageBacking* backing);
  virtual ~SharedImageRepresentation();

  viz::ResourceFormat format() const { return backing_->format(); }
  const gfx::Size& size() const { return backing_->size(); }
  const gfx::ColorSpace& color_space() const { return backing_->color_space(); }
  uint32_t usage() const { return backing_->usage(); }
  bool IsCleared() const { return backing_->IsCleared(); }
  void SetCleared() { backing_->SetCleared(); }

  // Indicates that the underlying graphics context has been lost, and the
  // backing should be treated as destroyed.
  void OnContextLost() { backing_->OnContextLost(); }

 protected:
  SharedImageBacking* backing() { return backing_; }

 private:
  SharedImageManager* manager_;
  SharedImageBacking* backing_;
};

class SharedImageRepresentationGLTexture : public SharedImageRepresentation {
 public:
  SharedImageRepresentationGLTexture(SharedImageManager* manager,
                                     SharedImageBacking* backing)
      : SharedImageRepresentation(manager, backing) {}

  virtual gles2::Texture* GetTexture() = 0;
};

class SharedImageRepresentationGLTexturePassthrough
    : public SharedImageRepresentation {
 public:
  SharedImageRepresentationGLTexturePassthrough(SharedImageManager* manager,
                                                SharedImageBacking* backing)
      : SharedImageRepresentation(manager, backing) {}

  virtual const scoped_refptr<gles2::TexturePassthrough>&
  GetTexturePassthrough() = 0;
};

class SharedImageRepresentationSkia : public SharedImageRepresentation {
 public:
  SharedImageRepresentationSkia(SharedImageManager* manager,
                                SharedImageBacking* backing)
      : SharedImageRepresentation(manager, backing) {}

  virtual sk_sp<SkSurface> BeginWriteAccess(
      GrContext* gr_context,
      int final_msaa_count,
      SkColorType color_type,
      const SkSurfaceProps& surface_props) = 0;
  virtual void EndWriteAccess(sk_sp<SkSurface> surface) = 0;
  virtual bool BeginReadAccess(SkColorType color_type,
                               GrBackendTexture* backend_texture_out) = 0;
  virtual void EndReadAccess() = 0;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_REPRESENTATION_H_
