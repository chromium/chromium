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

void SharedWorkerReportingProxy::ReportException(const String& error_message,
                                                 const SourceLocation* location,
                                                 int exception_id) {
  DCHECK(!IsMainThread());
  // Exceptions during the script evaluation phase are reported to the clients,
  // but runtime errors after evaluation are not.
  // See:
  // https://html.spec.whatwg.org/C/#worker-processing-model
  // and https://html.spec.whatwg.org/C/#runtime-script-errors-2
  if (script_evaluated_) {
    return;
  }

  // TODO(https://crbug.com/438606270): This is a heuristic to distinguish parse
  // errors from runtime errors during evaluation. "SyntaxError" indicates a
  // script parsing failure, which should dispatch a generic `Event`. Other
  // errors that occur during script evaluation are considered runtime errors
  // and should dispatch a detailed `ErrorEvent`. This should be replaced with a
  // more robust mechanism if one becomes available.
  const bool is_eval_error = !error_message.Contains("SyntaxError");

  PostCrossThreadTask(
      *main_thread_task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          &WebSharedWorkerImpl::ReportException, CrossThreadUnretained(worker_),
          error_message, location->Url(), location->LineNumber(),
          location->ColumnNumber(), exception_id, is_eval_error));
}

void SharedWorkerReportingProxy::ReportConsoleMessage(
    mojom::ConsoleMessageSource,
    mojom::ConsoleMessageLevel,
    const String& message,
    const SourceLocation*) {
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
  CHECK(!script_evaluated_);
  script_evaluated_ = true;
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
