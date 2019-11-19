/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/exported/web_shared_worker_impl.h"

#include <memory>
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/service_manager/public/mojom/interface_provider.mojom-blink.h"
#include "third_party/blink/public/mojom/browser_interface_broker.mojom-blink.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom-blink.h"
#include "third_party/blink/public/mojom/script/script_type.mojom-blink.h"
#include "third_party/blink/public/mojom/worker/worker_content_settings_proxy.mojom-blink.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_network_provider.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_worker_fetch_context.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_cache_options.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/worker_devtools_params.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/parent_execution_context_task_runners.h"
#include "third_party/blink/renderer/core/workers/shared_worker_content_settings_proxy.h"
#include "third_party/blink/renderer/core/workers/shared_worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/shared_worker_thread.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_parsers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

WebSharedWorkerImpl::WebSharedWorkerImpl(WebSharedWorkerClient* client)
    : client_(client) {
  DCHECK(IsMainThread());
}

WebSharedWorkerImpl::~WebSharedWorkerImpl() {
  DCHECK(IsMainThread());
}

void WebSharedWorkerImpl::TerminateWorkerThread() {
  DCHECK(IsMainThread());
  if (asked_to_terminate_)
    return;
  asked_to_terminate_ = true;

  if (!worker_thread_) {
    client_->WorkerScriptLoadFailed();
    // The worker thread hasn't been started yet. Immediately notify the client
    // of worker termination.
    client_->WorkerContextDestroyed();
    // |this| is deleted at this point.
    return;
  }
  worker_thread_->Terminate();
  // DidTerminateWorkerThread() will be called asynchronously.
}

void WebSharedWorkerImpl::CountFeature(WebFeature feature) {
  DCHECK(IsMainThread());
  client_->CountFeature(feature);
}

void WebSharedWorkerImpl::DidFailToFetchClassicScript() {
  DCHECK(IsMainThread());
  client_->WorkerScriptLoadFailed();
  TerminateWorkerThread();
  // DidTerminateWorkerThread() will be called asynchronously.
}

void WebSharedWorkerImpl::DidEvaluateClassicScript(bool success) {
  DCHECK(IsMainThread());
  client_->WorkerScriptEvaluated(success);
}

void WebSharedWorkerImpl::DidCloseWorkerGlobalScope() {
  DCHECK(IsMainThread());
  client_->WorkerContextClosed();
  TerminateWorkerThread();
  // DidTerminateWorkerThread() will be called asynchronously.
}

void WebSharedWorkerImpl::DidTerminateWorkerThread() {
  DCHECK(IsMainThread());
  client_->WorkerContextDestroyed();
  // |this| is deleted at this point.
}

void WebSharedWorkerImpl::Connect(MessagePortChannel web_channel) {
  DCHECK(IsMainThread());
  if (asked_to_terminate_)
    return;
  // The HTML spec requires to queue a connect event using the DOM manipulation
  // task source.
  // https://html.spec.whatwg.org/C/#shared-workers-and-the-sharedworker-interface
  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kDOMManipulation), FROM_HERE,
      CrossThreadBindOnce(&WebSharedWorkerImpl::ConnectTaskOnWorkerThread,
                          WTF::CrossThreadUnretained(this),
                          WTF::Passed(std::move(web_channel))));
}

void WebSharedWorkerImpl::ConnectTaskOnWorkerThread(
    MessagePortChannel channel) {
  // Wrap the passed-in channel in a MessagePort, and send it off via a connect
  // event.
  DCHECK(worker_thread_->IsCurrentThread());
  auto* scope = To<SharedWorkerGlobalScope>(worker_thread_->GlobalScope());
  scope->Connect(std::move(channel));
}

