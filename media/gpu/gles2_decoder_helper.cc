// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/gles2_decoder_helper.h"

#include <memory>

#include "base/check_op.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/decoder_context.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/scoped_binders.h"

using gpu::gles2::AbstractTexture;

namespace media {

class GLES2DecoderHelperImpl : public GLES2DecoderHelper {
 public:
  explicit GLES2DecoderHelperImpl(gpu::DecoderContext* decoder)
      : decoder_(decoder) {
    DCHECK(decoder_);
    gpu::gles2::ContextGroup* group = decoder_->GetContextGroup();
    mailbox_manager_ = group->mailbox_manager();
    DCHECK(mailbox_manager_);
  }

  bool MakeContextCurrent() override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return decoder_->MakeCurrent();
  }

  std::unique_ptr<AbstractTexture> CreateTexture(GLenum target,
                                                 GLenum internal_format,
                                                 GLsizei width,
                                                 GLsizei height,
                                                 GLenum format,
                                                 GLenum type) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(decoder_->GetGLContext()->IsCurrent(nullptr));

    std::unique_ptr<AbstractTexture> texture =
        decoder_->CreateAbstractTexture(target, internal_format, width, height,
                                        1,  // depth
                                        0,  // border
                                        format, type);

    texture->SetParameteri(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    texture->SetParameteri(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    texture->SetParameteri(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    texture->SetParameteri(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // TODO(sandersd): Do we always want to allocate for GL_TEXTURE_2D?
    if (target == GL_TEXTURE_2D) {
      gl::ScopedTextureBinder scoped_binder(target, texture->service_id());
      glTexImage2D(target,           // target
                   0,                // level
                   internal_format,  // internal_format
                   width,            // width
                   height,           // height
                   0,                // border
                   format,           // format
                   type,             // type
                   nullptr);         // data
    }

    return texture;
  }

  gl::GLContext* GetGLContext() override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return decoder_->GetGLContext();
  }

  gpu::Mailbox CreateMailbox(AbstractTexture* texture) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    gpu::Mailbox mailbox = gpu::Mailbox::Generate();
    mailbox_manager_->ProduceTexture(mailbox, texture->GetTextureBase());
    return mailbox;
  }

  void ProduceTexture(const gpu::Mailbox& mailbox,
                      AbstractTexture* texture) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    mailbox_manager_->ProduceTexture(mailbox, texture->GetTextureBase());
  }

 private:
  gpu::DecoderContext* decoder_;
  gpu::MailboxManager* mailbox_manager_;
  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(GLES2DecoderHelperImpl);
};

// static
std::unique_ptr<GLES2DecoderHelper> GLES2DecoderHelper::Create(
    gpu::DecoderContext* decoder) {
  return std::make_unique<GLES2DecoderHelperImpl>(decoder);
}

}  // namespace media
