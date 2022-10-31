// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/shared_worker_reporting_proxy.h"

#include "base/location.h"
#include "third_party/blink/renderer/core/exported/web_shared_worker_impl.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

SharedWorkerReportingProxy::SharedWorkerReportingProxy(
    WebSharedWorkerImpl* worker)
    : worker_(worker),
      main_thread_task_runner_(Thread::MainThread()->GetTaskRunner(
          MainThreadTaskRunnerRestricted())) {
  DCHECK(IsMainThread());
}

SharedWorkerReportingProxy::~SharedWorkerReportingProxy() {
  DCHECK(IsMainThread());
}

void SharedWorkerReportingProxy::CountFeature(WebFeature feature) {
  DCHECK(!IsMainThread());
  PostCrossThreadTask(
      *main_thread_task_runner_, FROM_HERE,
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
      *main_thread_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&WebSharedWorkerImpl::DidFailToFetchClassicScript,
                          CrossThreadUnretained(worker_)));
}

void SharedWorkerReportingProxy::DidFailToFetchModuleScript() {
  DCHECK(!IsMainThread());
  PostCrossThreadTask(
      *main_thread_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&WebSharedWorkerImpl::DidFailToFetchModuleScript,
                          CrossThreadUnretained(worker_)));
}

void SharedWorkerReportingProxy::DidEvaluateTopLevelScript(bool success) {
  DCHECK(!IsMainThread());
  PostCrossThreadTask(
      *main_thread_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&WebSharedWorkerImpl::DidEvaluateTopLevelScript,
                          CrossThreadUnretained(worker_), success));
}

void SharedWorkerReportingProxy::DidCloseWorkerGlobalScope() {
  DCHECK(!IsMainThread());
  PostCrossThreadTask(
      *main_thread_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&WebSharedWorkerImpl::DidCloseWorkerGlobalScope,
                          CrossThreadUnretained(worker_)));
}

void SharedWorkerReportingProxy::DidTerminateWorkerThread() {
  DCHECK(!IsMainThread());
  PostCrossThreadTask(
      *main_thread_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&WebSharedWorkerImpl::DidTerminateWorkerThread,
                          CrossThreadUnretained(worker_)));
}

void SharedWorkerReportingProxy::Trace(Visitor* visitor) const {}

}  // namespace blink
