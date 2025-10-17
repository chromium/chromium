// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/global_performance.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

namespace {

template <typename T, typename P>
class GlobalPerformanceImpl final
    : public GarbageCollected<GlobalPerformanceImpl<T, P>>,
      public Supplement<T> {
 public:
  static const char kSupplementName[];

  static GlobalPerformanceImpl& From(T& supplementable) {
    GlobalPerformanceImpl* supplement =
        Supplement<T>::template From<GlobalPerformanceImpl>(supplementable);
    if (!supplement) {
      supplement = MakeGarbageCollected<GlobalPerformanceImpl>(supplementable);
      Supplement<T>::ProvideTo(supplementable, supplement);
    }
    return *supplement;
  }

  explicit GlobalPerformanceImpl(T& supplementable)
      : Supplement<T>(supplementable) {}

  P* GetPerformance(T* supplementable) {
    if (!performance_) {
      performance_ = MakeGarbageCollected<P>(supplementable);
    }
    return performance_.Get();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(performance_);
    Supplement<T>::Trace(visitor);
  }

 private:
  mutable Member<P> performance_;
};

// static
template <typename T, typename P>
const char GlobalPerformanceImpl<T, P>::kSupplementName[] =
    "GlobalPerformanceImpl";

}  // namespace

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
