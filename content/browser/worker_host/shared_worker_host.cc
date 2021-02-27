// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/shared_worker_host.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/task/post_task.h"
#include "base/unguessable_token.h"
#include "content/browser/appcache/appcache_navigation_handle.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/devtools/shared_worker_devtools_manager.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/service_worker/service_worker_main_resource_handle.h"
#include "content/browser/service_worker/service_worker_object_host.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/url_loader_factory_params_helper.h"
#include "content/browser/webtransport/quic_transport_connector_impl.h"
#include "content/browser/worker_host/shared_worker_content_settings_proxy_impl.h"
#include "content/browser/worker_host/shared_worker_service_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/worker_type.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "net/base/isolation_info.h"
#include "net/cookies/site_for_cookies.h"
#include "services/metrics/public/cpp/delegating_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/loader/url_loader_factory_bundle.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "third_party/blink/public/mojom/renderer_preference_watcher.mojom.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom.h"
#include "third_party/blink/public/mojom/worker/shared_worker_info.mojom.h"
#include "third_party/blink/public/mojom/worker/worker_content_settings_proxy.mojom.h"

namespace content {

// RAII helper class for talking to SharedWorkerDevToolsManager.
class SharedWorkerHost::ScopedDevToolsHandle {
 public:
  explicit ScopedDevToolsHandle(SharedWorkerHost* owner) : owner_(owner) {
    SharedWorkerDevToolsManager::GetInstance()->WorkerCreated(
        owner, &pause_on_start_, &dev_tools_token_);
  }

  ~ScopedDevToolsHandle() {
    SharedWorkerDevToolsManager::GetInstance()->WorkerDestroyed(owner_);
  }

  void WorkerReadyForInspection(
      mojo::PendingRemote<blink::mojom::DevToolsAgent> agent_remote,
      mojo::PendingReceiver<blink::mojom::DevToolsAgentHost>
          agent_host_receiver) {
    SharedWorkerDevToolsManager::GetInstance()->WorkerReadyForInspection(
        owner_, std::move(agent_remote), std::move(agent_host_receiver));
  }

  bool pause_on_start() const { return pause_on_start_; }

  const base::UnguessableToken& dev_tools_token() const {
    return dev_tools_token_;
  }

 private:
  SharedWorkerHost* owner_;

  // Indicates if the worker should be paused when it is started. This is set
  // when a dev tools agent host already exists for that shared worker, which
  // happens when a shared worker is restarted while it is being debugged.
  bool pause_on_start_;

  base::UnguessableToken dev_tools_token_;

  DISALLOW_COPY_AND_ASSIGN(ScopedDevToolsHandle);
};

class SharedWorkerHost::ScopedProcessHostRef {
 public:
  explicit ScopedProcessHostRef(RenderProcessHost* render_process_host)
      : render_process_host_(render_process_host) {
    render_process_host_->IncrementKeepAliveRefCount();
  }

  ~ScopedProcessHostRef() {
    if (!render_process_host_->IsKeepAliveRefCountDisabled())
      render_process_host_->DecrementKeepAliveRefCount();
  }

  ScopedProcessHostRef(const ScopedProcessHostRef& other) = delete;

