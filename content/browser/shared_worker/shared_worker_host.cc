// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_worker/shared_worker_host.h"

#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/unguessable_token.h"
#include "content/browser/appcache/appcache_navigation_handle.h"
#include "content/browser/devtools/shared_worker_devtools_manager.h"
#include "content/browser/interface_provider_filtering.h"
#include "content/browser/renderer_interface_binders.h"
#include "content/browser/shared_worker/shared_worker_content_settings_proxy_impl.h"
#include "content/browser/shared_worker/shared_worker_instance.h"
#include "content/browser/shared_worker/shared_worker_service_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/navigation_subresource_loader_params.h"
#include "content/common/url_loader_factory_bundle.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/renderer_preference_watcher.mojom.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"
#include "third_party/blink/public/platform/web_feature.mojom.h"
#include "third_party/blink/public/web/worker_content_settings_proxy.mojom.h"

namespace content {
namespace {

void AllowFileSystemOnIOThreadResponse(base::OnceCallback<void(bool)> callback,
                                       bool result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(std::move(callback), result));
}

void AllowFileSystemOnIOThread(const GURL& url,
                               ResourceContext* resource_context,
                               std::vector<GlobalFrameRoutingId> render_frames,
                               base::OnceCallback<void(bool)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  GetContentClient()->browser()->AllowWorkerFileSystem(
      url, resource_context, render_frames,
      base::Bind(&AllowFileSystemOnIOThreadResponse, base::Passed(&callback)));
}

bool AllowIndexedDBOnIOThread(const GURL& url,
                              const base::string16& name,
                              ResourceContext* resource_context,
                              std::vector<GlobalFrameRoutingId> render_frames) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return GetContentClient()->browser()->AllowWorkerIndexedDB(
      url, name, resource_context, render_frames);
}

}  // namespace

// RAII helper class for talking to SharedWorkerDevToolsManager.
class SharedWorkerHost::ScopedDevToolsHandle {
 public:
  ScopedDevToolsHandle(SharedWorkerHost* owner,
                       bool* out_pause_on_start,
                       base::UnguessableToken* out_devtools_worker_token)
      : owner_(owner) {
    SharedWorkerDevToolsManager::GetInstance()->WorkerCreated(
        owner, out_pause_on_start, out_devtools_worker_token);
  }

  ~ScopedDevToolsHandle() {
    SharedWorkerDevToolsManager::GetInstance()->WorkerDestroyed(owner_);
  }

  void WorkerReadyForInspection() {
    SharedWorkerDevToolsManager::GetInstance()->WorkerReadyForInspection(
        owner_);
  }

 private:
  SharedWorkerHost* owner_;
  DISALLOW_COPY_AND_ASSIGN(ScopedDevToolsHandle);
};

SharedWorkerHost::SharedWorkerHost(
    SharedWorkerServiceImpl* service,
    std::unique_ptr<SharedWorkerInstance> instance,
    int process_id)
    : binding_(this),
      service_(service),
      instance_(std::move(instance)),
      process_id_(process_id),
      next_connection_request_id_(1),
      creation_time_(base::TimeTicks::Now()),
      interface_provider_binding_(this),
      weak_factory_(this) {
  DCHECK(instance_);
  // Set up the worker interface request. This is needed first in either
  // AddClient() or Start(). AddClient() can sometimes be called before Start()
  // when two clients call new SharedWorker() at around the same time.
  worker_request_ = mojo::MakeRequest(&worker_);

  // Keep the renderer process alive that will be hosting the shared worker.
  RenderProcessHost* process_host = RenderProcessHost::FromID(process_id);
  DCHECK(!IsShuttingDown(process_host));
  process_host->IncrementKeepAliveRefCount(
      RenderProcessHost::KeepAliveClientType::kSharedWorker);
}

