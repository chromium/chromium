// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_TEXTURE_BASE_H_
#define GPU_COMMAND_BUFFER_SERVICE_TEXTURE_BASE_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "gpu/gpu_export.h"

namespace gpu {

class GPU_EXPORT TextureBase {
 public:
  explicit TextureBase(unsigned int service_id);
  virtual ~TextureBase();

  // The service side OpenGL id of the texture.
  unsigned int service_id() const { return service_id_; }

  // Returns the target this texure was first bound to or 0 if it has not
  // been bound. Once a texture is bound to a specific target it can never be
  // bound to a different target.
  unsigned int target() const { return target_; }

  // An identifier for subclasses. Necessary for safe downcasting.
  enum class Type { kNone, kValidated, kPassthrough };
  virtual Type GetType() const;

 protected:
  // The id of the texture.
  unsigned int service_id_;

  // The target. 0 if unset, otherwise GL_TEXTURE_2D or GL_TEXTURE_CUBE_MAP.
  //             Or GL_TEXTURE_2D_ARRAY or GL_TEXTURE_3D (for GLES3).
  //             Or GL_TEXTURE_EXTERNAL_OES for YUV textures.
  unsigned int target_;

  void SetTarget(unsigned int target);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_TEXTURE_BASE_H_
