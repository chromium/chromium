// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/serial/navigator_serial.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/serial/serial.h"

namespace blink {

const char NavigatorSerial::kSupplementName[] = "NavigatorSerial";

NavigatorSerial& NavigatorSerial::From(Navigator& navigator) {
  NavigatorSerial* supplement =
      Supplement<Navigator>::From<NavigatorSerial>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorSerial>(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

Serial* NavigatorSerial::serial(Navigator& navigator) {
  return NavigatorSerial::From(navigator).serial();
}

void NavigatorSerial::Trace(blink::Visitor* visitor) {
  visitor->Trace(serial_);
  Supplement<Navigator>::Trace(visitor);
}

NavigatorSerial::NavigatorSerial(Navigator& navigator)
    : Supplement<Navigator>(navigator) {
  if (navigator.GetFrame()) {
    DCHECK(navigator.GetFrame()->GetDocument());
    serial_ =
        MakeGarbageCollected<Serial>(*navigator.GetFrame()->GetDocument());
  }
}

}  // namespace blink