SharedWorkerHost::~SharedWorkerHost() {
  UMA_HISTOGRAM_LONG_TIMES("SharedWorker.TimeToDeleted",
                           base::TimeTicks::Now() - creation_time_);
  switch (phase_) {
    case Phase::kInitial:
      // Tell clients that this worker failed to start. This is only needed in
      // kInitial. Once in kStarted, the worker in the renderer would alert this
      // host if script loading failed.
      for (const ClientInfo& info : clients_)
        info.client->OnScriptLoadFailed();
      break;
    case Phase::kStarted:
    case Phase::kClosed:
    case Phase::kTerminationSent:
    case Phase::kTerminationSentAndClosed:
      break;
  }

  RenderProcessHost* process_host = RenderProcessHost::FromID(process_id_);
  if (!IsShuttingDown(process_host)) {
    process_host->DecrementKeepAliveRefCount(
        RenderProcessHost::KeepAliveClientType::kSharedWorker);
  }
}

void SharedWorkerHost::Start(
    mojom::SharedWorkerFactoryPtr factory,
    mojom::ServiceWorkerProviderInfoForSharedWorkerPtr
        service_worker_provider_info,
    network::mojom::URLLoaderFactoryAssociatedPtrInfo
        main_script_loader_factory,
    blink::mojom::SharedWorkerMainScriptLoadParamsPtr main_script_load_params,
    std::unique_ptr<URLLoaderFactoryBundleInfo> subresource_loader_factories,
    base::Optional<SubresourceLoaderParams> subresource_loader_params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  AdvanceTo(Phase::kStarted);

#if DCHECK_IS_ON()
  // Verify the combination of the given args based on the flags. See the
  // function comment for details.
  if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    // NetworkService (PlzWorker):
    DCHECK(service_worker_provider_info);
    DCHECK(!main_script_loader_factory);
    DCHECK(main_script_load_params);
    DCHECK(subresource_loader_factories);
    DCHECK(!subresource_loader_factories->default_factory_info());
  } else if (base::FeatureList::IsEnabled(
                 blink::features::kServiceWorkerServicification)) {
    // S13nServiceWorker (non-NetworkService):
    DCHECK(service_worker_provider_info);
    DCHECK(main_script_loader_factory);
    DCHECK(subresource_loader_factories);
    DCHECK(subresource_loader_factories->default_factory_info());
    DCHECK(!subresource_loader_params);
    DCHECK(!main_script_load_params);
  } else {
    // Legacy case (to be deprecated):
    DCHECK(!service_worker_provider_info);
    DCHECK(!main_script_loader_factory);
    DCHECK(!subresource_loader_factories);
    DCHECK(!subresource_loader_params);
    DCHECK(!main_script_load_params);
  }
