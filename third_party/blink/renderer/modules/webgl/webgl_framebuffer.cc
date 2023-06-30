/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/webgl/webgl_framebuffer.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/blink/renderer/modules/webgl/webgl_renderbuffer.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"
#include "third_party/blink/renderer/modules/webgl/webgl_texture.h"

namespace blink {

namespace {

const char kIncompleteOpaque[] =
    "Cannot render to a XRWebGLLayer framebuffer outside of an XRSession "
    "animation frame callback.";

class WebGLRenderbufferAttachment final
    : public WebGLFramebuffer::WebGLAttachment {
 public:
  explicit WebGLRenderbufferAttachment(WebGLRenderbuffer*);

  void Trace(Visitor*) const override;
  const char* NameInHeapSnapshot() const override { return "WebGLAttachment"; }

 private:
  WebGLSharedObject* Object() const override;
  bool IsSharedObject(WebGLSharedObject*) const override;
  bool Valid() const override;
  void OnDetached(gpu::gles2::GLES2Interface*) override;
  void Attach(gpu::gles2::GLES2Interface*,
              GLenum target,
              GLenum attachment) override;
  void Unattach(gpu::gles2::GLES2Interface*,
                GLenum target,
                GLenum attachment) override;

  Member<WebGLRenderbuffer> renderbuffer_;
};

void WebGLRenderbufferAttachment::Trace(Visitor* visitor) const {
  visitor->Trace(renderbuffer_);
  WebGLFramebuffer::WebGLAttachment::Trace(visitor);
}

WebGLRenderbufferAttachment::WebGLRenderbufferAttachment(
    WebGLRenderbuffer* renderbuffer)
    : renderbuffer_(renderbuffer) {}

WebGLSharedObject* WebGLRenderbufferAttachment::Object() const {
  return renderbuffer_->Object() ? renderbuffer_.Get() : nullptr;
}

bool WebGLRenderbufferAttachment::IsSharedObject(
    WebGLSharedObject* object) const {
  return object == renderbuffer_;
}

bool WebGLRenderbufferAttachment::Valid() const {
  return renderbuffer_->Object();
}

void WebGLRenderbufferAttachment::OnDetached(gpu::gles2::GLES2Interface* gl) {
  renderbuffer_->OnDetached(gl);
}

void WebGLRenderbufferAttachment::Attach(gpu::gles2::GLES2Interface* gl,
                                         GLenum target,
                                         GLenum attachment) {
  GLuint object = ObjectOrZero(renderbuffer_.Get());
  gl->FramebufferRenderbuffer(target, attachment, GL_RENDERBUFFER, object);
}

void WebGLRenderbufferAttachment::Unattach(gpu::gles2::GLES2Interface* gl,
                                           GLenum target,
                                           GLenum attachment) {
  gl->FramebufferRenderbuffer(target, attachment, GL_RENDERBUFFER, 0);
}

class WebGLTextureAttachment final : public WebGLFramebuffer::WebGLAttachment {
 public:
  WebGLTextureAttachment(WebGLTexture*,
                         GLenum target,
                         GLint level,
                         GLint layer);

  void Trace(Visitor*) const override;
  const char* NameInHeapSnapshot() const override {
    return "WebGLTextureAttachment";
  }

 private:
  WebGLSharedObject* Object() const override;
  bool IsSharedObject(WebGLSharedObject*) const override;
  bool Valid() const override;
  void OnDetached(gpu::gles2::GLES2Interface*) override;
  void Attach(gpu::gles2::GLES2Interface*,
              GLenum target,
              GLenum attachment) override;
  void Unattach(gpu::gles2::GLES2Interface*,
                GLenum target,
                GLenum attachment) override;

