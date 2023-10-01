/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope_proxy.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "services/network/public/mojom/url_loader.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_client.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom-blink.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_error.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_client.h"
#include "third_party/blink/public/web/web_serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/headers.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/modules/exported/web_embedded_worker_impl.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope.h"
#include "third_party/blink/renderer/modules/service_worker/wait_until_observer.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

ServiceWorkerGlobalScopeProxy::~ServiceWorkerGlobalScopeProxy() {
  DCHECK(parent_thread_default_task_runner_->BelongsToCurrentThread());
  // Verify that the proxy has been detached.
  DCHECK(!embedded_worker_);
}

void ServiceWorkerGlobalScopeProxy::BindServiceWorker(
    CrossVariantMojoReceiver<mojom::blink::ServiceWorkerInterfaceBase>
        receiver) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  WorkerGlobalScope()->BindServiceWorker(std::move(receiver));
}

void ServiceWorkerGlobalScopeProxy::BindControllerServiceWorker(
    CrossVariantMojoReceiver<mojom::blink::ControllerServiceWorkerInterfaceBase>
        receiver) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  WorkerGlobalScope()->BindControllerServiceWorker(std::move(receiver));
}

void ServiceWorkerGlobalScopeProxy::OnNavigationPreloadResponse(
    int fetch_event_id,
    std::unique_ptr<WebURLResponse> response,
    mojo::ScopedDataPipeConsumerHandle data_pipe) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  WorkerGlobalScope()->OnNavigationPreloadResponse(
      fetch_event_id, std::move(response), std::move(data_pipe));
}

void ServiceWorkerGlobalScopeProxy::OnNavigationPreloadError(
    int fetch_event_id,
    std::unique_ptr<WebServiceWorkerError> error) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  WorkerGlobalScope()->OnNavigationPreloadError(fetch_event_id,
                                                std::move(error));
}

void ServiceWorkerGlobalScopeProxy::OnNavigationPreloadComplete(
    int fetch_event_id,
    base::TimeTicks completion_time,
    int64_t encoded_data_length,
    int64_t encoded_body_length,
    int64_t decoded_body_length) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  WorkerGlobalScope()->OnNavigationPreloadComplete(
      fetch_event_id, completion_time, encoded_data_length, encoded_body_length,
      decoded_body_length);
}

void ServiceWorkerGlobalScopeProxy::CountFeature(WebFeature feature) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  Client().CountFeature(feature);
}

void ServiceWorkerGlobalScopeProxy::ReportException(
    const String& error_message,
    std::unique_ptr<SourceLocation> location,
    int exception_id) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  Client().ReportException(error_message, location->LineNumber(),
                           location->ColumnNumber(), location->Url());
}

void ServiceWorkerGlobalScopeProxy::ReportConsoleMessage(
    mojom::ConsoleMessageSource source,
    mojom::ConsoleMessageLevel level,
    const String& message,
    SourceLocation* location) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  Client().ReportConsoleMessage(source, level, message, location->LineNumber(),
                                location->Url());
}

void ServiceWorkerGlobalScopeProxy::WillInitializeWorkerContext() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  Client().WillInitializeWorkerContext();
}

void ServiceWorkerGlobalScopeProxy::DidCreateWorkerGlobalScope(
    WorkerOrWorkletGlobalScope* worker_global_scope) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  DCHECK(!worker_global_scope_);
  worker_global_scope_ =
      static_cast<ServiceWorkerGlobalScope*>(worker_global_scope);
  scoped_refptr<base::SequencedTaskRunner> worker_task_runner =
      worker_global_scope->GetThread()->GetTaskRunner(
          TaskType::kInternalDefault);
  Client().WorkerContextStarted(this, std::move(worker_task_runner));
}

void ServiceWorkerGlobalScopeProxy::DidLoadClassicScript() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  Client().WorkerScriptLoadedOnWorkerThread();
}

void ServiceWorkerGlobalScopeProxy::DidFetchScript() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  Client().WorkerScriptLoadedOnWorkerThread();
}

void ServiceWorkerGlobalScopeProxy::DidFailToFetchClassicScript() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  Client().FailedToFetchClassicScript();
}

void ServiceWorkerGlobalScopeProxy::DidFailToFetchModuleScript() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  Client().FailedToFetchModuleScript();
}

void ServiceWorkerGlobalScopeProxy::WillEvaluateScript() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
      "ServiceWorker", "ServiceWorkerGlobalScopeProxy::EvaluateTopLevelScript",
      TRACE_ID_LOCAL(this));
  ScriptState::Scope scope(
      WorkerGlobalScope()->ScriptController()->GetScriptState());
  Client().WillEvaluateScript(
      WorkerGlobalScope()->ScriptController()->GetContext());
  top_level_script_evaluation_start_time_ = base::TimeTicks::Now();
}

void ServiceWorkerGlobalScopeProxy::DidEvaluateTopLevelScript(bool success) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  base::UmaHistogramTimes(
      base::StrCat({"ServiceWorker.EvaluateTopLevelScript.",
                    success ? "Succeeded" : "Failed", ".Time"}),
      base::TimeTicks::Now() - top_level_script_evaluation_start_time_);
  WorkerGlobalScope()->DidEvaluateScript();
  Client().DidEvaluateScript(success);
  TRACE_EVENT_NESTABLE_ASYNC_END1(
      "ServiceWorker", "ServiceWorkerGlobalScopeProxy::EvaluateTopLevelScript",
      TRACE_ID_LOCAL(this), "success", success);
}

