// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_THREADED_OBJECT_PROXY_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_THREADED_OBJECT_PROXY_BASE_H_

#include "base/macros.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

class ParentExecutionContextTaskRunners;
class ThreadedMessagingProxyBase;

// The base proxy class to talk to a DedicatedWorker or *Worklet object on the
// main thread via the ThreadedMessagingProxyBase from a worker thread. This is
// created and destroyed on the main thread, and used on the worker thread.
// ThreadedMessagingProxyBase always outlives this proxy.
class CORE_EXPORT ThreadedObjectProxyBase : public WorkerReportingProxy {
  USING_FAST_MALLOC(ThreadedObjectProxyBase);
 public:
  ~ThreadedObjectProxyBase() override = default;

  void ReportPendingActivity(bool has_pending_activity);

  // WorkerReportingProxy overrides.
  void CountFeature(WebFeature) override;
  void CountDeprecation(WebFeature) override;
  void ReportConsoleMessage(mojom::ConsoleMessageSource,
                            mojom::ConsoleMessageLevel,
                            const String& message,
                            SourceLocation*) override;
  void DidCloseWorkerGlobalScope() override;
  void DidTerminateWorkerThread() override;

 protected:
  explicit ThreadedObjectProxyBase(ParentExecutionContextTaskRunners*);
  virtual CrossThreadWeakPersistent<ThreadedMessagingProxyBase>
  MessagingProxyWeakPtr() = 0;
  ParentExecutionContextTaskRunners* GetParentExecutionContextTaskRunners();

 private:
  // Used to post a task to ThreadedMessagingProxyBase on the parent context
  // thread.
  CrossThreadPersistent<ParentExecutionContextTaskRunners>
      parent_execution_context_task_runners_;
  DISALLOW_COPY_AND_ASSIGN(ThreadedObjectProxyBase);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_THREADED_OBJECT_PROXY_BASE_H_
