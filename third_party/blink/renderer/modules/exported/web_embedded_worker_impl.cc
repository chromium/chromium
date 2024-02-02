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

#include "third_party/blink/renderer/modules/exported/web_embedded_worker_impl.h"

#include <memory>
#include <utility>
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/referrer_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/browser_interface_broker.mojom-blink.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom-blink.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_installed_scripts_manager.mojom-blink.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_network_provider.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_worker_fetch_context.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_client.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/inspector/worker_devtools_params.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/worker_fetch_context.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread_startup_data.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope_proxy.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_installed_scripts_manager.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_thread.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

WebServiceWorkerInstalledScriptsManagerParams::
    WebServiceWorkerInstalledScriptsManagerParams(
        WebVector<WebURL> installed_scripts_urls,
        CrossVariantMojoReceiver<
            mojom::blink::ServiceWorkerInstalledScriptsManagerInterfaceBase>
            manager_receiver,
        CrossVariantMojoRemote<
            mojom::blink::ServiceWorkerInstalledScriptsManagerHostInterfaceBase>
            manager_host_remote)
    : installed_scripts_urls(std::move(installed_scripts_urls)),
      manager_receiver(std::move(manager_receiver)),
      manager_host_remote(std::move(manager_host_remote)) {
  DCHECK(!this->installed_scripts_urls.empty());
  DCHECK(this->manager_receiver);
  DCHECK(this->manager_host_remote);
}

// static
std::unique_ptr<WebEmbeddedWorker> WebEmbeddedWorker::Create(
    WebServiceWorkerContextClient* client) {
  return std::make_unique<WebEmbeddedWorkerImpl>(std::move(client));
}

WebEmbeddedWorkerImpl::WebEmbeddedWorkerImpl(
    WebServiceWorkerContextClient* client)
    : worker_context_client_(client) {}

WebEmbeddedWorkerImpl::~WebEmbeddedWorkerImpl() {
  // TerminateWorkerContext() must be called before the destructor.
  DCHECK(asked_to_terminate_);
}

void WebEmbeddedWorkerImpl::StartWorkerContext(
    std::unique_ptr<WebEmbeddedWorkerStartData> worker_start_data,
    std::unique_ptr<WebServiceWorkerInstalledScriptsManagerParams>
        installed_scripts_manager_params,
    CrossVariantMojoRemote<
        mojom::blink::WorkerContentSettingsProxyInterfaceBase> content_settings,
    CrossVariantMojoRemote<mojom::blink::CacheStorageInterfaceBase>
        cache_storage,
    CrossVariantMojoRemote<mojom::blink::BrowserInterfaceBrokerInterfaceBase>
        browser_interface_broker,
    InterfaceRegistry* interface_registry,
    scoped_refptr<base::SingleThreadTaskRunner> initiator_thread_task_runner) {
  DCHECK(!asked_to_terminate_);

  std::unique_ptr<ServiceWorkerInstalledScriptsManager>
      installed_scripts_manager;
  if (installed_scripts_manager_params) {
    installed_scripts_manager =
        std::make_unique<ServiceWorkerInstalledScriptsManager>(
            std::move(installed_scripts_manager_params),
            Platform::Current()->GetIOTaskRunner());
  }

  StartWorkerThread(
      std::move(worker_start_data), std::move(installed_scripts_manager),
      std::make_unique<ServiceWorkerContentSettingsProxy>(
          std::move(content_settings)),
      std::move(cache_storage), std::move(browser_interface_broker),
      interface_registry, std::move(initiator_thread_task_runner));
}

void WebEmbeddedWorkerImpl::TerminateWorkerContext() {
  if (asked_to_terminate_)
    return;
  asked_to_terminate_ = true;
  // StartWorkerThread() must be called before.
  DCHECK(worker_thread_);
  worker_thread_->Terminate();
}

