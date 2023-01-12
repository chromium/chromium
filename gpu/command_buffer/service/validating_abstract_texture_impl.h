// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_VALIDATING_ABSTRACT_TEXTURE_IMPL_H_
#define GPU_COMMAND_BUFFER_SERVICE_VALIDATING_ABSTRACT_TEXTURE_IMPL_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/abstract_texture.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {
class DecoderContext;

namespace gles2 {

class ContextGroup;
class ErrorState;
class TextureManager;
class TextureRef;

// Implementation of AbstractTexture used by the validating command decoder.
class GPU_GLES2_EXPORT ValidatingAbstractTextureImpl : public AbstractTexture {
 public:
  using DestructionCB = base::OnceCallback<void(ValidatingAbstractTextureImpl*,
                                                scoped_refptr<TextureRef>)>;

  ValidatingAbstractTextureImpl(scoped_refptr<TextureRef> texture_ref,
                                DecoderContext* DecoderContext,
                                DestructionCB destruction_cb);
  ~ValidatingAbstractTextureImpl() override;

  // AbstractTexture
  TextureBase* GetTextureBase() const override;
  void SetParameteri(GLenum pname, GLint param) override;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  void SetUnboundImage(gl::GLImage* image) override;
#else
  void SetBoundImage(gl::GLImage* image) override;
#endif
  gl::GLImage* GetImageForTesting() const override;
  void SetCleared() override;
  void SetCleanupCallback(CleanupCallback cb) override;
  void NotifyOnContextLost() override;

  // Called when our decoder is going away, so that we can try to clean up.
  void OnDecoderWillDestroy(bool have_context);

  // Testing methods
  TextureRef* GetTextureRefForTesting();

 private:
  TextureManager* GetTextureManager() const;
  ContextGroup* GetContextGroup() const;
  ErrorState* GetErrorState() const;

  // Reset our level (0) info to be what we were given during construction.
  void SetLevelInfo();

  scoped_refptr<TextureRef> texture_ref_;

  raw_ptr<DecoderContext> decoder_context_ = nullptr;
  DestructionCB destruction_cb_;
  CleanupCallback cleanup_cb_;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_VALIDATING_ABSTRACT_TEXTURE_IMPL_H_