 private:
  RenderProcessHost* const render_process_host_;
};

SharedWorkerHost::SharedWorkerHost(
    SharedWorkerServiceImpl* service,
    const SharedWorkerInstance& instance,
    scoped_refptr<SiteInstanceImpl> site_instance,
    std::vector<network::mojom::ContentSecurityPolicyPtr>
        content_security_policies)
    : service_(service),
      token_(blink::SharedWorkerToken()),
      instance_(instance),
      content_security_policies_(std::move(content_security_policies)),
      site_instance_(std::move(site_instance)),
      scoped_process_host_ref_(
          std::make_unique<ScopedProcessHostRef>(site_instance_->GetProcess())),
      next_connection_request_id_(1),
      devtools_handle_(std::make_unique<ScopedDevToolsHandle>(this)),
      ukm_source_id_(ukm::ConvertToSourceId(ukm::AssignNewSourceId(),
                                            ukm::SourceIdType::WORKER_ID)) {
  DCHECK(GetProcessHost());
  DCHECK(GetProcessHost()->IsInitializedAndNotDead());

  site_instance_->AddObserver(this);

  // Set up the worker pending receiver. This is needed first in either
  // AddClient() or Start(). AddClient() can sometimes be called before Start()
  // when two clients call new SharedWorker() at around the same time.
  worker_receiver_ = worker_.BindNewPipeAndPassReceiver();

  service_->NotifyWorkerCreated(token_, GetProcessHost()->GetID(),
                                devtools_handle_->dev_tools_token());
}

SharedWorkerHost::~SharedWorkerHost() {
  site_instance_->RemoveObserver(this);

  if (started_) {
    // Attempt to notify the worker before disconnecting.
    if (worker_) {
      worker_->Terminate();
    }
  } else {
    // Tell clients that this worker failed to start.
    for (const ClientInfo& info : clients_)
      info.client->OnScriptLoadFailed(/*error_message=*/"");
  }

  // Notify the service that each client still connected will be removed and
  // that the worker will terminate.
  for (const auto& client : clients_) {
    service_->NotifyClientRemoved(token_, client.render_frame_host_id);
  }
  service_->NotifyBeforeWorkerDestroyed(token_);
}

RenderProcessHost* SharedWorkerHost::GetProcessHost() {
  DCHECK(site_instance_->HasProcess());
  return site_instance_->GetProcess();
}

void SharedWorkerHost::Start(
    mojo::PendingRemote<blink::mojom::SharedWorkerFactory> factory,
    blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
        subresource_loader_factories,
    blink::mojom::ControllerServiceWorkerInfoPtr controller,
    base::WeakPtr<ServiceWorkerObjectHost>
        controller_service_worker_object_host,
    blink::mojom::FetchClientSettingsObjectPtr
        outside_fetch_client_settings_object,
    const GURL& final_response_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!started_);
  DCHECK(main_script_load_params);
  DCHECK(subresource_loader_factories);
  DCHECK(!subresource_loader_factories->pending_default_factory());

  started_ = true;
  final_response_url_ = final_response_url;

  auto options = blink::mojom::WorkerOptions::New(
      instance_.script_type(), instance_.credentials_mode(), instance_.name());
  blink::mojom::SharedWorkerInfoPtr info(blink::mojom::SharedWorkerInfo::New(
      instance_.url(), std::move(options),
      mojo::Clone(content_security_policies_),
      instance_.creation_address_space(),
      std::move(outside_fetch_client_settings_object)));

  auto renderer_preferences = blink::RendererPreferences();
  GetContentClient()->browser()->UpdateRendererPreferencesForWorker(
      GetProcessHost()->GetBrowserContext(), &renderer_preferences);

  // Create a RendererPreferenceWatcher to observe updates in the preferences.
  mojo::PendingRemote<blink::mojom::RendererPreferenceWatcher> watcher_remote;
  mojo::PendingReceiver<blink::mojom::RendererPreferenceWatcher>
      preference_watcher_receiver =
          watcher_remote.InitWithNewPipeAndPassReceiver();
  GetContentClient()->browser()->RegisterRendererPreferenceWatcher(
      GetProcessHost()->GetBrowserContext(), std::move(watcher_remote));

  // Set up content settings interface.
  mojo::PendingRemote<blink::mojom::WorkerContentSettingsProxy>
      content_settings;
  content_settings_ = std::make_unique<SharedWorkerContentSettingsProxyImpl>(
      instance_.url(), this, content_settings.InitWithNewPipeAndPassReceiver());

  // Set up BrowserInterfaceBroker interface
  mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
      browser_interface_broker;
  broker_receiver_.Bind(
      browser_interface_broker.InitWithNewPipeAndPassReceiver());

  // Set the default factory to the bundle for subresource loading to pass to
  // the renderer.
  bool bypass_redirect_checks = false;
  subresource_loader_factories->pending_default_factory() =
      CreateNetworkFactoryForSubresources(&bypass_redirect_checks);
  subresource_loader_factories->set_bypass_redirect_checks(
      bypass_redirect_checks);

  // Prepare the controller service worker info to pass to the renderer.
  // |object_info| can be nullptr when the service worker context or the service
  // worker version is gone during shared worker startup.
  mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerObject>
      service_worker_remote_object;
  blink::mojom::ServiceWorkerState service_worker_sent_state;
  if (controller && controller->object_info) {
    controller->object_info->receiver =
        service_worker_remote_object.InitWithNewEndpointAndPassReceiver();
    service_worker_sent_state = controller->object_info->state;
  }

