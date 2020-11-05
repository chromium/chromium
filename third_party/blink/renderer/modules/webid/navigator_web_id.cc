// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webid/navigator_web_id.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/webid/web_id.h"

namespace blink {

const char NavigatorWebID::kSupplementName[] = "NavigatorWebID";

NavigatorWebID& NavigatorWebID::From(Navigator& navigator) {
  NavigatorWebID* supplement =
      Supplement<Navigator>::From<NavigatorWebID>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorWebID>(navigator);
    NavigatorWebID::ProvideTo(navigator, supplement);
  }
  return *supplement;
}

WebID* NavigatorWebID::id(Navigator& navigator) {
  return NavigatorWebID::From(navigator).id();
}

WebID* NavigatorWebID::id() {
  return web_id_;
}

void NavigatorWebID::Trace(Visitor* visitor) const {
  visitor->Trace(web_id_);
  Supplement<Navigator>::Trace(visitor);
}

NavigatorWebID::NavigatorWebID(Navigator& navigator) {
  if (navigator.DomWindow()) {
    web_id_ = MakeGarbageCollected<WebID>(*navigator.DomWindow());
  }
}

}  // namespace blink
