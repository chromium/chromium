// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_ABSTRACT_TEXTURE_ANDROID_H_
#define GPU_COMMAND_BUFFER_SERVICE_ABSTRACT_TEXTURE_ANDROID_H_

#include "build/build_config.h"
#include "gpu/command_buffer/service/texture_base.h"
#include "gpu/gpu_gles2_export.h"

// Forwardly declare a few GL types to avoid including GL header files.
typedef unsigned GLenum;
typedef int GLsizei;
typedef int GLint;
typedef unsigned int GLuint;

namespace gpu {

// An AbstractTexture enables access to GL textures from the GPU process, for
// things that set up textures using some client's decoder.  Creating an
// AbstractTexture is similar to "glGenTexture", and deleting it is similar to
// calling "glDeleteTextures".
class GPU_GLES2_EXPORT AbstractTextureAndroid {
 public:
  // The texture is guaranteed to be around while |this| exists, as long as
  // the decoder isn't destroyed / context isn't lost.
  virtual ~AbstractTextureAndroid() = default;

  // Return our TextureBase, useful mostly for creating a mailbox.  This may
  // return null if the texture has been destroyed.
  virtual TextureBase* GetTextureBase() const = 0;

  // Binds the texture to |service_id|. This will do nothing if the texture has
  // been destroyed.
  //
  // It is not required to SetCleared() if one calls this method.
  //
  // The context must be current.
  virtual void BindToServiceId(GLuint service_id) = 0;

  // Used to notify the AbstractTexture if the context is lost.
  virtual void NotifyOnContextLost() = 0;

  unsigned int service_id() const { return GetTextureBase()->service_id(); }
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_ABSTRACT_TEXTURE_ANDROID_H_
