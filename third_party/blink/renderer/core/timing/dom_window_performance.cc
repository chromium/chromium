// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/dom_window_performance.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/timing/global_performance.h"

namespace blink {
// static
WindowPerformance* DOMWindowPerformance::performance(LocalDOMWindow& window) {
  return GlobalPerformance::performance(window);
}

}  // namespace blink
