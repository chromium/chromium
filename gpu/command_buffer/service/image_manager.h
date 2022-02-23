// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_IMAGE_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_IMAGE_MANAGER_H_

#include <stdint.h>

#include <unordered_map>

#include "base/memory/ref_counted.h"
#include "gpu/gpu_export.h"

namespace gl {
class GLImage;
}

namespace gpu {
namespace gles2 {

// This class keeps track of the images and their state.
class GPU_EXPORT ImageManager {
 public:
  ImageManager();

  ImageManager(const ImageManager&) = delete;
  ImageManager& operator=(const ImageManager&) = delete;

  ~ImageManager();

  void AddImage(gl::GLImage* image, int32_t service_id);
  void RemoveImage(int32_t service_id);
  gl::GLImage* LookupImage(int32_t service_id);

 private:
  typedef std::unordered_map<int32_t, scoped_refptr<gl::GLImage>> GLImageMap;
  GLImageMap images_;
};

}  // namespage gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_IMAGE_MANAGER_H_