#endif  // DCHECK_IS_ON()

  mojom::SharedWorkerInfoPtr info(mojom::SharedWorkerInfo::New(
      instance_->url(), instance_->name(), instance_->content_security_policy(),
      instance_->content_security_policy_type(),
      instance_->creation_address_space()));

  // Register with DevTools.
  bool pause_on_start;
  base::UnguessableToken devtools_worker_token;
  devtools_handle_ = std::make_unique<ScopedDevToolsHandle>(
      this, &pause_on_start, &devtools_worker_token);

  RendererPreferences renderer_preferences;
  GetContentClient()->browser()->UpdateRendererPreferencesForWorker(
      RenderProcessHost::FromID(process_id_)->GetBrowserContext(),
      &renderer_preferences);

  // Create a RendererPreferenceWatcher to observe updates in the preferences.
  mojom::RendererPreferenceWatcherPtr watcher_ptr;
  mojom::RendererPreferenceWatcherRequest preference_watcher_request =
      mojo::MakeRequest(&watcher_ptr);
  GetContentClient()->browser()->RegisterRendererPreferenceWatcherForWorkers(
      RenderProcessHost::FromID(process_id_)->GetBrowserContext(),
      std::move(watcher_ptr));

  // Set up content settings interface.
  blink::mojom::WorkerContentSettingsProxyPtr content_settings;
  content_settings_ = std::make_unique<SharedWorkerContentSettingsProxyImpl>(
      instance_->url(), this, mojo::MakeRequest(&content_settings));

  // Set up host interface.
  mojom::SharedWorkerHostPtr host;
  binding_.Bind(mojo::MakeRequest(&host));

  // Set up interface provider interface.
  service_manager::mojom::InterfaceProviderPtr interface_provider;
  interface_provider_binding_.Bind(FilterRendererExposedInterfaces(
      mojom::kNavigation_SharedWorkerSpec, process_id_,
      mojo::MakeRequest(&interface_provider)));

  // Set the default factory to the bundle for subresource loading to pass to
  // the renderer when NetworkService is on. When S13nServiceWorker is on, the
  // default factory is already provided by SharedWorkerServiceImpl.
  if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    // If the caller has supplied a default URLLoaderFactory override (for,
    // e.g., AppCache), use that.
    network::mojom::URLLoaderFactoryPtrInfo default_factory_info;
    if (subresource_loader_params &&
        subresource_loader_params->loader_factory_info.is_valid()) {
      default_factory_info =
          std::move(subresource_loader_params->loader_factory_info);
    } else {
      CreateNetworkFactory(mojo::MakeRequest(&default_factory_info));
    }
    subresource_loader_factories->default_factory_info() =
        std::move(default_factory_info);
  }

  // NetworkService (PlzWorker):
  // Prepare the controller service worker info to pass to the renderer. This is
  // only provided if NetworkService is enabled. In the non-NetworkService case,
  // the controller is sent in SetController IPCs during the request for the
  // shared worker script.
  mojom::ControllerServiceWorkerInfoPtr controller;
  blink::mojom::ServiceWorkerObjectAssociatedPtrInfo remote_object;
  blink::mojom::ServiceWorkerState sent_state;
  if (subresource_loader_params &&
      subresource_loader_params->controller_service_worker_info) {
    DCHECK(base::FeatureList::IsEnabled(network::features::kNetworkService));
    controller =
        std::move(subresource_loader_params->controller_service_worker_info);
    // |object_info| can be nullptr when the service worker context or the
    // service worker version is gone during shared worker startup.
    if (controller->object_info) {
      controller->object_info->request = mojo::MakeRequest(&remote_object);
      sent_state = controller->object_info->state;
    }
  }

  // Send the CreateSharedWorker message.
  factory_ = std::move(factory);
  factory_->CreateSharedWorker(
      std::move(info), pause_on_start, devtools_worker_token,
      renderer_preferences, std::move(preference_watcher_request),
      std::move(content_settings), std::move(service_worker_provider_info),
      appcache_handle_ ? appcache_handle_->appcache_host_id()
                       : kAppCacheNoHostId,
      std::move(main_script_loader_factory), std::move(main_script_load_params),
      std::move(subresource_loader_factories), std::move(controller),
      std::move(host), std::move(worker_request_),
      std::move(interface_provider));

  // NetworkService (PlzWorker):
  // |remote_object| is an associated interface ptr, so calls can't be made on
  // it until its request endpoint is sent. Now that the request endpoint was
  // sent, it can be used, so add it to ServiceWorkerObjectHost.
  if (remote_object.is_valid()) {
    DCHECK(base::FeatureList::IsEnabled(network::features::kNetworkService));
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(
            &ServiceWorkerObjectHost::AddRemoteObjectPtrAndUpdateState,
            subresource_loader_params->controller_service_worker_object_host,
            std::move(remote_object), sent_state));
  }

  // Monitor the lifetime of the worker.
  worker_.set_connection_error_handler(base::BindOnce(
      &SharedWorkerHost::OnWorkerConnectionLost, weak_factory_.GetWeakPtr()));
}

//  This is similar to
//  RenderFrameHostImpl::CreateNetworkServiceDefaultFactoryAndObserve, but this
//  host doesn't observe network service crashes. Instead, the renderer detects
//  the connection error and terminates the worker.
void SharedWorkerHost::CreateNetworkFactory(
    network::mojom::URLLoaderFactoryRequest request) {
  network::mojom::URLLoaderFactoryParamsPtr params =
      network::mojom::URLLoaderFactoryParams::New();
  params->process_id = process_id_;
  // TODO(lukasza): https://crbug.com/792546: Start using CORB.
  params->is_corb_enabled = false;

  service_->storage_partition()->GetNetworkContext()->CreateURLLoaderFactory(
      std::move(request), std::move(params));
}

