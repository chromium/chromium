// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_PASSTHROUGH_ABSTRACT_TEXTURE_IMPL_H_
#define GPU_COMMAND_BUFFER_SERVICE_PASSTHROUGH_ABSTRACT_TEXTURE_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/abstract_texture.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_passthrough.h"

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/service/texture_manager.h"

namespace gpu {
namespace gles2 {

class TexturePassthrough;
class GLES2DecoderPassthroughImpl;

// Implementation of AbstractTexture used by the passthrough command decoder.
class GPU_GLES2_EXPORT PassthroughAbstractTextureImpl : public AbstractTexture {
 public:
  PassthroughAbstractTextureImpl(
      scoped_refptr<TexturePassthrough> texture_passthrough,
      GLES2DecoderPassthroughImpl* decoder);
  ~PassthroughAbstractTextureImpl() override;

  // AbstractTexture
  TextureBase* GetTextureBase() const override;
  void SetParameteri(GLenum pname, GLint param) override;
#if BUILDFLAG(IS_OZONE)
  void SetBoundImage(GLImageNativePixmap* image) override;
#endif

  void SetCleared() override;
  void SetCleanupCallback(CleanupCallback cb) override;
  void NotifyOnContextLost() override;

  // Called when our decoder is going away, so that we can try to clean up.
  scoped_refptr<TexturePassthrough> OnDecoderWillDestroy();

 private:
  scoped_refptr<TexturePassthrough> texture_passthrough_;
  raw_ptr<gl::GLApi> gl_api_;
  raw_ptr<GLES2DecoderPassthroughImpl> decoder_;
  CleanupCallback cleanup_cb_;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_PASSTHROUGH_ABSTRACT_TEXTURE_IMPL_H_
