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
#include <utility>

#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "third_party/blink/public/common/loader/worker_main_script_load_parameters.h"
#include "third_party/blink/public/mojom/browser_interface_broker.mojom-blink.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/fetch_client_settings_object.mojom-blink.h"
#include "third_party/blink/public/mojom/script/script_type.mojom-blink.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-blink.h"
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

WebSharedWorkerImpl::WebSharedWorkerImpl(
    const blink::SharedWorkerToken& token,
    const base::UnguessableToken& appcache_host_id,
    CrossVariantMojoRemote<mojom::SharedWorkerHostInterfaceBase> host,
    WebSharedWorkerClient* client,
    ukm::SourceId ukm_source_id)
    : reporting_proxy_(MakeGarbageCollected<SharedWorkerReportingProxy>(
          this,
          ParentExecutionContextTaskRunners::Create())),
      worker_thread_(std::make_unique<SharedWorkerThread>(*reporting_proxy_,
                                                          token,
                                                          appcache_host_id,
                                                          ukm_source_id)),
      host_(std::move(host)),
      client_(client) {
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
  pending_channels_.clear();
  worker_thread_->Terminate();
  // DidTerminateWorkerThread() will be called asynchronously.
}

void WebSharedWorkerImpl::CountFeature(WebFeature feature) {
  DCHECK(IsMainThread());
  host_->OnFeatureUsed(feature);
}

void WebSharedWorkerImpl::DidFailToFetchClassicScript() {
  DCHECK(IsMainThread());
  host_->OnScriptLoadFailed("Failed to fetch a worker script.");
  TerminateWorkerThread();
  // DidTerminateWorkerThread() will be called asynchronously.
}

void WebSharedWorkerImpl::DidFailToFetchModuleScript() {
  DCHECK(IsMainThread());
  host_->OnScriptLoadFailed("Failed to fetch a worker script.");
  TerminateWorkerThread();
  // DidTerminateWorkerThread() will be called asynchronously.
}

void WebSharedWorkerImpl::DidEvaluateTopLevelScript(bool success) {
  DCHECK(IsMainThread());
  DCHECK(!running_);
  running_ = true;
  DispatchPendingConnections();
}

void WebSharedWorkerImpl::DidCloseWorkerGlobalScope() {
  DCHECK(IsMainThread());
  host_->OnContextClosed();
  TerminateWorkerThread();
  // DidTerminateWorkerThread() will be called asynchronously.
}

void WebSharedWorkerImpl::DidTerminateWorkerThread() {
  DCHECK(IsMainThread());
  client_->WorkerContextDestroyed();
  // |this| is deleted at this point.
}

void WebSharedWorkerImpl::Connect(int connection_request_id,
                                  MessagePortDescriptor port) {
  DCHECK(IsMainThread());
  if (asked_to_terminate_)
    return;

  blink::MessagePortChannel channel(std::move(port));
  if (running_) {
    ConnectToChannel(connection_request_id, std::move(channel));
  } else {
    // If two documents try to load a SharedWorker at the same time, the
    // mojom::SharedWorker::Connect() for one of the documents can come in
    // before the worker is started. Just queue up the connect and deliver it
    // once the worker starts.
    pending_channels_.emplace_back(connection_request_id, std::move(channel));
  }
}

void WebSharedWorkerImpl::ConnectToChannel(int connection_request_id,
                                           MessagePortChannel channel) {
  DCHECK(IsMainThread());
  PostCrossThreadTask(
      *task_runner_for_connect_event_, FROM_HERE,
      CrossThreadBindOnce(&WebSharedWorkerImpl::ConnectTaskOnWorkerThread,
                          WTF::CrossThreadUnretained(this),
                          WTF::Passed(std::move(channel))));
  host_->OnConnected(connection_request_id);
}

