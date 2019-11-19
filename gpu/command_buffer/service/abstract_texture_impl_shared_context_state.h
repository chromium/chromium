// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_ABSTRACT_TEXTURE_IMPL_SHARED_CONTEXT_STATE_H_
#define GPU_COMMAND_BUFFER_SERVICE_ABSTRACT_TEXTURE_IMPL_SHARED_CONTEXT_STATE_H_

#include "base/callback.h"
#include "gpu/command_buffer/service/abstract_texture.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {
class SharedContextState;

namespace gles2 {
class Texture;
class TexturePassthrough;

// Implementation of AbstractTexture which will be used to create
// AbstractTextures on ShareContextState.
class GPU_GLES2_EXPORT AbstractTextureImplOnSharedContext
    : public AbstractTexture,
      public SharedContextState::ContextLostObserver {
 public:
  AbstractTextureImplOnSharedContext(
      GLenum target,
      GLenum internal_format,
      GLsizei width,
      GLsizei height,
      GLsizei depth,
      GLint border,
      GLenum format,
      GLenum type,
      scoped_refptr<gpu::SharedContextState> shared_context_state);
  ~AbstractTextureImplOnSharedContext() override;

  // AbstractTexture implementation.
  TextureBase* GetTextureBase() const override;
  void SetParameteri(GLenum pname, GLint param) override;
  void BindStreamTextureImage(GLStreamTextureImage* image,
                              GLuint service_id) override;
  void BindImage(gl::GLImage* image, bool client_managed) override;
  gl::GLImage* GetImage() const override;
  void SetCleared() override;
  void SetCleanupCallback(CleanupCallback cb) override;

  // SharedContextState::ContextLostObserver implementation.
  void OnContextLost() override;

 private:
  Texture* texture_;
  scoped_refptr<SharedContextState> shared_context_state_;
  CleanupCallback cleanup_cb_;
};

// Implementation of AbstractTexture which will be used to create
// AbstractTextures on SharedContextState for the passthrough command decoder.
class GPU_GLES2_EXPORT AbstractTextureImplOnSharedContextPassthrough
    : public AbstractTexture,
      public SharedContextState::ContextLostObserver {
 public:
  AbstractTextureImplOnSharedContextPassthrough(
      GLenum target,
      scoped_refptr<gpu::SharedContextState> shared_context_state);
  ~AbstractTextureImplOnSharedContextPassthrough() override;

  // AbstractTexture implementation.
  TextureBase* GetTextureBase() const override;
  void SetParameteri(GLenum pname, GLint param) override;
  void BindStreamTextureImage(GLStreamTextureImage* image,
                              GLuint service_id) override;
  void BindImage(gl::GLImage* image, bool client_managed) override;
  gl::GLImage* GetImage() const override;
  void SetCleared() override;
  void SetCleanupCallback(CleanupCallback cb) override;

  // SharedContextState::ContextLostObserver implementation.
  void OnContextLost() override;

 private:
  scoped_refptr<TexturePassthrough> texture_;
  scoped_refptr<SharedContextState> shared_context_state_;
  CleanupCallback cleanup_cb_;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_ABSTRACT_TEXTURE_IMPL_SHARED_CONTEXT_STATE_H_
