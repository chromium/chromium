// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/main_thread_worklet_reporting_proxy.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

MainThreadWorkletReportingProxy::MainThreadWorkletReportingProxy(
    Document* document)
    : document_(document) {}

void MainThreadWorkletReportingProxy::CountFeature(WebFeature feature) {
  DCHECK(IsMainThread());
  // A parent document is on the same thread, so just record API use in the
  // document's UseCounter.
  UseCounter::Count(document_, feature);
}

void MainThreadWorkletReportingProxy::CountDeprecation(WebFeature feature) {
  DCHECK(IsMainThread());
  // A parent document is on the same thread, so just record API use in the
  // document's UseCounter.
  Deprecation::CountDeprecation(document_, feature);
}

void MainThreadWorkletReportingProxy::DidTerminateWorkerThread() {
  // MainThreadWorklet does not start and terminate a thread.
  NOTREACHED();
}

}  // namespace blink