void WebSharedWorkerImpl::DispatchPendingConnections() {
  DCHECK(IsMainThread());
  for (auto& item : pending_channels_)
    ConnectToChannel(item.first, std::move(item.second));
  pending_channels_.clear();
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
    mojom::ScriptType script_type,
    network::mojom::CredentialsMode credentials_mode,
    const WebString& name,
    WebSecurityOrigin constructor_origin,
    const WebString& user_agent,
    const UserAgentMetadata& ua_metadata,
    const WebString& content_security_policy,
    network::mojom::ContentSecurityPolicyType policy_type,
    network::mojom::IPAddressSpace creation_address_space,
    const WebFetchClientSettingsObject& outside_fetch_client_settings_object,
    const base::UnguessableToken& devtools_worker_token,
    CrossVariantMojoRemote<
        mojom::blink::WorkerContentSettingsProxyInterfaceBase> content_settings,
    CrossVariantMojoRemote<mojom::blink::BrowserInterfaceBrokerInterfaceBase>
        browser_interface_broker,
    bool pause_worker_context_on_start,
    std::unique_ptr<WorkerMainScriptLoadParameters>
        worker_main_script_load_params,
    scoped_refptr<WebWorkerFetchContext> web_worker_fetch_context) {
  DCHECK(IsMainThread());
  DCHECK(web_worker_fetch_context);
  CHECK(constructor_origin.Get()->CanAccessSharedWorkers());

  // Creates 'outside settings' used in the "Processing model" algorithm in the
  // HTML spec:
  // https://html.spec.whatwg.org/C/#worker-processing-model
  auto* outside_settings_object =
      MakeGarbageCollected<FetchClientSettingsObjectSnapshot>(
          /*global_object_url=*/script_request_url,
          /*base_url=*/script_request_url, constructor_origin,
          outside_fetch_client_settings_object.referrer_policy,
          outside_fetch_client_settings_object.outgoing_referrer.GetString(),
          CalculateHttpsState(constructor_origin.Get()),
          AllowedByNosniff::MimeTypeCheck::kLaxForWorker,
          creation_address_space,
          outside_fetch_client_settings_object.insecure_requests_policy ==
                  mojom::blink::InsecureRequestsPolicy::kUpgrade
              ? mojom::blink::InsecureRequestPolicy::kUpgradeInsecureRequests |
                    mojom::blink::InsecureRequestPolicy::kBlockAllMixedContent
              : mojom::blink::InsecureRequestPolicy::kBlockAllMixedContent,
          FetchClientSettingsObject::InsecureNavigationsSet());

  bool constructor_secure_context =
      constructor_origin.IsPotentiallyTrustworthy() ||
      SchemeRegistry::SchemeShouldBypassSecureContextCheck(
          constructor_origin.Protocol());

  auto worker_settings = std::make_unique<WorkerSettings>(
      false /* disable_reading_from_canvas */,
      false /* strict_mixed_content_checking */,
      true /* allow_running_of_insecure_content */,
      false /* strictly_block_blockable_mixed_content */,
      GenericFontFamilySettings());

  // CSP headers for parent Window's CSP.
  Vector<CSPHeaderAndType> outside_csp_headers;
  outside_csp_headers.ReserveInitialCapacity(1);
  outside_csp_headers.UncheckedAppend(
      CSPHeaderAndType(content_security_policy, policy_type));

  // Some params (e.g. address space) passed to GlobalScopeCreationParams are
  // dummy values. They will be updated after worker script fetch on the worker
  // thread.
  auto creation_params = std::make_unique<GlobalScopeCreationParams>(
      script_request_url, script_type, name, user_agent, ua_metadata,
      std::move(web_worker_fetch_context), outside_csp_headers,
      outside_settings_object->GetReferrerPolicy(),
      outside_settings_object->GetSecurityOrigin(), constructor_secure_context,
      outside_settings_object->GetHttpsState(),
      MakeGarbageCollected<WorkerClients>(),
      std::make_unique<SharedWorkerContentSettingsProxy>(
          std::move(content_settings)),
      base::nullopt /* response_address_space */,
      nullptr /* origin_trial_tokens */, devtools_worker_token,
      std::move(worker_settings), kV8CacheOptionsDefault,
      nullptr /* worklet_module_response_map */,
      std::move(browser_interface_broker), BeginFrameProviderParams(),
      nullptr /* parent_feature_policy */, base::UnguessableToken());

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

  // Capture the task runner for dispatching connect events. This is necessary
  // for avoiding race condition with WorkerScheduler termination induced by
  // close() call on SharedWorkerGlobalScope. See https://crbug.com/1104046 for
  // details.
  //
  // The HTML spec requires to queue a connect event using the DOM manipulation
  // task source.
  // https://html.spec.whatwg.org/C/#shared-workers-and-the-sharedworker-interface
  task_runner_for_connect_event_ =
      GetWorkerThread()->GetTaskRunner(TaskType::kDOMManipulation);

  switch (script_type) {
    case mojom::ScriptType::kClassic:
      GetWorkerThread()->FetchAndRunClassicScript(
          script_request_url, std::move(worker_main_script_load_params),
          outside_settings_object->CopyData(),
          nullptr /* outside_resource_timing_notifier */,
          v8_inspector::V8StackTraceId());
      break;
    case mojom::ScriptType::kModule:
      GetWorkerThread()->FetchAndRunModuleScript(
          script_request_url, std::move(worker_main_script_load_params),
          outside_settings_object->CopyData(),
          nullptr /* outside_resource_timing_notifier */, credentials_mode);
      break;
  }

  // We are now ready to inspect worker thread.
  host_->OnReadyForInspection(std::move(devtools_agent_remote),
                              std::move(devtools_agent_host_receiver));
}

