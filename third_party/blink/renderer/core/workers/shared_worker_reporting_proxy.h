// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_SHARED_WORKER_REPORTING_PROXY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_SHARED_WORKER_REPORTING_PROXY_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/workers/parent_execution_context_task_runners.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class WebSharedWorkerImpl;

// An implementation of WorkerReportingProxy for SharedWorker. This is created
// and owned by WebSharedWorkerImpl on the main thread, accessed from a worker
// thread, and destroyed on the main thread.
class SharedWorkerReportingProxy final
    : public GarbageCollected<SharedWorkerReportingProxy>,
      public WorkerReportingProxy {
 public:
  SharedWorkerReportingProxy(WebSharedWorkerImpl*,
                             ParentExecutionContextTaskRunners*);
  ~SharedWorkerReportingProxy() override;

  // WorkerReportingProxy methods:
  void CountFeature(WebFeature) override;
  void CountDeprecation(WebFeature) override;
  void ReportException(const WTF::String&,
                       std::unique_ptr<SourceLocation>,
                       int exception_id) override;
  void ReportConsoleMessage(mojom::ConsoleMessageSource,
                            mojom::ConsoleMessageLevel,
                            const String& message,
                            SourceLocation*) override;
  void DidFailToFetchClassicScript() override;
  void DidFailToFetchModuleScript() override;
  void DidEvaluateClassicScript(bool success) override;
  void DidEvaluateModuleScript(bool success) override;
  void DidCloseWorkerGlobalScope() override;
  void WillDestroyWorkerGlobalScope() override {}
  void DidTerminateWorkerThread() override;

  void Trace(blink::Visitor*);

 private:
  // Not owned because this outlives the reporting proxy.
  WebSharedWorkerImpl* worker_;

  Member<ParentExecutionContextTaskRunners>
      parent_execution_context_task_runners_;
  DISALLOW_COPY_AND_ASSIGN(SharedWorkerReportingProxy);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_SHARED_WORKER_REPORTING_PROXY_H_
