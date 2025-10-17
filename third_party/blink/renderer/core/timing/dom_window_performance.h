// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_DOM_WINDOW_PERFORMANCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_DOM_WINDOW_PERFORMANCE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"

namespace blink {

class LocalDOMWindow;

class CORE_EXPORT DOMWindowPerformance final {
 public:
  static WindowPerformance* performance(LocalDOMWindow&);

  explicit DOMWindowPerformance(LocalDOMWindow&);
  DOMWindowPerformance(const DOMWindowPerformance&) = delete;
  DOMWindowPerformance& operator=(const DOMWindowPerformance&) = delete;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_DOM_WINDOW_PERFORMANCE_H_