  // Send the CreateSharedWorker message.
  factory_.Bind(std::move(factory));
  factory_->CreateSharedWorker(
      std::move(info), token_, instance_.constructor_origin(),
      GetContentClient()->browser()->GetUserAgent(),
      GetContentClient()->browser()->GetUserAgentMetadata(),
      devtools_handle_->pause_on_start(), devtools_handle_->dev_tools_token(),
      std::move(renderer_preferences), std::move(preference_watcher_receiver),
      std::move(content_settings), service_worker_handle_->TakeContainerInfo(),
      appcache_handle_
          ? base::make_optional(appcache_handle_->appcache_host_id())
          : base::nullopt,
      std::move(main_script_load_params),
      std::move(subresource_loader_factories), std::move(controller),
      receiver_.BindNewPipeAndPassRemote(), std::move(worker_receiver_),
      std::move(browser_interface_broker), ukm_source_id_);

  // |service_worker_remote_object| is an associated interface ptr, so calls
  // can't be made on it until its request endpoint is sent. Now that the
  // request endpoint was sent, it can be used, so add it to
  // ServiceWorkerObjectHost.
  if (service_worker_remote_object.is_valid()) {
    RunOrPostTaskOnThread(
        FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
        base::BindOnce(
            &ServiceWorkerObjectHost::AddRemoteObjectPtrAndUpdateState,
            controller_service_worker_object_host,
            std::move(service_worker_remote_object),
            service_worker_sent_state));
  }

  // Monitor the lifetime of the worker.
  worker_.set_disconnect_handler(base::BindOnce(
      &SharedWorkerHost::OnWorkerConnectionLost, weak_factory_.GetWeakPtr()));
}

//  This is similar to
//  RenderFrameHostImpl::CreateNetworkServiceDefaultFactoryAndObserve, but this
//  host doesn't observe network service crashes. Instead, the renderer detects
//  the connection error and terminates the worker.
mojo::PendingRemote<network::mojom::URLLoaderFactory>
SharedWorkerHost::CreateNetworkFactoryForSubresources(
    bool* bypass_redirect_checks) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(bypass_redirect_checks);

  mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_default_factory;
  mojo::PendingReceiver<network::mojom::URLLoaderFactory>
      default_factory_receiver =
          pending_default_factory.InitWithNewPipeAndPassReceiver();

  // TODO(https://crbug.com/1060832): Implement COEP reporter for shared
  // workers.
  network::mojom::URLLoaderFactoryParamsPtr factory_params =
      CreateNetworkFactoryParamsForSubresources();
  url::Origin origin = url::Origin::Create(instance_.url());
  GetContentClient()->browser()->WillCreateURLLoaderFactory(
      GetProcessHost()->GetBrowserContext(),
      /*frame=*/nullptr, GetProcessHost()->GetID(),
      ContentBrowserClient::URLLoaderFactoryType::kWorkerSubResource, origin,
      /*navigation_id=*/base::nullopt,
      ukm::SourceIdObj::FromInt64(ukm_source_id_), &default_factory_receiver,
      &factory_params->header_client, bypass_redirect_checks,
      /*disable_secure_dns=*/nullptr, &factory_params->factory_override);

  devtools_instrumentation::WillCreateURLLoaderFactoryForSharedWorker(
      this, &factory_params->factory_override);

  // TODO(yhirano): Support COEP.
  GetProcessHost()->CreateURLLoaderFactory(std::move(default_factory_receiver),
                                           std::move(factory_params));

  return pending_default_factory;
}

network::mojom::URLLoaderFactoryParamsPtr
SharedWorkerHost::CreateNetworkFactoryParamsForSubresources() {
  url::Origin origin = url::Origin::Create(instance_.url());

  // TODO(https://crbug.com/1060832): Implement COEP reporter for shared
  // workers.
  network::mojom::URLLoaderFactoryParamsPtr factory_params =
      URLLoaderFactoryParamsHelper::CreateForWorker(
          GetProcessHost(), instance_.constructor_origin(),
          net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                     origin, origin,
                                     net::SiteForCookies::FromOrigin(origin)),
          /*coep_reporter=*/mojo::NullRemote(),
          /*auth_cert_observer=*/mojo::NullRemote(),
          /*debug_tag=*/"SharedWorkerHost::CreateNetworkFactoryForSubresource");
  return factory_params;
}

void SharedWorkerHost::AllowFileSystem(
    const GURL& url,
    base::OnceCallback<void(bool)> callback) {
  GetContentClient()->browser()->AllowWorkerFileSystem(
      url, GetProcessHost()->GetBrowserContext(), GetRenderFrameIDsForWorker(),
      std::move(callback));
}

void SharedWorkerHost::AllowIndexedDB(const GURL& url,
                                      base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(GetContentClient()->browser()->AllowWorkerIndexedDB(
      url, GetProcessHost()->GetBrowserContext(),
      GetRenderFrameIDsForWorker()));
}

