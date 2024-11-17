// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_dom_overlay_state.h"

#include "third_party/blink/renderer/core/dom/element.h"

namespace blink {

XRDOMOverlayState::XRDOMOverlayState(V8XRDOMOverlayType::Enum type)
    : type_(type) {}

void XRDOMOverlayState::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
