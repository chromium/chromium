// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_webgl_drawing_context.h"

#include "base/functional/callback_helpers.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"
#include "third_party/blink/renderer/modules/xr/xr_layer_shared_image_manager.h"
#include "third_party/blink/renderer/modules/xr/xr_webgl_binding.h"
#include "third_party/blink/renderer/modules/xr/xr_webgl_frame_transport_context_impl.h"
#include "third_party/blink/renderer/modules/xr/xr_webgl_swap_chain.h"
#include "third_party/blink/renderer/platform/graphics/gpu/xr_webgl_drawing_buffer.h"
#include "third_party/blink/renderer/platform/graphics/gpu/xr_webgl_frame_transport_delegate.h"

namespace blink {

XRWebGLDrawingContext::XRWebGLDrawingContext(
    XRWebGLBinding* binding,
    XRWebGLSwapChain* color_swap_chain,
    XRWebGLSwapChain* depth_stencil_swap_chain)
    : webgl_context_(binding->context()),
      color_swap_chain_(color_swap_chain),
      depth_stencil_swap_chain_(depth_stencil_swap_chain),
      binding_(binding) {
  CHECK(color_swap_chain_);
  CHECK(binding_);
}

void XRWebGLDrawingContext::SetCompositionLayer(XRCompositionLayer* layer) {
  color_swap_chain_->SetLayer(layer);
  if (depth_stencil_swap_chain_) {
    depth_stencil_swap_chain_->SetLayer(layer);
  }
}

uint16_t XRWebGLDrawingContext::TextureWidth() const {
  return color_swap_chain_->descriptor().width;
}

uint16_t XRWebGLDrawingContext::TextureHeight() const {
  return color_swap_chain_->descriptor().height;
}

uint16_t XRWebGLDrawingContext::TextureArrayLength() const {
  return color_swap_chain_->descriptor().layers;
}

bool XRWebGLDrawingContext::TextureWasQueried() const {
  return color_swap_chain_->texture_was_queried();
}

void XRWebGLDrawingContext::OnFrameStart() {
  color_swap_chain_->OnFrameStart();
  if (depth_stencil_swap_chain_) {
    depth_stencil_swap_chain_->OnFrameStart();
  }
}

void XRWebGLDrawingContext::OnFrameEnd() {
  color_swap_chain_->OnFrameEnd();
  if (depth_stencil_swap_chain_) {
    depth_stencil_swap_chain_->OnFrameEnd();
  }
}

XRSession* XRWebGLDrawingContext::session() const {
  return color_swap_chain_->layer()->session();
}

std::unique_ptr<SharedImageHolder>
XRWebGLDrawingContext::TransferToSharedImageHolder() {
  return color_swap_chain_->TransferToSharedImageHolder();
}

std::unique_ptr<SharedImageHolder>
XRWebGLDrawingContext::DoneWithSharedBuffer() {
  // Get the shared image from the color swap chain.
  if (!color_swap_chain_ || !color_swap_chain_->layer()) {
    return nullptr;
  }

  const XRSharedImageData& shared_image_data =
      color_swap_chain_->layer()->SharedImage();

  if (!shared_image_data.shared_image) {
    return nullptr;
  }

  gpu::SyncToken sync_token = color_swap_chain_->GetSyncToken();

  return std::make_unique<SharedImageHolder>(shared_image_data.shared_image,
                                             sync_token, base::DoNothing());
}

XRFrameTransportDelegate* XRWebGLDrawingContext::GetTransportDelegate() {
  return binding_->GetTransportDelegate();
}

void XRWebGLDrawingContext::Trace(Visitor* visitor) const {
  visitor->Trace(webgl_context_);
  visitor->Trace(color_swap_chain_);
  visitor->Trace(depth_stencil_swap_chain_);
  visitor->Trace(binding_);
  XRLayerDrawingContext::Trace(visitor);
}

}  // namespace blink
