// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_PASSTHROUGH_ABSTRACT_TEXTURE_IMPL_H_
#define GPU_COMMAND_BUFFER_SERVICE_PASSTHROUGH_ABSTRACT_TEXTURE_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/abstract_texture.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_passthrough.h"

#include "base/callback.h"
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
  scoped_refptr<TexturePassthrough> OnDecoderWillDestroy();

 private:
  // Attaches |image| to |texture_passthrough_|, setting |texture_passthrough_|
  // as needing binding if |client_managed| is false. Releases any previous
  // image if *that* image was not client-managed.
  // NOTE: |client_managed| must be false on Windows/Mac and true on all other
  // platforms.
  void BindImageInternal(gl::GLImage* image, bool client_managed);

  scoped_refptr<TexturePassthrough> texture_passthrough_;
  bool decoder_managed_image_ = false;
  raw_ptr<gl::GLApi> gl_api_;
  raw_ptr<GLES2DecoderPassthroughImpl> decoder_;
  CleanupCallback cleanup_cb_;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_PASSTHROUGH_ABSTRACT_TEXTURE_IMPL_H_