void SharedWorkerHost::AllowFileSystem(
    const GURL& url,
    base::OnceCallback<void(bool)> callback) {
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&AllowFileSystemOnIOThread, url,
                     RenderProcessHost::FromID(process_id_)
                         ->GetBrowserContext()
                         ->GetResourceContext(),
                     GetRenderFrameIDsForWorker(), std::move(callback)));
}

void SharedWorkerHost::AllowIndexedDB(const GURL& url,
                                      const base::string16& name,
                                      base::OnceCallback<void(bool)> callback) {
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&AllowIndexedDBOnIOThread, url, name,
                     RenderProcessHost::FromID(process_id_)
                         ->GetBrowserContext()
                         ->GetResourceContext(),
                     GetRenderFrameIDsForWorker()),
      std::move(callback));
}

void SharedWorkerHost::TerminateWorker() {
  switch (phase_) {
    case Phase::kInitial:
      // The host is being asked to terminate the worker before it started.
      // Tell clients that this worker failed to start.
      for (const ClientInfo& info : clients_)
        info.client->OnScriptLoadFailed();
      // Tell the caller it terminated, so the caller doesn't wait forever.
      AdvanceTo(Phase::kTerminationSentAndClosed);
      OnWorkerConnectionLost();
      // |this| is destroyed here.
      return;
    case Phase::kStarted:
      AdvanceTo(Phase::kTerminationSent);
      break;
    case Phase::kClosed:
      AdvanceTo(Phase::kTerminationSentAndClosed);
      break;
    case Phase::kTerminationSent:
    case Phase::kTerminationSentAndClosed:
      // Termination was already sent. TerminateWorker can be called twice in
      // tests while cleaning up all the workers.
      return;
  }

  devtools_handle_.reset();
  worker_->Terminate();
  // Now, we wait to observe OnWorkerConnectionLost.
}

void SharedWorkerHost::AdvanceTo(Phase phase) {
  switch (phase_) {
    case Phase::kInitial:
      DCHECK(phase == Phase::kStarted ||
             phase == Phase::kTerminationSentAndClosed);
      break;
    case Phase::kStarted:
      DCHECK(phase == Phase::kClosed || phase == Phase::kTerminationSent);
      break;
    case Phase::kClosed:
      DCHECK(phase == Phase::kTerminationSentAndClosed);
      break;
    case Phase::kTerminationSent:
      DCHECK(phase == Phase::kTerminationSentAndClosed);
      break;
    case Phase::kTerminationSentAndClosed:
      NOTREACHED();
      break;
  }
  phase_ = phase;
}

SharedWorkerHost::ClientInfo::ClientInfo(mojom::SharedWorkerClientPtr client,
                                         int connection_request_id,
                                         int process_id,
                                         int frame_id)
    : client(std::move(client)),
      connection_request_id(connection_request_id),
      process_id(process_id),
      frame_id(frame_id) {}

SharedWorkerHost::ClientInfo::~ClientInfo() {}

void SharedWorkerHost::OnConnected(int connection_request_id) {
  if (!instance_)
    return;
  for (const ClientInfo& info : clients_) {
    if (info.connection_request_id != connection_request_id)
      continue;
    info.client->OnConnected(std::vector<blink::mojom::WebFeature>(
        used_features_.begin(), used_features_.end()));
    return;
  }
}

void SharedWorkerHost::OnContextClosed() {
  devtools_handle_.reset();

  // Mark as closed - this will stop any further messages from being sent to the
  // worker (messages can still be sent from the worker, for exception
  // reporting, etc).
  switch (phase_) {
    case Phase::kInitial:
      // Not possible: there is no Mojo connection on which OnContextClosed can
      // be called.
      NOTREACHED();
      break;
    case Phase::kStarted:
      AdvanceTo(Phase::kClosed);
      break;
    case Phase::kTerminationSent:
      AdvanceTo(Phase::kTerminationSentAndClosed);
      break;
    case Phase::kClosed:
    case Phase::kTerminationSentAndClosed:
      // Already closed, just ignore.
      break;
  }
}

