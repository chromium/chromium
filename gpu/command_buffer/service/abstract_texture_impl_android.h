// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_ABSTRACT_TEXTURE_IMPL_ANDROID_H_
#define GPU_COMMAND_BUFFER_SERVICE_ABSTRACT_TEXTURE_IMPL_ANDROID_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/abstract_texture_android.h"
#include "gpu/gpu_gles2_export.h"

namespace gl {
class GLApi;
}  // namespace gl

namespace gpu {

namespace gles2 {
class Texture;
class TexturePassthrough;
}  // namespace gles2
// Implementation of AbstractTextureAndroid which creates gles2::Texture on
// current context.
class GPU_GLES2_EXPORT AbstractTextureImpl : public AbstractTextureAndroid {
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

  // AbstractTextureAndroid implementation.
  TextureBase* GetTextureBase() const override;
  void SetParameteri(GLenum pname, GLint param) override;
#if BUILDFLAG(IS_ANDROID)
  void BindToServiceId(GLuint service_id) override;
#endif
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE)
  void SetUnboundImage(gl::GLImage* image) override;
#elif !BUILDFLAG(IS_ANDROID)
  void SetBoundImage(gl::GLImage* image) override;
#endif
  gl::GLImage* GetImageForTesting() const override;
  void SetCleared() override;
  void SetCleanupCallback(CleanupCallback cb) override;
  void NotifyOnContextLost() override;

 private:
  bool have_context_ = true;
  raw_ptr<gles2::Texture, DanglingUntriaged> texture_;
  raw_ptr<gl::GLApi, DanglingUntriaged> api_ = nullptr;
};

// Implementation of AbstractTextureAndroid which creates
// gles2::TexturePassthrough on current context.
class GPU_GLES2_EXPORT AbstractTextureImplPassthrough
    : public AbstractTextureAndroid {
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

  // AbstractTextureAndroid implementation.
  TextureBase* GetTextureBase() const override;
  void SetParameteri(GLenum pname, GLint param) override;
#if BUILDFLAG(IS_ANDROID)
  void BindToServiceId(GLuint service_id) override;
#endif
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE)
  void SetUnboundImage(gl::GLImage* image) override;
#elif !BUILDFLAG(IS_ANDROID)
  void SetBoundImage(gl::GLImage* image) override;
#endif
  gl::GLImage* GetImageForTesting() const override;
  void SetCleared() override;
  void SetCleanupCallback(CleanupCallback cb) override;
  void NotifyOnContextLost() override;

 private:
  bool have_context_ = true;
  scoped_refptr<gles2::TexturePassthrough> texture_;
  raw_ptr<gl::GLApi, DanglingUntriaged> api_ = nullptr;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_ABSTRACT_TEXTURE_IMPL_ANDROID_H_
