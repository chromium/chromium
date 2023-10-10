// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/embedded_worker_instance_client_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "content/child/child_thread_impl.h"
#include "content/child/scoped_child_process_reference.h"
#include "content/public/common/content_client.h"
#include "content/renderer/policy_container_util.h"
#include "content/renderer/service_worker/service_worker_context_client.h"
#include "content/renderer/worker/fetch_client_settings_object_helpers.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/worker_main_script_load_parameters.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_embedded_worker.h"
#include "third_party/blink/public/web/web_embedded_worker_start_data.h"

namespace content {

// static
void EmbeddedWorkerInstanceClientImpl::Create(
    scoped_refptr<base::SingleThreadTaskRunner> initiator_thread_task_runner,
    const std::vector<std::string>& cors_exempt_header_list,
    mojo::PendingReceiver<blink::mojom::EmbeddedWorkerInstanceClient>
        receiver) {
  // This won't be leaked because the lifetime will be managed internally.
  // See the class documentation for detail.
  // We can't use MakeSelfOwnedReceiver because must give the worker thread
  // a chance to stop by calling TerminateWorkerContext() and waiting
  // before destructing.
  new EmbeddedWorkerInstanceClientImpl(std::move(receiver),
                                       std::move(initiator_thread_task_runner),
                                       cors_exempt_header_list);
}

void EmbeddedWorkerInstanceClientImpl::WorkerContextDestroyed() {
  DCHECK(initiator_thread_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("ServiceWorker",
               "EmbeddedWorkerInstanceClientImpl::WorkerContextDestroyed");
  delete this;
}

void EmbeddedWorkerInstanceClientImpl::StartWorker(
    blink::mojom::EmbeddedWorkerStartParamsPtr params) {
  DCHECK(initiator_thread_task_runner_->BelongsToCurrentThread());
  DCHECK(!service_worker_context_client_);
  TRACE_EVENT0("ServiceWorker",
               "EmbeddedWorkerInstanceClientImpl::StartWorker");
  auto start_timing = blink::mojom::EmbeddedWorkerStartTiming::New();
  start_timing->start_worker_received_time = base::TimeTicks::Now();

  std::unique_ptr<blink::WebEmbeddedWorkerStartData> start_data =
      BuildStartData(*params);
  if (params->main_script_load_params) {
    start_data->main_script_load_params =
        std::make_unique<blink::WorkerMainScriptLoadParameters>();
    start_data->main_script_load_params->request_id =
        params->main_script_load_params->request_id;
    start_data->main_script_load_params->response_head =
        std::move(params->main_script_load_params->response_head);
    start_data->main_script_load_params->response_body =
        std::move(params->main_script_load_params->response_body);
    start_data->main_script_load_params->redirect_responses =
        std::move(params->main_script_load_params->redirect_response_heads);
    start_data->main_script_load_params->redirect_infos =
        params->main_script_load_params->redirect_infos;
    start_data->main_script_load_params->url_loader_client_endpoints =
        std::move(params->main_script_load_params->url_loader_client_endpoints);
  }
  start_data->policy_container =
      ToWebPolicyContainer(std::move(params->policy_container));

  for (const auto& feature : params->forced_enabled_runtime_features) {
    blink::WebRuntimeFeatures::EnableFeatureFromString(feature, true);
  }

  DCHECK(!params->provider_info->cache_storage ||
         base::FeatureList::IsEnabled(
             blink::features::kEagerCacheStorageSetupForServiceWorkers));
  mojo::PendingRemote<blink::mojom::CacheStorage> cache_storage =
      std::move(params->provider_info->cache_storage);
  mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
      browser_interface_broker =
          std::move(params->provider_info->browser_interface_broker);

  service_worker_context_client_ = std::make_unique<ServiceWorkerContextClient>(
      params->service_worker_version_id, params->scope, params->script_url,
      !params->installed_scripts_info.is_null(),
      std::move(params->renderer_preferences),
      std::move(params->service_worker_receiver),
      std::move(params->controller_receiver), std::move(params->instance_host),
      std::move(params->interface_provider), std::move(params->provider_info),
      this, std::move(start_timing),
      std::move(params->preference_watcher_receiver),
      std::move(params->subresource_loader_factories),
      std::move(params->subresource_loader_updater),
      params->script_url_to_skip_throttling, initiator_thread_task_runner_,
      params->service_worker_route_id, cors_exempt_header_list_,
      params->storage_key);
  // Record UMA to indicate StartWorker is received on renderer.
  StartWorkerHistogramEnum metric =
      params->is_installed ? StartWorkerHistogramEnum::RECEIVED_ON_INSTALLED
                           : StartWorkerHistogramEnum::RECEIVED_ON_UNINSTALLED;
  UMA_HISTOGRAM_ENUMERATION(
      "ServiceWorker.EmbeddedWorkerInstanceClient.StartWorker", metric,
      StartWorkerHistogramEnum::NUM_TYPES);

  std::unique_ptr<blink::WebServiceWorkerInstalledScriptsManagerParams>
      installed_scripts_manager_params;
  // |installed_scripts_info| is null if scripts should be served by net layer,
  // when the worker is not installed, or the worker is launched for checking
  // the update.
  if (params->installed_scripts_info) {
    installed_scripts_manager_params =
        std::make_unique<blink::WebServiceWorkerInstalledScriptsManagerParams>(
            std::move(params->installed_scripts_info->installed_urls),
            std::move(params->installed_scripts_info->manager_receiver),
            std::move(params->installed_scripts_info->manager_host_remote));
  }

  auto worker =
      blink::WebEmbeddedWorker::Create(service_worker_context_client_.get());
  service_worker_context_client_->StartWorkerContextOnInitiatorThread(
      std::move(worker), std::move(start_data),
      std::move(installed_scripts_manager_params),
      std::move(params->content_settings_proxy), std::move(cache_storage),
      std::move(browser_interface_broker));
}

void EmbeddedWorkerInstanceClientImpl::StopWorker() {
  DCHECK(initiator_thread_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("ServiceWorker", "EmbeddedWorkerInstanceClientImpl::StopWorker");
  // StopWorker must be called after StartWorker is called.
  service_worker_context_client_->worker().TerminateWorkerContext();
  // We continue in WorkerContextDestroyed() after the worker thread is stopped.
}

EmbeddedWorkerInstanceClientImpl::EmbeddedWorkerInstanceClientImpl(
    mojo::PendingReceiver<blink::mojom::EmbeddedWorkerInstanceClient> receiver,
    scoped_refptr<base::SingleThreadTaskRunner> initiator_thread_task_runner,
    const std::vector<std::string>& cors_exempt_header_list)
    : receiver_(this, std::move(receiver)),
      initiator_thread_task_runner_(std::move(initiator_thread_task_runner)),
      cors_exempt_header_list_(cors_exempt_header_list) {
  DCHECK(initiator_thread_task_runner_->BelongsToCurrentThread());
  receiver_.set_disconnect_handler(base::BindOnce(
      &EmbeddedWorkerInstanceClientImpl::OnError, base::Unretained(this)));
}

EmbeddedWorkerInstanceClientImpl::~EmbeddedWorkerInstanceClientImpl() {
  DCHECK(initiator_thread_task_runner_->BelongsToCurrentThread());
}

void EmbeddedWorkerInstanceClientImpl::OnError() {
  DCHECK(initiator_thread_task_runner_->BelongsToCurrentThread());
  // The connection to the browser process broke.
  if (service_worker_context_client_) {
    // The worker is running, so tell it to stop. We continue in
    // WorkerContextDestroyed().
    StopWorker();
    return;
  }

  // Nothing left to do.
  delete this;
}

std::unique_ptr<blink::WebEmbeddedWorkerStartData>
EmbeddedWorkerInstanceClientImpl::BuildStartData(
    const blink::mojom::EmbeddedWorkerStartParams& params) {
  DCHECK(initiator_thread_task_runner_->BelongsToCurrentThread());
  auto start_data = std::make_unique<blink::WebEmbeddedWorkerStartData>(
      FetchClientSettingsObjectFromMojomToWeb(
          params.outside_fetch_client_settings_object));

  start_data->script_url = params.script_url;
  start_data->user_agent = blink::WebString::FromUTF8(params.user_agent);
  start_data->ua_metadata = params.ua_metadata;
  start_data->script_type = params.script_type;
  start_data->wait_for_debugger_mode =
      params.wait_for_debugger
          ? blink::WebEmbeddedWorkerStartData::kWaitForDebugger
          : blink::WebEmbeddedWorkerStartData::kDontWaitForDebugger;
  start_data->devtools_worker_token = params.devtools_worker_token;
  start_data->service_worker_token = params.service_worker_token;
  start_data->ukm_source_id = params.ukm_source_id;
  return start_data;
}

}  // namespace content