void WebSharedWorkerImpl::StartWorkerContext(
    const WebURL& script_request_url,
    const WebString& name,
    const WebString& user_agent,
    const WebString& content_security_policy,
    network::mojom::ContentSecurityPolicyType policy_type,
    network::mojom::IPAddressSpace creation_address_space,
    const base::UnguessableToken& appcache_host_id,
    const base::UnguessableToken& devtools_worker_token,
    mojo::ScopedMessagePipeHandle content_settings_handle,
    mojo::ScopedMessagePipeHandle interface_provider,
    mojo::ScopedMessagePipeHandle browser_interface_broker,
    bool pause_worker_context_on_start) {
  DCHECK(IsMainThread());

  // Creates 'outside settings' used in the "Processing model" algorithm in the
  // HTML spec:
  // https://html.spec.whatwg.org/C/#worker-processing-model
  //
  // TODO(nhiroki): According to the spec, the 'outside settings' should
  // correspond to the Document that called 'new SharedWorker()'. The browser
  // process should pass it up to here.
  scoped_refptr<const SecurityOrigin> starter_origin =
      SecurityOrigin::Create(script_request_url);
  auto* outside_settings_object =
      MakeGarbageCollected<FetchClientSettingsObjectSnapshot>(
          /*global_object_url=*/script_request_url,
          /*base_url=*/script_request_url, starter_origin,
          network::mojom::ReferrerPolicy::kDefault,
          /*outgoing_referrer=*/String(),
          CalculateHttpsState(starter_origin.get()),
          AllowedByNosniff::MimeTypeCheck::kLaxForWorker,
          creation_address_space,
          /*insecure_request_policy=*/kBlockAllMixedContent,
          FetchClientSettingsObject::InsecureNavigationsSet(),
          /*mixed_autoupgrade_opt_out=*/false);

  scoped_refptr<WebWorkerFetchContext> web_worker_fetch_context =
      client_->CreateWorkerFetchContext();
  DCHECK(web_worker_fetch_context);

  // TODO(nhiroki); Set |script_type| to mojom::ScriptType::kModule for module
  // fetch (https://crbug.com/824646).
  mojom::ScriptType script_type = mojom::ScriptType::kClassic;

  bool starter_secure_context =
      starter_origin->IsPotentiallyTrustworthy() ||
      SchemeRegistry::SchemeShouldBypassSecureContextCheck(
          starter_origin->Protocol());

  auto worker_settings = std::make_unique<WorkerSettings>(
      false /* disable_reading_from_canvas */,
      false /* strict_mixed_content_checking */,
      true /* allow_running_of_insecure_content */,
      false /* strictly_block_blockable_mixed_content */,
      GenericFontFamilySettings());

  // Some params (e.g., referrer policy, address space, CSP) passed to
  // GlobalScopeCreationParams are dummy values. They will be updated after
  // worker script fetch on the worker thread.
  auto creation_params = std::make_unique<GlobalScopeCreationParams>(
      script_request_url, script_type,
      OffMainThreadWorkerScriptFetchOption::kEnabled, name, user_agent,
      std::move(web_worker_fetch_context), Vector<CSPHeaderAndType>(),
      outside_settings_object->GetReferrerPolicy(),
      outside_settings_object->GetSecurityOrigin(), starter_secure_context,
      outside_settings_object->GetHttpsState(),
      MakeGarbageCollected<WorkerClients>(),
      std::make_unique<SharedWorkerContentSettingsProxy>(
          mojo::PendingRemote<mojom::blink::WorkerContentSettingsProxy>(
              std::move(content_settings_handle), 0u)),
      base::nullopt /* response_address_space */,
      nullptr /* origin_trial_tokens */, devtools_worker_token,
      std::move(worker_settings), kV8CacheOptionsDefault,
      nullptr /* worklet_module_response_map */,
      service_manager::mojom::blink::InterfaceProviderPtrInfo(
          std::move(interface_provider), 0u),
      mojo::PendingRemote<mojom::blink::BrowserInterfaceBroker>(
          std::move(browser_interface_broker),
          mojom::blink::BrowserInterfaceBroker::Version_),
      BeginFrameProviderParams(), nullptr /* parent_feature_policy */,
      base::UnguessableToken());

  reporting_proxy_ = MakeGarbageCollected<SharedWorkerReportingProxy>(
      this, ParentExecutionContextTaskRunners::Create());
  worker_thread_ =
      std::make_unique<SharedWorkerThread>(*reporting_proxy_, appcache_host_id);

  auto thread_startup_data = WorkerBackingThreadStartupData::CreateDefault();
  thread_startup_data.atomics_wait_mode =
      WorkerBackingThreadStartupData::AtomicsWaitMode::kAllow;

  auto devtools_params = std::make_unique<WorkerDevToolsParams>();
  devtools_params->devtools_worker_token = devtools_worker_token;
  devtools_params->wait_for_debugger = pause_worker_context_on_start;
  mojo::PendingRemote<mojom::blink::DevToolsAgent> devtools_agent_remote;
  devtools_params->agent_receiver =
      devtools_agent_remote.InitWithNewPipeAndPassReceiver();
  mojo::PendingReceiver<mojom::blink::DevToolsAgentHost>
      devtools_agent_host_receiver =
          devtools_params->agent_host_remote.InitWithNewPipeAndPassReceiver();

  GetWorkerThread()->Start(std::move(creation_params), thread_startup_data,
                           std::move(devtools_params));
  GetWorkerThread()->FetchAndRunClassicScript(
      script_request_url, outside_settings_object->CopyData(),
      nullptr /* outside_resource_timing_notifier */,
      v8_inspector::V8StackTraceId());

  // We are now ready to inspect worker thread.
  client_->WorkerReadyForInspection(devtools_agent_remote.PassPipe(),
                                    devtools_agent_host_receiver.PassPipe());
}

void WebSharedWorkerImpl::TerminateWorkerContext() {
  DCHECK(IsMainThread());
  TerminateWorkerThread();
}

std::unique_ptr<WebSharedWorker> WebSharedWorker::Create(
    WebSharedWorkerClient* client) {
  return base::WrapUnique(new WebSharedWorkerImpl(client));
}

}  // namespace blink
