// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/hid/navigator_hid.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/hid/hid.h"

namespace blink {

NavigatorHID& NavigatorHID::From(Navigator& navigator) {
  NavigatorHID* supplement =
      Supplement<Navigator>::From<NavigatorHID>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorHID>(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

HID* NavigatorHID::hid(Navigator& navigator) {
  return NavigatorHID::From(navigator).hid();
}

HID* NavigatorHID::hid() {
  return hid_;
}

void NavigatorHID::Trace(blink::Visitor* visitor) {
  visitor->Trace(hid_);
  Supplement<Navigator>::Trace(visitor);
}

NavigatorHID::NavigatorHID(Navigator& navigator) {
  if (navigator.GetFrame()) {
    DCHECK(navigator.GetFrame()->GetDocument());
    hid_ = MakeGarbageCollected<HID>(*navigator.GetFrame()->GetDocument());
  }
}

const char NavigatorHID::kSupplementName[] = "NavigatorHID";

}  // namespace blink
