// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/shared_worker_reporting_proxy.h"

#include "base/location.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/core/exported/web_shared_worker_impl.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

SharedWorkerReportingProxy::SharedWorkerReportingProxy(
    WebSharedWorkerImpl* worker,
    ParentExecutionContextTaskRunners* parent_execution_context_task_runners)
    : worker_(worker),
      parent_execution_context_task_runners_(
          parent_execution_context_task_runners) {
  DCHECK(IsMainThread());
}

SharedWorkerReportingProxy::~SharedWorkerReportingProxy() {
  DCHECK(IsMainThread());
}

void SharedWorkerReportingProxy::CountFeature(WebFeature feature) {
  DCHECK(!IsMainThread());
  PostCrossThreadTask(
      *parent_execution_context_task_runners_->Get(TaskType::kInternalDefault),
      FROM_HERE,
      CrossThreadBindOnce(&WebSharedWorkerImpl::CountFeature,
                          CrossThreadUnretained(worker_), feature));
}

void SharedWorkerReportingProxy::CountDeprecation(WebFeature feature) {
  DCHECK(!IsMainThread());
  // Go through the same code path with countFeature() because a deprecation
  // message is already shown on the worker console and a remaining work is just
  // to record an API use.
  CountFeature(feature);
}

void SharedWorkerReportingProxy::ReportException(
    const String& error_message,
    std::unique_ptr<SourceLocation>,
    int exception_id) {
  DCHECK(!IsMainThread());
  // TODO(nhiroki): Implement the "runtime script errors" algorithm in the HTML
  // spec:
  // "For shared workers, if the error is still not handled afterwards, the
  // error may be reported to a developer console."
  // https://html.spec.whatwg.org/C/#runtime-script-errors-2
}

void SharedWorkerReportingProxy::ReportConsoleMessage(
    mojom::ConsoleMessageSource,
    mojom::ConsoleMessageLevel,
    const String& message,
    SourceLocation*) {
  DCHECK(!IsMainThread());
  // Not supported in SharedWorker.
}

void SharedWorkerReportingProxy::DidFailToFetchClassicScript() {
  // TODO(nhiroki): Add a runtime flag check for off-the-main-thread shared
  // worker script fetch. This function should be called only when the flag is
  // enabled (https://crbug.com/924041).
  DCHECK(!IsMainThread());
  PostCrossThreadTask(
      *parent_execution_context_task_runners_->Get(TaskType::kInternalDefault),
      FROM_HERE,
      CrossThreadBindOnce(&WebSharedWorkerImpl::DidFailToFetchClassicScript,
                          CrossThreadUnretained(worker_)));
}

void SharedWorkerReportingProxy::DidFailToFetchModuleScript() {
  DCHECK(!IsMainThread());
  // TODO(nhiroki): Implement module scripts for shared workers.
  // (https://crbug.com/824646)
  NOTIMPLEMENTED();
}

void SharedWorkerReportingProxy::DidEvaluateClassicScript(bool success) {
  DCHECK(!IsMainThread());
  PostCrossThreadTask(
      *parent_execution_context_task_runners_->Get(TaskType::kInternalDefault),
      FROM_HERE,
      CrossThreadBindOnce(&WebSharedWorkerImpl::DidEvaluateClassicScript,
                          CrossThreadUnretained(worker_), success));
}

void SharedWorkerReportingProxy::DidEvaluateModuleScript(bool success) {
  DCHECK(!IsMainThread());
  // TODO(nhiroki): Implement module scripts for shared workers.
  // (https://crbug.com/824646)
  NOTIMPLEMENTED();
}

void SharedWorkerReportingProxy::DidCloseWorkerGlobalScope() {
  DCHECK(!IsMainThread());
  PostCrossThreadTask(
      *parent_execution_context_task_runners_->Get(TaskType::kInternalDefault),
      FROM_HERE,
      CrossThreadBindOnce(&WebSharedWorkerImpl::DidCloseWorkerGlobalScope,
                          CrossThreadUnretained(worker_)));
}

void SharedWorkerReportingProxy::DidTerminateWorkerThread() {
  DCHECK(!IsMainThread());
  PostCrossThreadTask(
      *parent_execution_context_task_runners_->Get(TaskType::kInternalDefault),
      FROM_HERE,
      CrossThreadBindOnce(&WebSharedWorkerImpl::DidTerminateWorkerThread,
                          CrossThreadUnretained(worker_)));
}

void SharedWorkerReportingProxy::Trace(blink::Visitor* visitor) {
  visitor->Trace(parent_execution_context_task_runners_);
}

}  // namespace blink
