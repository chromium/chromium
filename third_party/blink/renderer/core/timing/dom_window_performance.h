// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_DOM_WINDOW_PERFORMANCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_DOM_WINDOW_PERFORMANCE_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class LocalDOMWindow;

class CORE_EXPORT DOMWindowPerformance final
    : public GarbageCollected<DOMWindowPerformance>,
      public Supplement<LocalDOMWindow> {
  USING_GARBAGE_COLLECTED_MIXIN(DOMWindowPerformance);

 public:
  static const char kSupplementName[];

  static DOMWindowPerformance& From(LocalDOMWindow&);
  static WindowPerformance* performance(LocalDOMWindow&);

  explicit DOMWindowPerformance(LocalDOMWindow&);

  void Trace(blink::Visitor*) override;

 private:
  WindowPerformance* performance();

  Member<WindowPerformance> performance_;
  DISALLOW_COPY_AND_ASSIGN(DOMWindowPerformance);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_DOM_WINDOW_PERFORMANCE_H_