void SharedWorkerHost::OnReadyForInspection() {
  if (devtools_handle_)
    devtools_handle_->WorkerReadyForInspection();
}

void SharedWorkerHost::OnScriptLoaded() {
  UMA_HISTOGRAM_TIMES("SharedWorker.TimeToScriptLoaded",
                      base::TimeTicks::Now() - creation_time_);
}

void SharedWorkerHost::OnScriptLoadFailed() {
  UMA_HISTOGRAM_TIMES("SharedWorker.TimeToScriptLoadFailed",
                      base::TimeTicks::Now() - creation_time_);
  for (const ClientInfo& info : clients_)
    info.client->OnScriptLoadFailed();
}

void SharedWorkerHost::OnFeatureUsed(blink::mojom::WebFeature feature) {
  // Avoid reporting a feature more than once, and enable any new clients to
  // observe features that were historically used.
  if (!used_features_.insert(feature).second)
    return;
  for (const ClientInfo& info : clients_)
    info.client->OnFeatureUsed(feature);
}

std::vector<GlobalFrameRoutingId>
SharedWorkerHost::GetRenderFrameIDsForWorker() {
  std::vector<GlobalFrameRoutingId> result;
  result.reserve(clients_.size());
  for (const ClientInfo& info : clients_)
    result.push_back(GlobalFrameRoutingId(info.process_id, info.frame_id));
  return result;
}

bool SharedWorkerHost::IsAvailable() const {
  switch (phase_) {
    case Phase::kInitial:
    case Phase::kStarted:
      return true;
    case Phase::kClosed:
    case Phase::kTerminationSent:
    case Phase::kTerminationSentAndClosed:
      return false;
  }
  NOTREACHED();
  return false;
}

base::WeakPtr<SharedWorkerHost> SharedWorkerHost::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void SharedWorkerHost::AddClient(mojom::SharedWorkerClientPtr client,
                                 int process_id,
                                 int frame_id,
                                 const blink::MessagePortChannel& port) {
  // Pass the actual creation context type, so the client can understand if
  // there is a mismatch between security levels.
  client->OnCreated(instance_->creation_context_type());

  clients_.emplace_back(std::move(client), next_connection_request_id_++,
                        process_id, frame_id);
  ClientInfo& info = clients_.back();

  // Observe when the client goes away.
  info.client.set_connection_error_handler(base::BindOnce(
      &SharedWorkerHost::OnClientConnectionLost, weak_factory_.GetWeakPtr()));

  worker_->Connect(info.connection_request_id, port.ReleaseHandle());
}

void SharedWorkerHost::BindDevToolsAgent(
    blink::mojom::DevToolsAgentHostAssociatedPtrInfo host,
    blink::mojom::DevToolsAgentAssociatedRequest request) {
  worker_->BindDevToolsAgent(std::move(host), std::move(request));
}

void SharedWorkerHost::SetAppCacheHandle(
    std::unique_ptr<AppCacheNavigationHandle> appcache_handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  appcache_handle_ = std::move(appcache_handle);
}

void SharedWorkerHost::OnClientConnectionLost() {
  // We'll get a notification for each dropped connection.
  for (auto it = clients_.begin(); it != clients_.end(); ++it) {
    if (it->client.encountered_error()) {
      clients_.erase(it);
      break;
    }
  }
  // If there are no clients left, then it's cleanup time.
  if (clients_.empty())
    TerminateWorker();
}

void SharedWorkerHost::OnWorkerConnectionLost() {
  // This will destroy |this| resulting in client's observing their mojo
  // connection being dropped.
  service_->DestroyHost(this);
}

void SharedWorkerHost::GetInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto* process = RenderProcessHost::FromID(process_id_);
  if (!process)
    return;

  BindWorkerInterface(interface_name, std::move(interface_pipe), process,
                      url::Origin::Create(instance()->url()));
}

}  // namespace content