void ServiceWorkerGlobalScopeProxy::DidCloseWorkerGlobalScope() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  // close() is not web-exposed for ServiceWorker. This is called when
  // ServiceWorkerGlobalScope internally requests close(), for example, due to
  // failure on startup when installed scripts couldn't be read.
  //
  // This may look like a roundabout way to terminate the thread, but close()
  // seems like the standard way to initiate termination from inside the thread.

  // ServiceWorkerGlobalScope expects us to terminate the thread, so request
  // that here.
  PostCrossThreadTask(
      *parent_thread_default_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&WebEmbeddedWorkerImpl::TerminateWorkerContext,
                          CrossThreadUnretained(embedded_worker_.get())));

  // NOTE: WorkerThread calls WillDestroyWorkerGlobalScope() synchronously after
  // this function returns, since it calls DidCloseWorkerGlobalScope() then
  // PrepareForShutdownOnWorkerThread().
}

void ServiceWorkerGlobalScopeProxy::WillDestroyWorkerGlobalScope() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  v8::HandleScope handle_scope(WorkerGlobalScope()->GetThread()->GetIsolate());
  Client().WillDestroyWorkerContext(
      WorkerGlobalScope()->ScriptController()->GetContext());
  worker_global_scope_ = nullptr;
}

void ServiceWorkerGlobalScopeProxy::DidTerminateWorkerThread() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  // This must be called after WillDestroyWorkerGlobalScope().
  DCHECK(!worker_global_scope_);
  Client().WorkerContextDestroyed();
}

bool ServiceWorkerGlobalScopeProxy::IsServiceWorkerGlobalScopeProxy() const {
  return true;
}

void ServiceWorkerGlobalScopeProxy::SetupNavigationPreload(
    int fetch_event_id,
    const KURL& url,
    mojo::PendingReceiver<network::mojom::blink::URLLoaderClient>
        preload_url_loader_client_receiver) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  Client().SetupNavigationPreload(
      fetch_event_id, url, std::move(preload_url_loader_client_receiver));
}

void ServiceWorkerGlobalScopeProxy::RequestTermination(
    CrossThreadOnceFunction<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  Client().RequestTermination(ConvertToBaseOnceCallback(std::move(callback)));
}

bool ServiceWorkerGlobalScopeProxy::
    ShouldNotifyServiceWorkerOnWebSocketActivity(
        v8::Local<v8::Context> context) {
  return Client().ShouldNotifyServiceWorkerOnWebSocketActivity(context);
}

ServiceWorkerGlobalScopeProxy::ServiceWorkerGlobalScopeProxy(
    WebEmbeddedWorkerImpl& embedded_worker,
    WebServiceWorkerContextClient& client,
    scoped_refptr<base::SingleThreadTaskRunner>
        parent_thread_default_task_runner)
    : embedded_worker_(&embedded_worker),
      parent_thread_default_task_runner_(
          std::move(parent_thread_default_task_runner)),
      client_(&client),
      worker_global_scope_(nullptr) {
  DETACH_FROM_THREAD(worker_thread_checker_);
  DCHECK(parent_thread_default_task_runner_);
}

void ServiceWorkerGlobalScopeProxy::Detach() {
  DCHECK(parent_thread_default_task_runner_->BelongsToCurrentThread());
  embedded_worker_ = nullptr;
  client_ = nullptr;
}

void ServiceWorkerGlobalScopeProxy::TerminateWorkerContext() {
  DCHECK(parent_thread_default_task_runner_->BelongsToCurrentThread());
  embedded_worker_->TerminateWorkerContext();
}

bool ServiceWorkerGlobalScopeProxy::IsWindowInteractionAllowed() {
  return WorkerGlobalScope()->IsWindowInteractionAllowed();
}

void ServiceWorkerGlobalScopeProxy::PauseEvaluation() {
  WorkerGlobalScope()->PauseEvaluation();
}

void ServiceWorkerGlobalScopeProxy::ResumeEvaluation() {
  WorkerGlobalScope()->ResumeEvaluation();
}

mojom::blink::ServiceWorkerFetchHandlerType
ServiceWorkerGlobalScopeProxy::FetchHandlerType() {
  return WorkerGlobalScope()->FetchHandlerType();
}

bool ServiceWorkerGlobalScopeProxy::HasHidEventHandlers() {
  return WorkerGlobalScope()->HasHidEventHandlers();
}

bool ServiceWorkerGlobalScopeProxy::HasUsbEventHandlers() {
  return WorkerGlobalScope()->HasUsbEventHandlers();
}

void ServiceWorkerGlobalScopeProxy::GetRemoteAssociatedInterface(
    const WebString& name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  WorkerGlobalScope()->GetRemoteAssociatedInterface(name, std::move(handle));
}

blink::AssociatedInterfaceRegistry&
ServiceWorkerGlobalScopeProxy::GetAssociatedInterfaceRegistry() {
  return WorkerGlobalScope()->GetAssociatedInterfaceRegistry();
}

WebServiceWorkerContextClient& ServiceWorkerGlobalScopeProxy::Client() const {
  DCHECK(client_);
  return *client_;
}

ServiceWorkerGlobalScope* ServiceWorkerGlobalScopeProxy::WorkerGlobalScope()
    const {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  DCHECK(worker_global_scope_);
  return worker_global_scope_;
}

}  // namespace blink
