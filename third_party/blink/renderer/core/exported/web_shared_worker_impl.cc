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
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_network_provider.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_worker_fetch_context.h"
#include "third_party/blink/public/web/devtools_agent.mojom-blink.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_cache_options.h"
#include "third_party/blink/renderer/core/core_initializer.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/worker_fetch_context.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/parent_execution_context_task_runners.h"
#include "third_party/blink/renderer/core/workers/shared_worker_content_settings_proxy.h"
#include "third_party/blink/renderer/core/workers/shared_worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/shared_worker_thread.h"
#include "third_party/blink/renderer/core/workers/worker_classic_script_loader.h"
#include "third_party/blink/renderer/core/workers/worker_content_settings_client.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_inspector_proxy.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_parsers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

WebSharedWorkerImpl::WebSharedWorkerImpl(WebSharedWorkerClient* client)
    : worker_inspector_proxy_(WorkerInspectorProxy::Create()),
      client_(client),
      creation_address_space_(mojom::IPAddressSpace::kPublic),
      parent_execution_context_task_runners_(
          ParentExecutionContextTaskRunners::Create()),
      weak_ptr_factory_(this) {
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
  if (shadow_page_ && !shadow_page_->WasInitialized()) {
    client_->WorkerScriptLoadFailed();
    // |this| is deleted at this point.
    return;
  }
  if (main_script_loader_) {
    main_script_loader_->Cancel();
    main_script_loader_ = nullptr;
    client_->WorkerScriptLoadFailed();
    // |this| is deleted at this point.
    return;
  }
  if (worker_thread_)
    worker_thread_->Terminate();
  worker_inspector_proxy_->WorkerThreadTerminated();
}

std::unique_ptr<WebApplicationCacheHost>
WebSharedWorkerImpl::CreateApplicationCacheHost(
    WebApplicationCacheHostClient* appcache_host_client) {
  DCHECK(IsMainThread());
  return client_->CreateApplicationCacheHost(appcache_host_client);
}

void WebSharedWorkerImpl::OnShadowPageInitialized() {
  DCHECK(IsMainThread());
  DCHECK(!main_script_loader_);
  shadow_page_->DocumentLoader()->SetServiceWorkerNetworkProvider(
      client_->CreateServiceWorkerNetworkProvider());
  main_script_loader_ = WorkerClassicScriptLoader::Create();

  network::mojom::FetchRequestMode fetch_request_mode =
      network::mojom::FetchRequestMode::kSameOrigin;
  network::mojom::FetchCredentialsMode fetch_credentials_mode =
      network::mojom::FetchCredentialsMode::kSameOrigin;

  main_script_loader_->LoadTopLevelScriptAsynchronously(
      *shadow_page_->GetDocument(), script_request_url_,
      mojom::RequestContextType::SHARED_WORKER, fetch_request_mode,
      fetch_credentials_mode, creation_address_space_,
      Bind(&WebSharedWorkerImpl::DidReceiveScriptLoaderResponse,
           WTF::Unretained(this)),
      Bind(&WebSharedWorkerImpl::OnScriptLoaderFinished,
           WTF::Unretained(this)));
  // Do nothing here since OnScriptLoaderFinished() might have been already
  // invoked and |this| might have been deleted at this point.
}

void WebSharedWorkerImpl::ResumeStartup() {
  DCHECK(IsMainThread());
  bool is_paused_on_start = is_paused_on_start_;
  is_paused_on_start_ = false;
  if (is_paused_on_start) {
    // We'll continue in OnShadowPageInitialized().
    shadow_page_->Initialize(script_request_url_);
  }
}

const base::UnguessableToken& WebSharedWorkerImpl::GetDevToolsWorkerToken() {
  return devtools_worker_token_;
}

void WebSharedWorkerImpl::CountFeature(WebFeature feature) {
  DCHECK(IsMainThread());
  client_->CountFeature(feature);
}

void WebSharedWorkerImpl::PostMessageToPageInspector(int session_id,
                                                     const String& message) {
  DCHECK(IsMainThread());
  worker_inspector_proxy_->DispatchMessageFromWorker(session_id, message);
}

void WebSharedWorkerImpl::DidCloseWorkerGlobalScope() {
  DCHECK(IsMainThread());
  client_->WorkerContextClosed();
  TerminateWorkerThread();
}

void WebSharedWorkerImpl::DidTerminateWorkerThread() {
  DCHECK(IsMainThread());
  client_->WorkerContextDestroyed();
  // |this| is deleted at this point.
}

void WebSharedWorkerImpl::Connect(MessagePortChannel web_channel) {
  DCHECK(IsMainThread());
  // The HTML spec requires to queue a connect event using the DOM manipulation
  // task source.
  // https://html.spec.whatwg.org/multipage/workers.html#shared-workers-and-the-sharedworker-interface
  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kDOMManipulation), FROM_HERE,
      CrossThreadBind(&WebSharedWorkerImpl::ConnectTaskOnWorkerThread,
                      WTF::CrossThreadUnretained(this),
                      WTF::Passed(std::move(web_channel))));
}

