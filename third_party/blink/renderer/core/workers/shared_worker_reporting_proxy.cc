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
  DCHECK(!IsMainThread());
  PostCrossThreadTask(
      *parent_execution_context_task_runners_->Get(TaskType::kInternalDefault),
      FROM_HERE,
      CrossThreadBindOnce(&WebSharedWorkerImpl::DidFailToFetchClassicScript,
                          CrossThreadUnretained(worker_)));
}

void SharedWorkerReportingProxy::DidFailToFetchModuleScript() {
  DCHECK(!IsMainThread());
  PostCrossThreadTask(
      *parent_execution_context_task_runners_->Get(TaskType::kInternalDefault),
      FROM_HERE,
      CrossThreadBindOnce(&WebSharedWorkerImpl::DidFailToFetchModuleScript,
                          CrossThreadUnretained(worker_)));
}

void SharedWorkerReportingProxy::DidEvaluateTopLevelScript(bool success) {
  DCHECK(!IsMainThread());
  PostCrossThreadTask(
      *parent_execution_context_task_runners_->Get(TaskType::kInternalDefault),
      FROM_HERE,
      CrossThreadBindOnce(&WebSharedWorkerImpl::DidEvaluateTopLevelScript,
                          CrossThreadUnretained(worker_), success));
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

void SharedWorkerReportingProxy::Trace(Visitor* visitor) const {
  visitor->Trace(parent_execution_context_task_runners_);
}

}  // namespace blink
