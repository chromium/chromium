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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_FRAMEBUFFER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_FRAMEBUFFER_H_

#include "third_party/blink/renderer/modules/webgl/webgl_context_object.h"
#include "third_party/blink/renderer/modules/webgl/webgl_shared_object.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace blink {

class WebGLRenderbuffer;
class WebGLTexture;

class WebGLFramebuffer final : public WebGLContextObject {
  DEFINE_WRAPPERTYPEINFO();

 public:
  class WebGLAttachment : public GarbageCollected<WebGLAttachment>,
                          public NameClient {
   public:
    virtual WebGLSharedObject* Object() const = 0;
    virtual bool IsSharedObject(WebGLSharedObject*) const = 0;
    virtual bool Valid() const = 0;
    virtual void OnDetached(gpu::gles2::GLES2Interface*) = 0;
    virtual void Attach(gpu::gles2::GLES2Interface*,
                        GLenum target,
                        GLenum attachment) = 0;
    virtual void Unattach(gpu::gles2::GLES2Interface*,
                          GLenum target,
                          GLenum attachment) = 0;

    virtual void Trace(blink::Visitor* visitor) {}

   protected:
    WebGLAttachment();
  };

  explicit WebGLFramebuffer(WebGLRenderingContextBase*, bool opaque);
  ~WebGLFramebuffer() override;

  static WebGLFramebuffer* Create(WebGLRenderingContextBase*);

  // An opaque framebuffer is one whose attachments are created and managed by
  // the browser and not inspectable or alterable via Javascript. This is
  // primarily used by the VRWebGLLayer interface.
  static WebGLFramebuffer* CreateOpaque(WebGLRenderingContextBase*);

  GLuint Object() const { return object_; }

  // For a non-multiview attachment, set the num_views parameter to 0. For a
  // multiview attachment, set the layer to the base view index.
  void SetAttachmentForBoundFramebuffer(GLenum target,
                                        GLenum attachment,
                                        GLenum tex_target,
                                        WebGLTexture*,
                                        GLint level,
                                        GLint layer,
                                        GLsizei num_views);
  void SetAttachmentForBoundFramebuffer(GLenum target,
                                        GLenum attachment,
                                        WebGLRenderbuffer*);
  // If an object is attached to the currently bound framebuffer, remove it.
  void RemoveAttachmentFromBoundFramebuffer(GLenum target, WebGLSharedObject*);
  WebGLSharedObject* GetAttachmentObject(GLenum) const;

  // WebGL 1 specific:
  //   1) can't allow depth_stencil for depth/stencil attachments, and vice
  //      versa.
  //   2) no conflicting DEPTH/STENCIL/DEPTH_STENCIL attachments.
  GLenum CheckDepthStencilStatus(const char** reason) const;

  bool HasEverBeenBound() const { return Object() && has_ever_been_bound_; }

  void SetHasEverBeenBound() { has_ever_been_bound_ = true; }

  bool HasStencilBuffer() const;

  bool HaveContentsChanged() { return contents_changed_; }
  void SetContentsChanged(bool changed) { contents_changed_ = changed; }

  bool Opaque() const { return opaque_; }
  void MarkOpaqueBufferComplete(bool complete) { opaque_complete_ = complete; }

  // Wrapper for drawBuffersEXT/drawBuffersARB to work around a driver bug.
  void DrawBuffers(const Vector<GLenum>& bufs);

  GLenum GetDrawBuffer(GLenum);

  void ReadBuffer(const GLenum color_buffer) { read_buffer_ = color_buffer; }

  GLenum GetReadBuffer() const { return read_buffer_; }

  void Trace(blink::Visitor*) override;
  const char* NameInHeapSnapshot() const override { return "WebGLFramebuffer"; }

 protected:
  bool HasObject() const override { return object_ != 0; }
  void DeleteObjectImpl(gpu::gles2::GLES2Interface*) override;

 private:
  WebGLAttachment* GetAttachment(GLenum attachment) const;

  // Check if the framebuffer is currently bound.
  bool IsBound(GLenum target) const;

  // Check if a new drawBuffers call should be issued. This is called when we
  // add or remove an attachment.
  void DrawBuffersIfNecessary(bool force);

  void SetAttachmentInternal(GLenum target,
                             GLenum attachment,
                             GLenum tex_target,
                             WebGLTexture*,
                             GLint level,
                             GLint layer);
  void SetAttachmentInternal(GLenum target,
                             GLenum attachment,
                             WebGLRenderbuffer*);
  // If a given attachment point for the currently bound framebuffer is not
  // null, remove the attached object.
  void RemoveAttachmentInternal(GLenum target, GLenum attachment);

  void CommitWebGL1DepthStencilIfConsistent(GLenum target);

  GLuint object_;

  typedef HeapHashMap<GLenum, Member<WebGLAttachment>> AttachmentMap;

  AttachmentMap attachments_;

  bool has_ever_been_bound_;
  bool web_gl1_depth_stencil_consistent_;
  bool contents_changed_ = false;
  const bool opaque_;
  bool opaque_complete_ = false;

  Vector<GLenum> draw_buffers_;
  Vector<GLenum> filtered_draw_buffers_;

  GLenum read_buffer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_FRAMEBUFFER_H_