  Member<WebGLTexture> texture_;
  GLenum target_;
  GLint level_;
  GLint layer_;
};

void WebGLTextureAttachment::Trace(Visitor* visitor) const {
  visitor->Trace(texture_);
  WebGLFramebuffer::WebGLAttachment::Trace(visitor);
}

WebGLTextureAttachment::WebGLTextureAttachment(WebGLTexture* texture,
                                               GLenum target,
                                               GLint level,
                                               GLint layer)
    : texture_(texture), target_(target), level_(level), layer_(layer) {}

WebGLSharedObject* WebGLTextureAttachment::Object() const {
  return texture_->Object() ? texture_.Get() : nullptr;
}

bool WebGLTextureAttachment::IsSharedObject(WebGLSharedObject* object) const {
  return object == texture_;
}

bool WebGLTextureAttachment::Valid() const {
  return texture_->Object();
}

void WebGLTextureAttachment::OnDetached(gpu::gles2::GLES2Interface* gl) {
  texture_->OnDetached(gl);
}

void WebGLTextureAttachment::Attach(gpu::gles2::GLES2Interface* gl,
                                    GLenum target,
                                    GLenum attachment) {
  GLuint object = ObjectOrZero(texture_.Get());
  if (target_ == GL_TEXTURE_3D || target_ == GL_TEXTURE_2D_ARRAY) {
    gl->FramebufferTextureLayer(target, attachment, object, level_, layer_);
  } else {
    gl->FramebufferTexture2D(target, attachment, target_, object, level_);
  }
}

void WebGLTextureAttachment::Unattach(gpu::gles2::GLES2Interface* gl,
                                      GLenum target,
                                      GLenum attachment) {
  // GL_DEPTH_STENCIL_ATTACHMENT attachment is valid in ES3.
  if (target_ == GL_TEXTURE_3D || target_ == GL_TEXTURE_2D_ARRAY) {
    gl->FramebufferTextureLayer(target, attachment, 0, level_, layer_);
  } else {
    gl->FramebufferTexture2D(target, attachment, target_, 0, level_);
  }
}

}  // anonymous namespace

WebGLFramebuffer::WebGLAttachment::WebGLAttachment() = default;

WebGLFramebuffer* WebGLFramebuffer::CreateOpaque(WebGLRenderingContextBase* ctx,
                                                 bool has_depth,
                                                 bool has_stencil) {
  WebGLFramebuffer* const fb =
      MakeGarbageCollected<WebGLFramebuffer>(ctx, true);
  fb->SetOpaqueHasDepth(has_depth);
  fb->SetOpaqueHasStencil(has_stencil);
  return fb;
}

WebGLFramebuffer::WebGLFramebuffer(WebGLRenderingContextBase* ctx, bool opaque)
    : WebGLContextObject(ctx),
      object_(0),
      has_ever_been_bound_(false),
      web_gl1_depth_stencil_consistent_(true),
      opaque_(opaque),
      read_buffer_(GL_COLOR_ATTACHMENT0) {
  ctx->ContextGL()->GenFramebuffers(1, &object_);
}

WebGLFramebuffer::~WebGLFramebuffer() = default;

void WebGLFramebuffer::SetAttachmentForBoundFramebuffer(GLenum target,
                                                        GLenum attachment,
                                                        GLenum tex_target,
                                                        WebGLTexture* texture,
                                                        GLint level,
                                                        GLint layer,
                                                        GLsizei num_views) {
  DCHECK(object_);
  DCHECK(IsBound(target));
  if (Context()->IsWebGL2()) {
    if (attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
      SetAttachmentInternal(target, GL_DEPTH_ATTACHMENT, tex_target, texture,
                            level, layer);
      SetAttachmentInternal(target, GL_STENCIL_ATTACHMENT, tex_target, texture,
                            level, layer);
    } else {
      SetAttachmentInternal(target, attachment, tex_target, texture, level,
                            layer);
    }
    GLuint texture_id = ObjectOrZero(texture);
    // texTarget can be 0 if detaching using framebufferTextureLayer.
    DCHECK(tex_target || !texture_id);
    switch (tex_target) {
      case 0:
      case GL_TEXTURE_3D:
      case GL_TEXTURE_2D_ARRAY:
        if (num_views > 0) {
          DCHECK_EQ(static_cast<GLenum>(GL_TEXTURE_2D_ARRAY), tex_target);
          Context()->ContextGL()->FramebufferTextureMultiviewOVR(
              target, attachment, texture_id, level, layer, num_views);
        } else {
          Context()->ContextGL()->FramebufferTextureLayer(
              target, attachment, texture_id, level, layer);
        }
        break;
      default:
        DCHECK_EQ(layer, 0);
        DCHECK_EQ(num_views, 0);
        Context()->ContextGL()->FramebufferTexture2D(
            target, attachment, tex_target, texture_id, level);
        break;
    }
  } else {
    DCHECK_EQ(layer, 0);
    DCHECK_EQ(num_views, 0);
    SetAttachmentInternal(target, attachment, tex_target, texture, level,
                          layer);
    switch (attachment) {
      case GL_DEPTH_ATTACHMENT:
      case GL_STENCIL_ATTACHMENT:
      case GL_DEPTH_STENCIL_ATTACHMENT:
        CommitWebGL1DepthStencilIfConsistent(target);
        break;
      default:
        Context()->ContextGL()->FramebufferTexture2D(
            target, attachment, tex_target, ObjectOrZero(texture), level);
        break;
    }
  }
}

void WebGLFramebuffer::SetAttachmentForBoundFramebuffer(
    GLenum target,
    GLenum attachment,
    WebGLRenderbuffer* renderbuffer) {
  DCHECK(object_);
  DCHECK(IsBound(target));
  if (Context()->IsWebGL2()) {
    if (attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
      SetAttachmentInternal(target, GL_DEPTH_ATTACHMENT, renderbuffer);
      SetAttachmentInternal(target, GL_STENCIL_ATTACHMENT, renderbuffer);
    } else {
      SetAttachmentInternal(target, attachment, renderbuffer);
    }
    Context()->ContextGL()->FramebufferRenderbuffer(
        target, attachment, GL_RENDERBUFFER, ObjectOrZero(renderbuffer));
  } else {
    SetAttachmentInternal(target, attachment, renderbuffer);
    switch (attachment) {
      case GL_DEPTH_ATTACHMENT:
      case GL_STENCIL_ATTACHMENT:
      case GL_DEPTH_STENCIL_ATTACHMENT:
        CommitWebGL1DepthStencilIfConsistent(target);
        break;
      default:
        Context()->ContextGL()->FramebufferRenderbuffer(
            target, attachment, GL_RENDERBUFFER, ObjectOrZero(renderbuffer));
        break;
    }
  }
}

WebGLSharedObject* WebGLFramebuffer::GetAttachmentObject(
    GLenum attachment) const {
  if (!object_)
    return nullptr;
  WebGLAttachment* attachment_object = GetAttachment(attachment);
  return attachment_object ? attachment_object->Object() : nullptr;
}

WebGLFramebuffer::WebGLAttachment* WebGLFramebuffer::GetAttachment(
    GLenum attachment) const {
  const AttachmentMap::const_iterator it = attachments_.find(attachment);
  return (it != attachments_.end()) ? it->value.Get() : nullptr;
}

void WebGLFramebuffer::RemoveAttachmentFromBoundFramebuffer(
    GLenum target,
    WebGLSharedObject* attachment) {
  DCHECK(IsBound(target));
  if (!object_)
    return;
  if (!attachment)
    return;

  bool check_more = true;
  bool is_web_gl1 = !Context()->IsWebGL2();
  bool check_web_gl1_depth_stencil = false;
  while (check_more) {
    check_more = false;
    for (const auto& it : attachments_) {
      WebGLAttachment* attachment_object = it.value.Get();
      if (attachment_object->IsSharedObject(attachment)) {
        GLenum attachment_type = it.key;
        switch (attachment_type) {
          case GL_DEPTH_ATTACHMENT:
          case GL_STENCIL_ATTACHMENT:
          case GL_DEPTH_STENCIL_ATTACHMENT:
            if (is_web_gl1) {
              check_web_gl1_depth_stencil = true;
            } else {
              attachment_object->Unattach(Context()->ContextGL(), target,
                                          attachment_type);
            }
            break;
          default:
            attachment_object->Unattach(Context()->ContextGL(), target,
                                        attachment_type);
            break;
        }
        RemoveAttachmentInternal(target, attachment_type);
        check_more = true;
        break;
      }
    }
  }
  if (check_web_gl1_depth_stencil)
    CommitWebGL1DepthStencilIfConsistent(target);
}

GLenum WebGLFramebuffer::CheckDepthStencilStatus(const char** reason) const {
  // This function is called any time framebuffer completeness is checked, which
  // makes it the most convenient place to add this check.
  if (opaque_) {
    if (opaque_complete_)
      return GL_FRAMEBUFFER_COMPLETE;
    *reason = kIncompleteOpaque;
    return GL_FRAMEBUFFER_UNSUPPORTED;
  }
  if (Context()->IsWebGL2() || web_gl1_depth_stencil_consistent_)
    return GL_FRAMEBUFFER_COMPLETE;
  *reason = "conflicting DEPTH/STENCIL/DEPTH_STENCIL attachments";
  return GL_FRAMEBUFFER_UNSUPPORTED;
}

bool WebGLFramebuffer::HasDepthBuffer() const {
  if (opaque_) {
    return opaque_has_depth_;
  } else {
    WebGLAttachment* attachment = GetAttachment(GL_DEPTH_ATTACHMENT);
    if (!attachment) {
      attachment = GetAttachment(GL_DEPTH_STENCIL_ATTACHMENT);
    }
    return attachment && attachment->Valid();
  }
}

bool WebGLFramebuffer::HasStencilBuffer() const {
  if (opaque_) {
    return opaque_has_stencil_;
  } else {
    WebGLAttachment* attachment = GetAttachment(GL_STENCIL_ATTACHMENT);
    if (!attachment)
      attachment = GetAttachment(GL_DEPTH_STENCIL_ATTACHMENT);
    return attachment && attachment->Valid();
  }
}

void WebGLFramebuffer::DeleteObjectImpl(gpu::gles2::GLES2Interface* gl) {
  // Both the AttachmentMap and its WebGLAttachment objects are GCed
  // objects and cannot be accessed after the destructor has been
  // entered, as they may have been finalized already during the
  // same GC sweep. These attachments' OpenGL objects will be fully
  // destroyed once their JavaScript wrappers are collected.
  if (!DestructionInProgress()) {
    for (const auto& attachment : attachments_)
      attachment.value->OnDetached(gl);
    for (const auto& tex : pls_textures_) {
      tex.value->OnDetached(gl);
    }
  }

  gl->DeleteFramebuffers(1, &object_);
  object_ = 0;
}

bool WebGLFramebuffer::IsBound(GLenum target) const {
  return (Context()->GetFramebufferBinding(target) == this);
}

void WebGLFramebuffer::DrawBuffers(const Vector<GLenum>& bufs) {
  draw_buffers_ = bufs;
  filtered_draw_buffers_.resize(draw_buffers_.size());
  for (wtf_size_t i = 0; i < filtered_draw_buffers_.size(); ++i)
    filtered_draw_buffers_[i] = GL_NONE;
  DrawBuffersIfNecessary(true);
}

void WebGLFramebuffer::DrawBuffersIfNecessary(bool force) {
  if (Context()->IsWebGL2() ||
      Context()->ExtensionEnabled(kWebGLDrawBuffersName)) {
    bool reset = force;
    // This filtering works around graphics driver bugs on Mac OS X.
    for (wtf_size_t i = 0; i < draw_buffers_.size(); ++i) {
      if (draw_buffers_[i] != GL_NONE && GetAttachment(draw_buffers_[i])) {
        if (filtered_draw_buffers_[i] != draw_buffers_[i]) {
          filtered_draw_buffers_[i] = draw_buffers_[i];
          reset = true;
        }
      } else {
        if (filtered_draw_buffers_[i] != GL_NONE) {
          filtered_draw_buffers_[i] = GL_NONE;
          reset = true;
        }
      }
    }
    if (reset) {
      Context()->ContextGL()->DrawBuffersEXT(filtered_draw_buffers_.size(),
                                             filtered_draw_buffers_.data());
    }
  }
}

void WebGLFramebuffer::SetAttachmentInternal(GLenum target,
                                             GLenum attachment,
                                             GLenum tex_target,
                                             WebGLTexture* texture,
                                             GLint level,
                                             GLint layer) {
  DCHECK(IsBound(target));
  DCHECK(object_);
  RemoveAttachmentInternal(target, attachment);
  if (texture && texture->Object()) {
    attachments_.insert(attachment,
                        MakeGarbageCollected<WebGLTextureAttachment>(
                            texture, tex_target, level, layer));
    DrawBuffersIfNecessary(false);
    texture->OnAttached();
  }
}

void WebGLFramebuffer::SetAttachmentInternal(GLenum target,
                                             GLenum attachment,
                                             WebGLRenderbuffer* renderbuffer) {
  DCHECK(IsBound(target));
  DCHECK(object_);
  RemoveAttachmentInternal(target, attachment);
  if (renderbuffer && renderbuffer->Object()) {
    attachments_.insert(
        attachment,
        MakeGarbageCollected<WebGLRenderbufferAttachment>(renderbuffer));
    DrawBuffersIfNecessary(false);
    renderbuffer->OnAttached();
  }
}

void WebGLFramebuffer::RemoveAttachmentInternal(GLenum target,
                                                GLenum attachment) {
  DCHECK(IsBound(target));
  DCHECK(object_);

  WebGLAttachment* attachment_object = GetAttachment(attachment);
  if (attachment_object) {
    attachment_object->OnDetached(Context()->ContextGL());
    attachments_.erase(attachment);
    DrawBuffersIfNecessary(false);
  }
}

void WebGLFramebuffer::CommitWebGL1DepthStencilIfConsistent(GLenum target) {
  DCHECK(!Context()->IsWebGL2());
  WebGLAttachment* depth_attachment = nullptr;
  WebGLAttachment* stencil_attachment = nullptr;
  WebGLAttachment* depth_stencil_attachment = nullptr;
  int count = 0;
  for (const auto& it : attachments_) {
    WebGLAttachment* attachment = it.value.Get();
    DCHECK(attachment);
    switch (it.key) {
      case GL_DEPTH_ATTACHMENT:
        depth_attachment = attachment;
        ++count;
        break;
      case GL_STENCIL_ATTACHMENT:
        stencil_attachment = attachment;
        ++count;
        break;
      case GL_DEPTH_STENCIL_ATTACHMENT:
        depth_stencil_attachment = attachment;
        ++count;
        break;
      default:
        break;
    }
  }

  web_gl1_depth_stencil_consistent_ = count <= 1;
  if (!web_gl1_depth_stencil_consistent_)
    return;

  gpu::gles2::GLES2Interface* gl = Context()->ContextGL();
  if (depth_attachment) {
    gl->FramebufferRenderbuffer(target, GL_DEPTH_STENCIL_ATTACHMENT,
                                GL_RENDERBUFFER, 0);
    depth_attachment->Attach(gl, target, GL_DEPTH_ATTACHMENT);
    gl->FramebufferRenderbuffer(target, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                                0);
  } else if (stencil_attachment) {
    gl->FramebufferRenderbuffer(target, GL_DEPTH_STENCIL_ATTACHMENT,
                                GL_RENDERBUFFER, 0);
    gl->FramebufferRenderbuffer(target, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                                0);
    stencil_attachment->Attach(gl, target, GL_STENCIL_ATTACHMENT);
  } else if (depth_stencil_attachment) {
    gl->FramebufferRenderbuffer(target, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                                0);
    gl->FramebufferRenderbuffer(target, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                                0);
    depth_stencil_attachment->Attach(gl, target, GL_DEPTH_STENCIL_ATTACHMENT);
  } else {
    gl->FramebufferRenderbuffer(target, GL_DEPTH_STENCIL_ATTACHMENT,
                                GL_RENDERBUFFER, 0);
    gl->FramebufferRenderbuffer(target, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                                0);
    gl->FramebufferRenderbuffer(target, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                                0);
  }
}

GLenum WebGLFramebuffer::GetDrawBuffer(GLenum draw_buffer) {
  int index = static_cast<int>(draw_buffer - GL_DRAW_BUFFER0_EXT);
  DCHECK_GE(index, 0);
  if (index < static_cast<int>(draw_buffers_.size()))
    return draw_buffers_[index];
  if (draw_buffer == GL_DRAW_BUFFER0_EXT)
    return GL_COLOR_ATTACHMENT0;
  return GL_NONE;
}

// HeapHashMap does not allow keys with a value of 0.
constexpr static GLint PlaneKey(GLint plane) {
  return plane + 1;
}

void WebGLFramebuffer::SetPLSTexture(GLint plane, WebGLTexture* texture) {
  if (texture == nullptr) {
    pls_textures_.erase(PlaneKey(plane));
  } else {
    pls_textures_.Set(PlaneKey(plane), texture);
  }
}

WebGLTexture* WebGLFramebuffer::GetPLSTexture(GLint plane) const {
  const auto it = pls_textures_.find(PlaneKey(plane));
  return (it != pls_textures_.end()) ? it->value.Get() : nullptr;
}

void WebGLFramebuffer::Trace(Visitor* visitor) const {
  visitor->Trace(attachments_);
  visitor->Trace(pls_textures_);
  WebGLContextObject::Trace(visitor);
}

}  // namespace blink
