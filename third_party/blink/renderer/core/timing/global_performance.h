// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_GLOBAL_PERFORMANCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_GLOBAL_PERFORMANCE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/core/timing/worker_performance.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class LocalDOMWindow;
class WorkerGlobalScope;

class CORE_EXPORT GlobalPerformance {
  STATIC_ONLY(GlobalPerformance);

 public:
  static WindowPerformance* performance(LocalDOMWindow&);
  static WorkerPerformance* performance(WorkerGlobalScope&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_GLOBAL_PERFORMANCE_H_
