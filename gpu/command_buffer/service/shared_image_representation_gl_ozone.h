// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_REPRESENTATION_GL_OZONE_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_REPRESENTATION_GL_OZONE_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_backing_ozone.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gfx/native_pixmap.h"

namespace gpu {

// Representation of an Ozone-backed SharedImage that can be accessed as a
// GL texture.
class SharedImageRepresentationGLOzone
    : public SharedImageRepresentationGLTexture {
 public:
  // Creates and initializes a SharedImageRepresentationGLOzone. On failure,
  // returns nullptr.
  static std::unique_ptr<SharedImageRepresentationGLOzone> Create(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      scoped_refptr<gfx::NativePixmap> pixmap,
      viz::ResourceFormat format);

  ~SharedImageRepresentationGLOzone() override;

  // SharedImageRepresentationGLTexture implementation.
  gles2::Texture* GetTexture() override;
  bool BeginAccess(GLenum mode) override;
  void EndAccess() override;

 private:
  SharedImageRepresentationGLOzone(SharedImageManager* manager,
                                   SharedImageBacking* backing,
                                   MemoryTypeTracker* tracker,
                                   gles2::Texture* texture);

  SharedImageBackingOzone* ozone_backing() {
    return static_cast<SharedImageBackingOzone*>(backing());
  }

  gles2::Texture* texture_;
  GLenum current_access_mode_ = 0;

  DISALLOW_COPY_AND_ASSIGN(SharedImageRepresentationGLOzone);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_REPRESENTATION_GL_OZONE_H_
