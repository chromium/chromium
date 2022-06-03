// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/main_thread_worklet_reporting_proxy.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

MainThreadWorkletReportingProxy::MainThreadWorkletReportingProxy(
    ExecutionContext* context)
    : context_(context) {}

void MainThreadWorkletReportingProxy::CountFeature(WebFeature feature) {
  DCHECK(IsMainThread());
  // A parent context is on the same thread, so just record API use in the
  // context's UseCounter.
  UseCounter::Count(context_, feature);
}

void MainThreadWorkletReportingProxy::DidTerminateWorkerThread() {
  // MainThreadWorklet does not start and terminate a thread.
  NOTREACHED();
}

}  // namespace blink
