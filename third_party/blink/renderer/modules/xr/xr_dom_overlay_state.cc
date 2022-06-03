// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_dom_overlay_state.h"

#include "third_party/blink/renderer/core/dom/element.h"

namespace blink {

namespace {

const String MapOverlayType(XRDOMOverlayState::DOMOverlayType type) {
  switch (type) {
    case XRDOMOverlayState::DOMOverlayType::kScreen:
      return "screen";
    case XRDOMOverlayState::DOMOverlayType::kFloating:
      return "floating";
  }
}

}  // namespace

XRDOMOverlayState::XRDOMOverlayState(DOMOverlayType type)
    : type_string_(MapOverlayType(type)) {}

void XRDOMOverlayState::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
