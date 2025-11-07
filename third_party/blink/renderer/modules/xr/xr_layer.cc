// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_layer.h"

#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/xr/xr_layer_shared_image_manager.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"

namespace blink {

XRLayer::XRLayer(XRSession* session)
    : session_(session), layer_id_(session->GetNextLayerId()) {}

ExecutionContext* XRLayer::GetExecutionContext() const {
  return session_->GetExecutionContext();
}

const AtomicString& XRLayer::InterfaceName() const {
  return event_target_names::kXRLayer;
}

const XRSharedImageData& XRLayer::SharedImage() const {
  return session_->LayerSharedImageManager().LayerSharedImage(layer_id_);
}

bool XRLayer::HasSharedImage() const {
  return session_->LayerSharedImageManager().HasLayerSharedImage(layer_id_);
}

void XRLayer::SetModified(bool is_modified) {
  is_modified_ = is_modified;
}

bool XRLayer::IsModified() const {
  return is_modified_;
}

void XRLayer::CreateLayerBackend() {
  if (auto* layer_manager = session()->LayerManager(); layer_manager) {
    layer_manager->CreateCompositionLayer(
        CreateLayerData(),
        BindOnce(&XRLayer::OnBackendLayerCreated, WrapWeakPersistent(this)));
  }
}

void XRLayer::OnBackendLayerCreated(
    device::mojom::blink::CreateCompositionLayerResult result) {
  is_backend_active_ =
      result == device::mojom::blink::CreateCompositionLayerResult::SUCCESS;
}

bool XRLayer::IsBackendActive() const {
  return is_backend_active_;
}

void XRLayer::DestroyBackend() {
  if (auto* layer_manager = session()->LayerManager(); layer_manager) {
    layer_manager->DestroyCompositionLayer(layer_id_);
  }
}

bool XRLayer::needsRedraw() const {
  return needs_redraw_ && HasSharedImage();
}

void XRLayer::SetNeedsRedraw(bool needsRedraw) {
  needs_redraw_ = needsRedraw;
}

void XRLayer::Trace(Visitor* visitor) const {
  visitor->Trace(session_);
  EventTarget::Trace(visitor);
}

}  // namespace blink
