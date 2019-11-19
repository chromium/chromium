// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/dom_window_performance.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

DOMWindowPerformance::DOMWindowPerformance(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window) {}

void DOMWindowPerformance::Trace(blink::Visitor* visitor) {
  visitor->Trace(performance_);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

// static
const char DOMWindowPerformance::kSupplementName[] = "DOMWindowPerformance";

// static
DOMWindowPerformance& DOMWindowPerformance::From(LocalDOMWindow& window) {
  DOMWindowPerformance* supplement =
      Supplement<LocalDOMWindow>::From<DOMWindowPerformance>(window);
  if (!supplement) {
    supplement = MakeGarbageCollected<DOMWindowPerformance>(window);
    ProvideTo(window, supplement);
  }
  return *supplement;
}

// static
WindowPerformance* DOMWindowPerformance::performance(LocalDOMWindow& window) {
  return From(window).performance();
}

WindowPerformance* DOMWindowPerformance::performance() {
  if (!performance_)
    performance_ = MakeGarbageCollected<WindowPerformance>(GetSupplementable());
  return performance_.Get();
}

}  // namespace blink
