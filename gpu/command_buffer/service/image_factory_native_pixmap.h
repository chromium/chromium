// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_IMAGE_FACTORY_NATIVE_PIXMAP_H_
#define GPU_COMMAND_BUFFER_SERVICE_IMAGE_FACTORY_NATIVE_PIXMAP_H_

#include "base/memory/ref_counted.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/gpu_gles2_export.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gpu {

class GPU_GLES2_EXPORT ImageFactoryNativePixmap : public ImageFactory {
 public:
  ImageFactoryNativePixmap();

  ImageFactoryNativePixmap(const ImageFactoryNativePixmap&) = delete;
  ImageFactoryNativePixmap& operator=(const ImageFactoryNativePixmap&) = delete;

  ~ImageFactoryNativePixmap() override;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_IMAGE_FACTORY_NATIVE_PIXMAP_H_
