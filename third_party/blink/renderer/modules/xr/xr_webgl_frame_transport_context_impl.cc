// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_webgl_frame_transport_context_impl.h"

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

XRWebGLFrameTransportContextImpl::XRWebGLFrameTransportContextImpl(
    WebGLRenderingContextBase* webgl_context)
    : webgl_context_(webgl_context) {}

XRWebGLFrameTransportContextImpl::~XRWebGLFrameTransportContextImpl() = default;

gpu::gles2::GLES2Interface* XRWebGLFrameTransportContextImpl::ContextGL()
    const {
  if (!webgl_context_) {
    return nullptr;
  }
  return webgl_context_->ContextGL();
}

gpu::SharedImageInterface*
XRWebGLFrameTransportContextImpl::SharedImageInterface() const {
  if (!webgl_context_) {
    return nullptr;
  }
  return webgl_context_->SharedImageInterface();
}

DrawingBuffer::Client*
XRWebGLFrameTransportContextImpl::GetDrawingBufferClient() const {
  if (!webgl_context_) {
    return nullptr;
  }
  return static_cast<DrawingBuffer::Client*>(webgl_context_.Get());
}

void XRWebGLFrameTransportContextImpl::Trace(Visitor* visitor) const {
  visitor->Trace(webgl_context_);
  XRWebGLFrameTransportContext::Trace(visitor);
}

}  // namespace blink
