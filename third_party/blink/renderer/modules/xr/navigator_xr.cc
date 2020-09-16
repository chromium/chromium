// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/navigator_xr.h"

#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/xr/xr_system.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

const char NavigatorXR::kSupplementName[] = "NavigatorXR";

bool NavigatorXR::AlreadyExists(Document& document) {
  if (!document.GetFrame())
    return false;

  Navigator& navigator = *document.GetFrame()->DomWindow()->navigator();
  return !!Supplement<Navigator>::From<NavigatorXR>(navigator);
}

NavigatorXR* NavigatorXR::From(Document& document) {
  if (!document.GetFrame())
    return nullptr;
  Navigator& navigator = *document.GetFrame()->DomWindow()->navigator();
  return &From(navigator);
}

NavigatorXR& NavigatorXR::From(Navigator& navigator) {
  NavigatorXR* supplement = Supplement<Navigator>::From<NavigatorXR>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorXR>(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

XRSystem* NavigatorXR::xr(Navigator& navigator) {
  // Always return null when the navigator is detached.
  if (!navigator.GetFrame()) {
    return nullptr;
  }

  return NavigatorXR::From(navigator).xr();
}

XRSystem* NavigatorXR::xr() {
  auto* document = GetDocument();

  // Always return null when the navigator is detached.
  if (!document)
    return nullptr;

  if (!did_log_navigator_xr_) {
    ukm::builders::XR_WebXR(document->UkmSourceID())
        .SetDidUseNavigatorXR(1)
        .Record(document->UkmRecorder());

    did_log_navigator_xr_ = true;
  }

  if (!xr_) {
    xr_ = MakeGarbageCollected<XRSystem>(*document->GetFrame(),
                                         document->UkmSourceID());
  }

  return xr_;
}

Document* NavigatorXR::GetDocument() {
  if (!GetSupplementable() || !GetSupplementable()->GetFrame())
    return nullptr;

  return GetSupplementable()->GetFrame()->GetDocument();
}

void NavigatorXR::Trace(Visitor* visitor) const {
  visitor->Trace(xr_);
  Supplement<Navigator>::Trace(visitor);
}

NavigatorXR::NavigatorXR(Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

}  // namespace blink
