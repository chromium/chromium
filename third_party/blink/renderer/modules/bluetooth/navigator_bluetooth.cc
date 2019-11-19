// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/bluetooth/navigator_bluetooth.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth.h"

namespace blink {

NavigatorBluetooth& NavigatorBluetooth::From(Navigator& navigator) {
  NavigatorBluetooth* supplement =
      Supplement<Navigator>::From<NavigatorBluetooth>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorBluetooth>(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

Bluetooth* NavigatorBluetooth::bluetooth(Navigator& navigator) {
  return NavigatorBluetooth::From(navigator).bluetooth();
}

Bluetooth* NavigatorBluetooth::bluetooth() {
  if (bluetooth_)
    return bluetooth_.Get();

  if (!GetSupplementable()->GetFrame())
    return nullptr;

  bluetooth_ = MakeGarbageCollected<Bluetooth>(
      GetSupplementable()->GetFrame()->GetDocument()->GetExecutionContext());
  return bluetooth_.Get();
}

void NavigatorBluetooth::Trace(blink::Visitor* visitor) {
  visitor->Trace(bluetooth_);
  Supplement<Navigator>::Trace(visitor);
}

NavigatorBluetooth::NavigatorBluetooth(Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

// static
const char NavigatorBluetooth::kSupplementName[] = "NavigatorBluetooth";

}  // namespace blink
