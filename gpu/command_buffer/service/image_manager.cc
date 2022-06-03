// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/image_manager.h"

#include <stdint.h>

#include "base/check.h"
#include "ui/gl/gl_image.h"

namespace gpu {
namespace gles2 {

ImageManager::ImageManager() = default;

ImageManager::~ImageManager() = default;

void ImageManager::AddImage(gl::GLImage* image, int32_t service_id) {
  DCHECK(images_.find(service_id) == images_.end());
  images_[service_id] = image;
}

void ImageManager::RemoveImage(int32_t service_id) {
  DCHECK(images_.find(service_id) != images_.end());
  images_.erase(service_id);
}

gl::GLImage* ImageManager::LookupImage(int32_t service_id) {
  GLImageMap::const_iterator iter = images_.find(service_id);
  if (iter != images_.end())
    return iter->second.get();

  return nullptr;
}

}  // namespace gles2
}  // namespace gpu
