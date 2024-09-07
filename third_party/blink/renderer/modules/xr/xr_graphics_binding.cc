// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_graphics_binding.h"

#include "third_party/blink/renderer/modules/xr/xr_composition_layer.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"

namespace blink {

XRGraphicsBinding::XRGraphicsBinding(XRSession* session) : session_(session) {}

double XRGraphicsBinding::nativeProjectionScaleFactor() const {
  return session_->NativeFramebufferScale();
}

bool XRGraphicsBinding::OwnsLayer(XRCompositionLayer* layer) {
  if (layer == nullptr) {
    return false;
  }
  return this == layer->binding();
}

void XRGraphicsBinding::Trace(Visitor* visitor) const {
  visitor->Trace(session_);
}

}  // namespace blink
