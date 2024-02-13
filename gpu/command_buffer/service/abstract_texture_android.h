// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_ABSTRACT_TEXTURE_ANDROID_H_
#define GPU_COMMAND_BUFFER_SERVICE_ABSTRACT_TEXTURE_ANDROID_H_

#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/texture_base.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gfx/geometry/size.h"

// Forwardly declare a few GL types to avoid including GL header files.
typedef unsigned GLenum;
typedef int GLsizei;
typedef int GLint;
typedef unsigned int GLuint;

namespace gl {
class GLApi;
}  // namespace gl

namespace gpu {
namespace gles2 {
class Texture;
class TexturePassthrough;
}  // namespace gles2

// An AbstractTextureAndroid enables access to GL textures from the GPU process,
// for things that set up textures using some client's decoder.  Creating an
// AbstractTextureAndroid is similar to "glGenTexture", and deleting it is
// similar to calling "glDeleteTextures".
class GPU_GLES2_EXPORT AbstractTextureAndroid final {
 public:
  static std::unique_ptr<AbstractTextureAndroid> CreateForValidating(
      gfx::Size size);
  static std::unique_ptr<AbstractTextureAndroid> CreateForPassthrough(
      gfx::Size size);
  static std::unique_ptr<AbstractTextureAndroid> CreateForTesting(
      GLuint texture_id);

  explicit AbstractTextureAndroid(std::unique_ptr<TextureBase> texture);
  explicit AbstractTextureAndroid(gles2::Texture* texture);
  explicit AbstractTextureAndroid(
      scoped_refptr<gles2::TexturePassthrough> texture,
      const gfx::Size& size);

  // The texture is guaranteed to be around while |this| exists, as long as
  // the decoder isn't destroyed / context isn't lost.
  ~AbstractTextureAndroid();

  // Return our TextureBase, useful mostly for creating a mailbox.  This may
  // return null if the texture has been destroyed.
  TextureBase* GetTextureBase() const;

  // Binds the texture to |service_id|. This will do nothing if the texture has
  // been destroyed.
  //
  // It is not required to SetCleared() if one calls this method.
  //
  // The context must be current.
  void BindToServiceId(GLuint service_id);

  // Used to notify the AbstractTexture if the context is lost.
  void NotifyOnContextLost();

  unsigned int service_id() const { return GetTextureBase()->service_id(); }

  base::WeakPtr<AbstractTextureAndroid> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  bool have_context_ = true;

  std::unique_ptr<TextureBase> texture_for_testing_;
  raw_ptr<gles2::Texture> texture_ = nullptr;
  scoped_refptr<gles2::TexturePassthrough> texture_passthrough_;
  gfx::Size texture_passthrough_size_;
  raw_ptr<gl::GLApi, DanglingUntriaged> api_ = nullptr;
  base::WeakPtrFactory<AbstractTextureAndroid> weak_ptr_factory_{this};
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_ABSTRACT_TEXTURE_ANDROID_H_
