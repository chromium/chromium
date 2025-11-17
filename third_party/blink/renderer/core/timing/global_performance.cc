// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/global_performance.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

template <typename T, typename P>
class GlobalPerformanceImpl final
    : public GarbageCollected<GlobalPerformanceImpl<T, P>>,
      public GarbageCollectedMixin {
 public:
  static GlobalPerformanceImpl& From(T& supplementable) {
    GlobalPerformanceImpl* supplement =
        supplementable.GetGlobalPerformanceImpl();
    if (!supplement) {
      supplement = MakeGarbageCollected<GlobalPerformanceImpl>();
      supplementable.SetGlobalPerformanceImpl(supplement);
    }
    return *supplement;
  }

  GlobalPerformanceImpl() = default;

  P* GetPerformance(T* supplementable) {
    if (!performance_) {
      performance_ = MakeGarbageCollected<P>(supplementable);
    }
    return performance_.Get();
  }

  void Trace(Visitor* visitor) const override { visitor->Trace(performance_); }

 private:
  mutable Member<P> performance_;
};

// static
WindowPerformance* GlobalPerformance::performance(LocalDOMWindow& window) {
  return GlobalPerformanceImpl<LocalDOMWindow, WindowPerformance>::From(window)
      .GetPerformance(&window);
}

// static
WorkerPerformance* GlobalPerformance::performance(WorkerGlobalScope& worker) {
  return GlobalPerformanceImpl<WorkerGlobalScope, WorkerPerformance>::From(
             worker)
      .GetPerformance(&worker);
}

}  // namespace blink
