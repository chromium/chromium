// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_WEBGL_FRAME_TRANSPORT_CONTEXT_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_WEBGL_FRAME_TRANSPORT_CONTEXT_IMPL_H_

#include "third_party/blink/renderer/platform/graphics/gpu/xr_webgl_frame_transport_delegate.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class WebGLRenderingContextBase;

class XRWebGLFrameTransportContextImpl final
    : public GarbageCollected<XRWebGLFrameTransportContextImpl>,
      public XRWebGLFrameTransportContext {
 public:
  explicit XRWebGLFrameTransportContextImpl(
      WebGLRenderingContextBase* webgl_context);
  ~XRWebGLFrameTransportContextImpl() override;

  gpu::gles2::GLES2Interface* ContextGL() const override;
  gpu::SharedImageInterface* SharedImageInterface() const override;
  DrawingBuffer::Client* GetDrawingBufferClient() const override;

  void Trace(Visitor* visitor) const override;

 private:
  Member<WebGLRenderingContextBase> webgl_context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_WEBGL_FRAME_TRANSPORT_CONTEXT_IMPL_H_
