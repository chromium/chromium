// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_ABSTRACT_TEXTURE_IMPL_H_
#define GPU_COMMAND_BUFFER_SERVICE_ABSTRACT_TEXTURE_IMPL_H_

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/abstract_texture.h"
#include "gpu/gpu_gles2_export.h"

namespace gl {
class GLApi;
}  // namespace gl

namespace gpu {

namespace gles2 {
class Texture;
class TexturePassthrough;

// Implementation of AbstractTexture which creates gles2::Texture on current
// context.
class GPU_GLES2_EXPORT AbstractTextureImpl : public AbstractTexture {
 public:
  AbstractTextureImpl(GLenum target,
                      GLenum internal_format,
                      GLsizei width,
                      GLsizei height,
                      GLsizei depth,
                      GLint border,
                      GLenum format,
                      GLenum type);
  ~AbstractTextureImpl() override;

  // AbstractTexture implementation.
  TextureBase* GetTextureBase() const override;
  void SetParameteri(GLenum pname, GLint param) override;
#if BUILDFLAG(IS_ANDROID)
  void BindStreamTextureImage(gl::GLImage* image, GLuint service_id) override;
#endif
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  void SetUnboundImage(gl::GLImage* image) override;
#else
  void SetBoundImage(gl::GLImage* image) override;
#endif
  gl::GLImage* GetImageForTesting() const override;
  void SetCleared() override;
  void SetCleanupCallback(CleanupCallback cb) override;
  void NotifyOnContextLost() override;

 private:
  bool have_context_ = true;
  raw_ptr<Texture, DanglingUntriaged> texture_;
  raw_ptr<gl::GLApi, DanglingUntriaged> api_ = nullptr;
};

// Implementation of AbstractTexture which creates gles2::TexturePassthrough on
// current context.
class GPU_GLES2_EXPORT AbstractTextureImplPassthrough : public AbstractTexture {
 public:
  AbstractTextureImplPassthrough(GLenum target,
                                 GLenum internal_format,
                                 GLsizei width,
                                 GLsizei height,
                                 GLsizei depth,
                                 GLint border,
                                 GLenum format,
                                 GLenum type);
  ~AbstractTextureImplPassthrough() override;

  // AbstractTexture implementation.
  TextureBase* GetTextureBase() const override;
  void SetParameteri(GLenum pname, GLint param) override;
#if BUILDFLAG(IS_ANDROID)
  void BindStreamTextureImage(gl::GLImage* image, GLuint service_id) override;
#endif
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  void SetUnboundImage(gl::GLImage* image) override;
#else
  void SetBoundImage(gl::GLImage* image) override;
#endif
  gl::GLImage* GetImageForTesting() const override;
  void SetCleared() override;
  void SetCleanupCallback(CleanupCallback cb) override;
  void NotifyOnContextLost() override;

 private:
  bool have_context_ = true;
  scoped_refptr<TexturePassthrough> texture_;
  raw_ptr<gl::GLApi, DanglingUntriaged> api_ = nullptr;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_ABSTRACT_TEXTURE_IMPL_H_