void WebSharedWorkerImpl::TerminateWorkerContext() {
  DCHECK(IsMainThread());
  TerminateWorkerThread();
}

std::unique_ptr<WebSharedWorker> WebSharedWorker::CreateAndStart(
    const blink::SharedWorkerToken& token,
    const WebURL& script_request_url,
    mojom::ScriptType script_type,
    network::mojom::CredentialsMode credentials_mode,
    const WebString& name,
    WebSecurityOrigin constructor_origin,
    const WebString& user_agent,
    const UserAgentMetadata& ua_metadata,
    const WebString& content_security_policy,
    network::mojom::ContentSecurityPolicyType policy_type,
    network::mojom::IPAddressSpace creation_address_space,
    const WebFetchClientSettingsObject& outside_fetch_client_settings_object,
    const base::UnguessableToken& appcache_host_id,
    const base::UnguessableToken& devtools_worker_token,
    CrossVariantMojoRemote<
        mojom::blink::WorkerContentSettingsProxyInterfaceBase> content_settings,
    CrossVariantMojoRemote<mojom::blink::BrowserInterfaceBrokerInterfaceBase>
        browser_interface_broker,
    bool pause_worker_context_on_start,
    std::unique_ptr<WorkerMainScriptLoadParameters>
        worker_main_script_load_params,
    scoped_refptr<WebWorkerFetchContext> web_worker_fetch_context,
    CrossVariantMojoRemote<mojom::SharedWorkerHostInterfaceBase> host,
    WebSharedWorkerClient* client,
    ukm::SourceId ukm_source_id) {
  auto worker = base::WrapUnique(new WebSharedWorkerImpl(
      token, appcache_host_id, std::move(host), client, ukm_source_id));
  worker->StartWorkerContext(
      script_request_url, script_type, credentials_mode, name,
      constructor_origin, user_agent, ua_metadata, content_security_policy,
      policy_type, creation_address_space, outside_fetch_client_settings_object,
      devtools_worker_token, std::move(content_settings),
      std::move(browser_interface_broker), pause_worker_context_on_start,
      std::move(worker_main_script_load_params),
      std::move(web_worker_fetch_context));
  return worker;
}

}  // namespace blink
