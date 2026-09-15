// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/paint_timing_utils.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"

namespace blink::paint_timing {

void NotifyLoaderPerformanceTimingChanged(LocalDOMWindow* window) {
  if (!window) {
    return;
  }
  NotifyLoaderPerformanceTimingChanged(window->document());
}

void NotifyLoaderPerformanceTimingChanged(Document* document) {
  if (!document || !document->Loader()) {
    return;
  }
  document->Loader()->DidChangePerformanceTiming();
}

}  // namespace blink::paint_timing