void WebEmbeddedWorkerImpl::StartWorkerThread(
    std::unique_ptr<WebEmbeddedWorkerStartData> worker_start_data,
    std::unique_ptr<ServiceWorkerInstalledScriptsManager>
        installed_scripts_manager,
    std::unique_ptr<ServiceWorkerContentSettingsProxy> content_settings_proxy,
    mojo::PendingRemote<mojom::blink::CacheStorage> cache_storage_remote,
    mojo::PendingRemote<mojom::blink::BrowserInterfaceBroker>
        browser_interface_broker,
    InterfaceRegistry* interface_registry,
    scoped_refptr<base::SingleThreadTaskRunner> initiator_thread_task_runner) {
  DCHECK(!asked_to_terminate_);

  // For now we don't use global scope name for service workers.
  const String global_scope_name = g_empty_string;

  // TODO(crbug.com/967265,937177): Plumb these starter parameters from an
  // appropriate Document. See comment in CreateFetchClientSettingsObject() for
  // details.
  scoped_refptr<const SecurityOrigin> starter_origin =
      SecurityOrigin::Create(worker_start_data->script_url);
  // This roughly equals to shadow document's IsSecureContext() as a shadow
  // document have a frame with no parent.
  // See also Document::InitSecureContextState().
  bool starter_secure_context =
      starter_origin->IsPotentiallyTrustworthy() ||
      SchemeRegistry::SchemeShouldBypassSecureContextCheck(
          starter_origin->Protocol());
  const HttpsState starter_https_state =
      CalculateHttpsState(starter_origin.get());

  scoped_refptr<WebWorkerFetchContext> web_worker_fetch_context =
      worker_context_client_->CreateWorkerFetchContextOnInitiatorThread();

  // Create WorkerSettings. Currently we block all mixed-content requests from
  // a ServiceWorker.
  // TODO(bashi): Set some of these settings from WebPreferences. We may want
  // to propagate and update these settings from the browser process in a way
  // similar to mojom::RendererPreference{Watcher}.
  auto worker_settings = std::make_unique<WorkerSettings>(
      false /* disable_reading_from_canvas */,
      true /* strict_mixed_content_checking */,
      false /* allow_running_of_insecure_content */,
      false /* strictly_block_blockable_mixed_content */,
      GenericFontFamilySettings());

  std::unique_ptr<GlobalScopeCreationParams> global_scope_creation_params;
  String source_code;
  std::unique_ptr<Vector<uint8_t>> cached_meta_data;

  // We don't have to set ContentSecurityPolicy and ReferrerPolicy. They're
  // served by the worker script loader or the installed scripts manager on the
  // worker thread.
  global_scope_creation_params = std::make_unique<GlobalScopeCreationParams>(
      worker_start_data->script_url, worker_start_data->script_type,
      global_scope_name, worker_start_data->user_agent,
      worker_start_data->ua_metadata, std::move(web_worker_fetch_context),
      Vector<network::mojom::blink::ContentSecurityPolicyPtr>(),
      Vector<network::mojom::blink::ContentSecurityPolicyPtr>(),
      network::mojom::ReferrerPolicy::kDefault, starter_origin.get(),
      starter_secure_context, starter_https_state, nullptr /* worker_clients */,
      std::move(content_settings_proxy), nullptr /* inherited_trial_features */,
      worker_start_data->devtools_worker_token, std::move(worker_settings),
      // Generate the full code cache in the first execution of the script.
      mojom::blink::V8CacheOptions::kFullCodeWithoutHeatCheck,
      nullptr /* worklet_module_respones_map */,
      std::move(browser_interface_broker),
      mojo::NullRemote() /* code_cache_host_interface */,
      mojo::NullRemote() /* blob_url_store */, BeginFrameProviderParams(),
      nullptr /* parent_permissions_policy */,
      base::UnguessableToken() /* agent_cluster_id */,
      worker_start_data->ukm_source_id, std::nullopt, /* parent_context_token */
      false, /* parent_cross_origin_isolated_capability */
      false, /* parent_is_isolated_context */
      interface_registry);

  worker_thread_ = std::make_unique<ServiceWorkerThread>(
      std::make_unique<ServiceWorkerGlobalScopeProxy>(
          *this, *worker_context_client_, initiator_thread_task_runner),
      std::move(installed_scripts_manager), std::move(cache_storage_remote),
      initiator_thread_task_runner, worker_start_data->service_worker_token);

  auto devtools_params = std::make_unique<WorkerDevToolsParams>();
  devtools_params->devtools_worker_token =
      worker_start_data->devtools_worker_token;
  devtools_params->wait_for_debugger =
      worker_start_data->wait_for_debugger_mode ==
      WebEmbeddedWorkerStartData::kWaitForDebugger;
  mojo::PendingRemote<mojom::blink::DevToolsAgent> devtools_agent_remote;
  devtools_params->agent_receiver =
      devtools_agent_remote.InitWithNewPipeAndPassReceiver();
  mojo::PendingReceiver<mojom::blink::DevToolsAgentHost>
      devtools_agent_host_receiver =
          devtools_params->agent_host_remote.InitWithNewPipeAndPassReceiver();

  worker_thread_->Start(std::move(global_scope_creation_params),
                        WorkerBackingThreadStartupData::CreateDefault(),
                        std::move(devtools_params));

  std::unique_ptr<CrossThreadFetchClientSettingsObjectData>
      fetch_client_setting_object_data = CreateFetchClientSettingsObjectData(
          worker_start_data->script_url, starter_origin.get(),
          starter_https_state,
          worker_start_data->outside_fetch_client_settings_object);

  // > Switching on job's worker type, run these substeps with the following
  // > options:
  // https://w3c.github.io/ServiceWorker/#update-algorithm
  switch (worker_start_data->script_type) {
    // > "classic": Fetch a classic worker script given job's serialized script
    // > url, job's client, "serviceworker", and the to-be-created environment
    // > settings object for this service worker.
    case mojom::blink::ScriptType::kClassic:
      worker_thread_->FetchAndRunClassicScript(
          worker_start_data->script_url,
          std::move(worker_start_data->main_script_load_params),
          std::move(worker_start_data->policy_container),
          std::move(fetch_client_setting_object_data),
          nullptr /* outside_resource_timing_notifier */,
          v8_inspector::V8StackTraceId());
      break;

    // > "module": Fetch a module worker script graph given job’s serialized
    // > script url, job’s client, "serviceworker", "omit", and the
    // > to-be-created environment settings object for this service worker.
    case mojom::blink::ScriptType::kModule:
      worker_thread_->FetchAndRunModuleScript(
          worker_start_data->script_url,
          std::move(worker_start_data->main_script_load_params),
          std::move(worker_start_data->policy_container),
          std::move(fetch_client_setting_object_data),
          nullptr /* outside_resource_timing_notifier */,
          network::mojom::CredentialsMode::kOmit);
      break;
  }

  // We are now ready to inspect worker thread.
  worker_context_client_->WorkerReadyForInspectionOnInitiatorThread(
      std::move(devtools_agent_remote),
      std::move(devtools_agent_host_receiver));
}