void SharedWorkerHost::AllowCacheStorage(
    const GURL& url,
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(
      GetContentClient()->browser()->AllowWorkerCacheStorage(
          url, GetProcessHost()->GetBrowserContext(),
          GetRenderFrameIDsForWorker()));
}

void SharedWorkerHost::AllowWebLocks(const GURL& url,
                                     base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(GetContentClient()->browser()->AllowWorkerWebLocks(
      url, GetProcessHost()->GetBrowserContext(),
      GetRenderFrameIDsForWorker()));
}

void SharedWorkerHost::CreateAppCacheBackend(
    mojo::PendingReceiver<blink::mojom::AppCacheBackend> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* storage_partition_impl = static_cast<StoragePartitionImpl*>(
      GetProcessHost()->GetStoragePartition());
  if (!storage_partition_impl)
    return;
  auto* appcache_service = storage_partition_impl->GetAppCacheService();
  if (!appcache_service)
    return;
  appcache_service->CreateBackend(GetProcessHost()->GetID(), MSG_ROUTING_NONE,
                                  std::move(receiver));
}

void SharedWorkerHost::CreateQuicTransportConnector(
    mojo::PendingReceiver<blink::mojom::QuicTransportConnector> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const url::Origin origin = url::Origin::Create(instance().url());
  mojo::MakeSelfOwnedReceiver(std::make_unique<QuicTransportConnectorImpl>(
                                  GetProcessHost()->GetID(), /*frame=*/nullptr,
                                  origin, GetNetworkIsolationKey()),
                              std::move(receiver));
}

