// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/breakout_box/breakout_box_util.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/core/timing/worker_global_scope_performance.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"

namespace blink {

Performance* GetPerformanceFromExecutionContext(ExecutionContext* context) {
  if (!context) {
    return nullptr;
  }
  if (auto* window = DynamicTo<LocalDOMWindow>(context)) {
    return DOMWindowPerformance::performance(*window);
  } else if (auto* worker = DynamicTo<WorkerGlobalScope>(context)) {
    return WorkerGlobalScopePerformance::performance(*worker);
  }
  NOTREACHED();
}

}  // namespace blink
