// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webid/navigator_web_id.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/webid/web_id.h"

namespace blink {

const char NavigatorWebId::kSupplementName[] = "NavigatorWebId";

NavigatorWebId& NavigatorWebId::From(Navigator& navigator) {
  NavigatorWebId* supplement =
      Supplement<Navigator>::From<NavigatorWebId>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorWebId>(navigator);
    NavigatorWebId::ProvideTo(navigator, supplement);
  }
  return *supplement;
}

WebId* NavigatorWebId::id(Navigator& navigator) {
  return NavigatorWebId::From(navigator).id();
}

WebId* NavigatorWebId::id() {
  return web_id_;
}

void NavigatorWebId::Trace(Visitor* visitor) const {
  visitor->Trace(web_id_);
  Supplement<Navigator>::Trace(visitor);
}

NavigatorWebId::NavigatorWebId(Navigator& navigator) : Supplement(navigator) {
  if (navigator.DomWindow()) {
    web_id_ = MakeGarbageCollected<WebId>(*navigator.DomWindow());
  }
}

}  // namespace blink