void SharedWorkerHost::BindCacheStorage(
    mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(https://crbug.com/1031542): Add support enforcing CORP in
  // cache.match() for SharedWorker by providing the correct value here.
  network::CrossOriginEmbedderPolicy cross_origin_embedder_policy;

  // TODO(https://crbug.com/1031542): Plumb a CrossOriginEmbedderPolicyReporter
  // here to handle reports.
  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter;

  const url::Origin origin = url::Origin::Create(instance().url());
  GetProcessHost()->BindCacheStorage(cross_origin_embedder_policy,
                                     std::move(coep_reporter), origin,
                                     std::move(receiver));
}

void SharedWorkerHost::Destruct() {
  // Ask the service to destroy |this| which will terminate the worker.
  service_->DestroyHost(this);
}

SharedWorkerHost::ClientInfo::ClientInfo(
    mojo::Remote<blink::mojom::SharedWorkerClient> client,
    int connection_request_id,
    GlobalFrameRoutingId render_frame_host_id)
    : client(std::move(client)),
      connection_request_id(connection_request_id),
      render_frame_host_id(render_frame_host_id) {}

SharedWorkerHost::ClientInfo::~ClientInfo() {}

void SharedWorkerHost::OnConnected(int connection_request_id) {
  for (const ClientInfo& info : clients_) {
    if (info.connection_request_id != connection_request_id)
      continue;
    info.client->OnConnected(std::vector<blink::mojom::WebFeature>(
        used_features_.begin(), used_features_.end()));
    return;
  }
}

void SharedWorkerHost::OnContextClosed() {
  // Not possible: there is no Mojo connection on which OnContextClosed can
  // be called.
  DCHECK(started_);

  Destruct();
}

void SharedWorkerHost::OnReadyForInspection(
    mojo::PendingRemote<blink::mojom::DevToolsAgent> agent_remote,
    mojo::PendingReceiver<blink::mojom::DevToolsAgentHost>
        agent_host_receiver) {
  devtools_handle_->WorkerReadyForInspection(std::move(agent_remote),
                                             std::move(agent_host_receiver));
}

void SharedWorkerHost::OnScriptLoadFailed(const std::string& error_message) {
  for (const ClientInfo& info : clients_)
    info.client->OnScriptLoadFailed(error_message);
}

void SharedWorkerHost::OnFeatureUsed(blink::mojom::WebFeature feature) {
  // Avoid reporting a feature more than once, and enable any new clients to
  // observe features that were historically used.
  if (!used_features_.insert(feature).second)
    return;
  for (const ClientInfo& info : clients_)
    info.client->OnFeatureUsed(feature);
}

void SharedWorkerHost::RenderProcessHostDestroyed() {
  Destruct();
}

std::vector<GlobalFrameRoutingId>
SharedWorkerHost::GetRenderFrameIDsForWorker() {
  std::vector<GlobalFrameRoutingId> result;
  result.reserve(clients_.size());
  for (const ClientInfo& info : clients_)
    result.push_back(info.render_frame_host_id);
  return result;
}

base::WeakPtr<SharedWorkerHost> SharedWorkerHost::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

net::NetworkIsolationKey SharedWorkerHost::GetNetworkIsolationKey() const {
  const url::Origin origin = url::Origin::Create(instance().url());
  // TODO(https://crbug.com/1147281): This is the NetworkIsolationKey of a
  // top-level browsing context, which shouldn't be use for SharedWorkers used
  // in iframes.
  return net::NetworkIsolationKey::ToDoUseTopFrameOriginAsWell(origin);
}

void SharedWorkerHost::ReportNoBinderForInterface(const std::string& error) {
  broker_receiver_.ReportBadMessage(error + " for the shared worker scope");
}

void SharedWorkerHost::AddClient(
    mojo::PendingRemote<blink::mojom::SharedWorkerClient> client,
    GlobalFrameRoutingId client_render_frame_host_id,
    const blink::MessagePortChannel& port,
    ukm::SourceId client_ukm_source_id) {
  mojo::Remote<blink::mojom::SharedWorkerClient> remote_client(
      std::move(client));

  // Pass the actual creation context type, so the client can understand if
  // there is a mismatch between security levels.
  remote_client->OnCreated(instance_.creation_context_type());

  clients_.emplace_back(std::move(remote_client), next_connection_request_id_++,
                        client_render_frame_host_id);
  ClientInfo& info = clients_.back();

  // Observe when the client goes away.
  info.client.set_disconnect_handler(base::BindOnce(
      &SharedWorkerHost::OnClientConnectionLost, weak_factory_.GetWeakPtr()));

  ukm::DelegatingUkmRecorder* ukm_recorder = ukm::DelegatingUkmRecorder::Get();
  if (ukm_recorder) {
    ukm::builders::Worker_ClientAdded(ukm_source_id_)
        .SetClientSourceId(client_ukm_source_id)
        .SetWorkerType(static_cast<int64_t>(WorkerType::kSharedWorker))
        .Record(ukm_recorder);
  }

  worker_->Connect(info.connection_request_id, port.ReleaseHandle());

  // Notify that a new client was added now.
  service_->NotifyClientAdded(token_, client_render_frame_host_id);
}

void SharedWorkerHost::SetAppCacheHandle(
    std::unique_ptr<AppCacheNavigationHandle> appcache_handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  appcache_handle_ = std::move(appcache_handle);
}

void SharedWorkerHost::SetServiceWorkerHandle(
    std::unique_ptr<ServiceWorkerMainResourceHandle> service_worker_handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  service_worker_handle_ = std::move(service_worker_handle);
}

void SharedWorkerHost::PruneNonExistentClients() {
  DCHECK(!started_);

  auto it = clients_.begin();
  auto end = clients_.end();
  while (it != end) {
    if (!RenderFrameHostImpl::FromID(it->render_frame_host_id)) {
      service_->NotifyClientRemoved(token_, it->render_frame_host_id);
      it = clients_.erase(it);
    } else {
      ++it;
    }
  }
}

bool SharedWorkerHost::HasClients() const {
  return !clients_.empty();
}

const base::UnguessableToken& SharedWorkerHost::GetDevToolsToken() const {
  return devtools_handle_->dev_tools_token();
}

mojo::Remote<blink::mojom::SharedWorker>
SharedWorkerHost::TerminateRemoteWorkerForTesting() {
  mojo::Remote<blink::mojom::SharedWorker> worker = std::move(worker_);

  // Tell the remote worker to terminate.
  if (worker && worker.is_connected()) {
    worker.reset_on_disconnect();
    worker->Terminate();
  }

  return worker;
}

void SharedWorkerHost::OnClientConnectionLost() {
  // We'll get a notification for each dropped connection.
  for (auto it = clients_.begin(); it != clients_.end(); ++it) {
    if (!it->client.is_connected()) {
      // Notify the service that the client is gone.
      service_->NotifyClientRemoved(token_, it->render_frame_host_id);
      clients_.erase(it);
      break;
    }
  }
  // If there are no clients left, then it's cleanup time.
  if (clients_.empty())
    Destruct();
}

void SharedWorkerHost::OnWorkerConnectionLost() {
  // This will destroy |this| resulting in client's observing their mojo
  // connection being dropped.
  Destruct();
}

}  // namespace content
