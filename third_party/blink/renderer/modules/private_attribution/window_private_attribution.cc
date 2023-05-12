// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/private_attribution/window_private_attribution.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/private_attribution/private_attribution.h"

namespace blink {

WindowPrivateAttribution::WindowPrivateAttribution(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window) {}

// static
const char WindowPrivateAttribution::kSupplementName[] =
    "WindowPrivateAttribution";

// static
WindowPrivateAttribution& WindowPrivateAttribution::From(
    LocalDOMWindow& window) {
  WindowPrivateAttribution* supplement =
      Supplement<LocalDOMWindow>::From<WindowPrivateAttribution>(window);
  if (!supplement) {
    supplement = MakeGarbageCollected<WindowPrivateAttribution>(window);
    ProvideTo(window, supplement);
  }
  return *supplement;
}

// static
PrivateAttribution* WindowPrivateAttribution::privateAttribution(
    LocalDOMWindow& window) {
  return WindowPrivateAttribution::From(window).privateAttribution();
}

PrivateAttribution* WindowPrivateAttribution::privateAttribution() {
  if (!private_attribution_) {
    private_attribution_ = MakeGarbageCollected<PrivateAttribution>();
  }
  return private_attribution_.Get();
}

void WindowPrivateAttribution::Trace(Visitor* visitor) const {
  visitor->Trace(private_attribution_);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

}  // namespace blink