void WebSharedWorkerImpl::ConnectTaskOnWorkerThread(
    MessagePortChannel channel) {
  // Wrap the passed-in channel in a MessagePort, and send it off via a connect
  // event.
  DCHECK(worker_thread_->IsCurrentThread());
  auto* scope = To<SharedWorkerGlobalScope>(worker_thread_->GlobalScope());
  scope->ConnectPausable(std::move(channel));
}

void WebSharedWorkerImpl::StartWorkerContext(
    const WebURL& script_request_url,
    const WebString& name,
    const WebString& content_security_policy,
    WebContentSecurityPolicyType policy_type,
    mojom::IPAddressSpace creation_address_space,
    const base::UnguessableToken& devtools_worker_token,
    PrivacyPreferences privacy_preferences,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    mojo::ScopedMessagePipeHandle content_settings_handle,
    mojo::ScopedMessagePipeHandle interface_provider) {
  DCHECK(IsMainThread());
  script_request_url_ = script_request_url;
  name_ = name;
  creation_address_space_ = creation_address_space;
  // Chrome doesn't use interface versioning.
  content_settings_info_ = mojom::blink::WorkerContentSettingsProxyPtrInfo(
      std::move(content_settings_handle), 0u);
  pending_interface_provider_.set_handle(std::move(interface_provider));

  devtools_worker_token_ = devtools_worker_token;
  // |shadow_page_| must be created after |devtools_worker_token_| because it
  // triggers creation of a InspectorNetworkAgent that tries to access the
  // token.
  shadow_page_ =
      std::make_unique<WorkerShadowPage>(this, std::move(loader_factory),
                                         std::move(privacy_preferences));

  // If we were asked to pause worker context on start and wait for debugger
  // then now is a good time to do that.
  client_->WorkerReadyForInspection();
  if (pause_worker_context_on_start_) {
    is_paused_on_start_ = true;
    return;
  }

  // We'll continue in OnShadowPageInitialized().
  shadow_page_->Initialize(script_request_url_);
}

void WebSharedWorkerImpl::DidReceiveScriptLoaderResponse() {
  DCHECK(IsMainThread());
  probe::didReceiveScriptResponse(shadow_page_->GetDocument(),
                                  main_script_loader_->Identifier());
  client_->SelectAppCacheID(main_script_loader_->AppCacheID());
}

void WebSharedWorkerImpl::OnScriptLoaderFinished() {
  DCHECK(IsMainThread());
  DCHECK(main_script_loader_);
  if (asked_to_terminate_)
    return;
  if (main_script_loader_->Failed()) {
    main_script_loader_->Cancel();
    client_->WorkerScriptLoadFailed();
    // |this| is deleted at this point.
    return;
  }

  // S13nServiceWorker: The browser process is expected to send a
  // SetController IPC before sending the script response, but there is no
  // guarantee of the ordering as the messages arrive on different message
  // pipes. Wait for the SetController IPC to be received before starting the
  // worker; otherwise fetches from the worker might not go through the
  // appropriate controller.
  //
  // (For non-S13nServiceWorker, we don't need to do this step as the controller
  // service worker isn't used directly by the renderer, but to minimize code
  // differences between the flags just do it anyway.)
  client_->WaitForServiceWorkerControllerInfo(
      shadow_page_->DocumentLoader()->GetServiceWorkerNetworkProvider(),
      WTF::Bind(&WebSharedWorkerImpl::ContinueOnScriptLoaderFinished,
                weak_ptr_factory_.GetWeakPtr()));
}

