// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_IOSURFACE_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_IOSURFACE_H_

#include <memory>

#include "base/macros.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gl/gl_image.h"

namespace gpu {

// Helper functions used used by SharedImageRepresentationGLImage to do
// IOSurface-specific sharing.
class GPU_GLES2_EXPORT SharedImageBackingFactoryIOSurface {
 public:
  static sk_sp<SkPromiseImageTexture> ProduceSkiaPromiseTextureMetal(
      SharedImageBacking* backing,
      scoped_refptr<SharedContextState> context_state,
      scoped_refptr<gl::GLImage> image);
  static std::unique_ptr<SharedImageRepresentationDawn> ProduceDawn(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      WGPUDevice device,
      scoped_refptr<gl::GLImage> image);
  static bool InitializePixels(SharedImageBacking* backing,
                               scoped_refptr<gl::GLImage> image,
                               const uint8_t* pixel_data);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_IOSURFACE_H_