std::unique_ptr<CrossThreadFetchClientSettingsObjectData>
WebEmbeddedWorkerImpl::CreateFetchClientSettingsObjectData(
    const KURL& script_url,
    const SecurityOrigin* security_origin,
    const HttpsState& https_state,
    const WebFetchClientSettingsObject& passed_settings_object) {
  // TODO(crbug.com/967265): Currently |passed_settings_object| doesn't contain
  // enough parameters to create a complete outside settings object. Pass
  // all necessary information from the parent execution context.
  // For new worker case, the parent is the Document that called
  // navigator.serviceWorker.register(). For ServiceWorkerRegistration#update()
  // case, it should be the Document that called update(). For soft update case,
  // it seems to be 'null' document.

  mojom::blink::InsecureRequestPolicy insecure_requests_policy =
      passed_settings_object.insecure_requests_policy ==
              mojom::InsecureRequestsPolicy::kUpgrade
          ? mojom::blink::InsecureRequestPolicy::kUpgradeInsecureRequests
          : mojom::blink::InsecureRequestPolicy::kBlockAllMixedContent;

  return std::make_unique<CrossThreadFetchClientSettingsObjectData>(
      script_url /* global_object_url */, script_url /* base_url */,
      security_origin->IsolatedCopy(), passed_settings_object.referrer_policy,
      KURL(passed_settings_object.outgoing_referrer.GetString()), https_state,
      AllowedByNosniff::MimeTypeCheck::kLaxForWorker, insecure_requests_policy,
      FetchClientSettingsObject::InsecureNavigationsSet());
}

void WebEmbeddedWorkerImpl::WaitForShutdownForTesting() {
  DCHECK(worker_thread_);
  worker_thread_->WaitForShutdownForTesting();
}

}  // namespace blink