void WebSharedWorkerImpl::ContinueOnScriptLoaderFinished() {
  DCHECK(IsMainThread());
  DCHECK(main_script_loader_);
  DCHECK(!main_script_loader_->Failed());
  if (asked_to_terminate_)
    return;

  // FIXME: this document's origin is pristine and without any extra privileges
  // (e.g. GrantUniversalAccess) that can be overriden in regular documents
  // via WebPreference by embedders. (crbug.com/254993)
  Document* document = shadow_page_->GetDocument();
  const SecurityOrigin* starter_origin = document->GetSecurityOrigin();
  bool starter_secure_context = document->IsSecureContext();

  WorkerClients* worker_clients = WorkerClients::Create();
  CoreInitializer::GetInstance().ProvideLocalFileSystemToWorker(
      *worker_clients);
  CoreInitializer::GetInstance().ProvideIndexedDBClientToWorker(
      *worker_clients);

  ProvideContentSettingsClientToWorker(
      worker_clients, std::make_unique<SharedWorkerContentSettingsProxy>(
                          std::move(content_settings_info_)));

  std::unique_ptr<WebWorkerFetchContext> web_worker_fetch_context =
      client_->CreateWorkerFetchContext(
          shadow_page_->DocumentLoader()->GetServiceWorkerNetworkProvider());
  DCHECK(web_worker_fetch_context);
  web_worker_fetch_context->SetApplicationCacheHostID(
      shadow_page_->GetDocument()
          ->Fetcher()
          ->Context()
          .ApplicationCacheHostID());
  ProvideWorkerFetchContextToWorker(worker_clients,
                                    std::move(web_worker_fetch_context));

  ContentSecurityPolicy* content_security_policy =
      main_script_loader_->GetContentSecurityPolicy();
  ReferrerPolicy referrer_policy = kReferrerPolicyDefault;
  if (!main_script_loader_->GetReferrerPolicy().IsNull()) {
    SecurityPolicy::ReferrerPolicyFromHeaderValue(
        main_script_loader_->GetReferrerPolicy(),
        kDoNotSupportReferrerPolicyLegacyKeywords, &referrer_policy);
  }
  auto worker_settings = std::make_unique<WorkerSettings>(
      shadow_page_->GetDocument()->GetFrame()->GetSettings());

  // TODO(nhiroki); Set |script_type| to ScriptType::kModule for module fetch.
  // (https://crbug.com/824646)
  ScriptType script_type = ScriptType::kClassic;

  const KURL script_response_url = main_script_loader_->ResponseURL();
  DCHECK(static_cast<KURL>(script_request_url_) == script_response_url ||
         SecurityOrigin::AreSameSchemeHostPort(script_request_url_,
                                               script_response_url));

  auto global_scope_creation_params =
      std::make_unique<GlobalScopeCreationParams>(
          script_response_url, script_type, document->UserAgent(),
          content_security_policy ? content_security_policy->Headers()
                                  : Vector<CSPHeaderAndType>(),
          referrer_policy, starter_origin, starter_secure_context,
          document->GetHttpsState(), worker_clients,
          main_script_loader_->ResponseAddressSpace(),
          main_script_loader_->OriginTrialTokens(), devtools_worker_token_,
          std::move(worker_settings), kV8CacheOptionsDefault,
          nullptr /* worklet_module_response_map */,
          std::move(pending_interface_provider_));
  String source_code = main_script_loader_->SourceText();

  reporting_proxy_ = new SharedWorkerReportingProxy(
      this, parent_execution_context_task_runners_);
  worker_thread_ =
      std::make_unique<SharedWorkerThread>(name_, *reporting_proxy_);
  probe::scriptImported(document, main_script_loader_->Identifier(),
                        main_script_loader_->SourceText());
  main_script_loader_ = nullptr;

  auto thread_startup_data = WorkerBackingThreadStartupData::CreateDefault();
  thread_startup_data.atomics_wait_mode =
      WorkerBackingThreadStartupData::AtomicsWaitMode::kAllow;

  GetWorkerThread()->Start(
      std::move(global_scope_creation_params), thread_startup_data,
      worker_inspector_proxy_->ShouldPauseOnWorkerStart(document),
      parent_execution_context_task_runners_);
  worker_inspector_proxy_->WorkerThreadCreated(document, GetWorkerThread(),
                                               script_response_url);
  // TODO(nhiroki): Support module workers (https://crbug.com/680046).
  // Shared worker is origin-bound, so use kSharableCrossOrigin.
  GetWorkerThread()->EvaluateClassicScript(
      script_response_url, kSharableCrossOrigin, source_code,
      nullptr /* cached_meta_data */, v8_inspector::V8StackTraceId());
  client_->WorkerScriptLoaded();
}

void WebSharedWorkerImpl::TerminateWorkerContext() {
  DCHECK(IsMainThread());
  TerminateWorkerThread();
}

void WebSharedWorkerImpl::PauseWorkerContextOnStart() {
  pause_worker_context_on_start_ = true;
}

void WebSharedWorkerImpl::BindDevToolsAgent(
    mojo::ScopedInterfaceEndpointHandle devtools_agent_host_ptr_info,
    mojo::ScopedInterfaceEndpointHandle devtools_agent_request) {
  shadow_page_->DevToolsAgent()->BindRequest(
      mojom::blink::DevToolsAgentHostAssociatedPtrInfo(
          std::move(devtools_agent_host_ptr_info),
          mojom::blink::DevToolsAgentHost::Version_),
      mojom::blink::DevToolsAgentAssociatedRequest(
          std::move(devtools_agent_request)));
}

scoped_refptr<base::SingleThreadTaskRunner> WebSharedWorkerImpl::GetTaskRunner(
    TaskType task_type) {
  return parent_execution_context_task_runners_->Get(task_type);
}

std::unique_ptr<WebSharedWorker> WebSharedWorker::Create(
    WebSharedWorkerClient* client) {
  return base::WrapUnique(new WebSharedWorkerImpl(client));
}

}  // namespace blink
