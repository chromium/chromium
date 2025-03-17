// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/peer_connection_util.h"

#include "base/time/time.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/core/timing/worker_global_scope_performance.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"

namespace blink {

namespace {

Performance* GetPerformanceFromExecutionContext(ExecutionContext* context) {
  if (auto* window = DynamicTo<LocalDOMWindow>(context)) {
    return DOMWindowPerformance::performance(*window);
  } else if (auto* worker = DynamicTo<WorkerGlobalScope>(context)) {
    return WorkerGlobalScopePerformance::performance(*worker);
  }
  NOTREACHED();
}

}  // namespace

DOMHighResTimeStamp CalculateRTCEncodedFrameTimestamp(
    ExecutionContext* context,
    base::TimeTicks timestamp) {
  Performance* performance = GetPerformanceFromExecutionContext(context);
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      performance->GetTimeOriginInternal(), timestamp,
      /*allow_negative_value=*/true,
      performance->CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp CalculateRTCEncodedFrameTimeDelta(
    ExecutionContext* context,
    base::TimeDelta time_delta) {
  return Performance::ClampTimeResolution(
      time_delta, GetPerformanceFromExecutionContext(context)
                      ->CrossOriginIsolatedCapability());
}

}  // namespace blink
