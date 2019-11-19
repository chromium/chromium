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
#include "content/browser/devtools/shared_worker_devtools_manager.h"
#include "content/browser/interface_provider_filtering.h"
#include "content/browser/renderer_interface_binders.h"
#include "content/browser/service_worker/service_worker_navigation_handle.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/webtransport/quic_transport_connector_impl.h"
#include "content/browser/worker_host/shared_worker_content_settings_proxy_impl.h"
#include "content/browser/worker_host/shared_worker_service_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "net/base/network_isolation_key.h"
#include "third_party/blink/public/common/loader/url_loader_factory_bundle.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "third_party/blink/public/mojom/renderer_preference_watcher.mojom.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom.h"
#include "third_party/blink/public/mojom/worker/shared_worker_info.mojom.h"
#include "third_party/blink/public/mojom/worker/worker_content_settings_proxy.mojom.h"

namespace content {
namespace {

SharedWorkerHost::CreateNetworkFactoryCallback&
GetCreateNetworkFactoryCallbackForSharedWorker() {
  static base::NoDestructor<SharedWorkerHost::CreateNetworkFactoryCallback>
      s_callback;
  return *s_callback;
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

  void WorkerReadyForInspection(
      mojo::PendingRemote<blink::mojom::DevToolsAgent> agent_remote,
      mojo::PendingReceiver<blink::mojom::DevToolsAgentHost>
          agent_host_receiver) {
    SharedWorkerDevToolsManager::GetInstance()->WorkerReadyForInspection(
        owner_, std::move(agent_remote), std::move(agent_host_receiver));
  }

 private:
  SharedWorkerHost* owner_;
  DISALLOW_COPY_AND_ASSIGN(ScopedDevToolsHandle);
};

SharedWorkerHost::SharedWorkerHost(SharedWorkerServiceImpl* service,
                                   const SharedWorkerInstance& instance,
                                   int worker_process_id)
    : service_(service),
      instance_(instance),
      worker_process_id_(worker_process_id),
      next_connection_request_id_(1),
      interface_provider_binding_(this) {
  // Set up the worker pending receiver. This is needed first in either
  // AddClient() or Start(). AddClient() can sometimes be called before Start()
  // when two clients call new SharedWorker() at around the same time.
  worker_receiver_ = worker_.BindNewPipeAndPassReceiver();

  // Keep the renderer process alive that will be hosting the shared worker.
  auto* worker_process_host = RenderProcessHost::FromID(worker_process_id_);
  DCHECK(!IsShuttingDown(worker_process_host));
  worker_process_host->IncrementKeepAliveRefCount();
}

SharedWorkerHost::~SharedWorkerHost() {
  if (started_) {
    // Attempt to notify the worker before disconnecting.
    if (worker_)
      worker_->Terminate();

    // Notify the service that each client still connected will be removed and
    // that the worker will terminate.
    for (const auto& client : clients_) {
      service_->NotifyClientRemoved(instance_, client.client_process_id,
                                    client.frame_id);
    }
    service_->NotifyWorkerTerminating(instance_);
  } else {
    // Tell clients that this worker failed to start.
    for (const ClientInfo& info : clients_)
      info.client->OnScriptLoadFailed();
  }

  auto* worker_process_host = RenderProcessHost::FromID(worker_process_id_);
  if (!IsShuttingDown(worker_process_host))
    worker_process_host->DecrementKeepAliveRefCount();
}

// static
void SharedWorkerHost::SetNetworkFactoryForSubresourcesForTesting(
    const CreateNetworkFactoryCallback& create_network_factory_callback) {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(create_network_factory_callback.is_null() ||
         GetCreateNetworkFactoryCallbackForSharedWorker().is_null())
      << "It is not expected that this is called with non-null callback when "
      << "another overriding callback is already set.";
  GetCreateNetworkFactoryCallbackForSharedWorker() =
      create_network_factory_callback;
}

void SharedWorkerHost::Start(
    mojo::PendingRemote<blink::mojom::SharedWorkerFactory> factory,
    blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
    std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
        subresource_loader_factories,
    blink::mojom::ControllerServiceWorkerInfoPtr controller,
    base::WeakPtr<ServiceWorkerObjectHost>
        controller_service_worker_object_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!started_);
  DCHECK(main_script_load_params);
  DCHECK(subresource_loader_factories);
  DCHECK(!subresource_loader_factories->pending_default_factory());

  started_ = true;

  blink::mojom::SharedWorkerInfoPtr info(blink::mojom::SharedWorkerInfo::New(
      instance_.url(), instance_.name(), instance_.content_security_policy(),
      instance_.content_security_policy_type(),
      instance_.creation_address_space()));

  // Register with DevTools.
  bool pause_on_start;
  base::UnguessableToken devtools_worker_token;
  devtools_handle_ = std::make_unique<ScopedDevToolsHandle>(
      this, &pause_on_start, &devtools_worker_token);

  auto renderer_preferences = blink::mojom::RendererPreferences::New();
  GetContentClient()->browser()->UpdateRendererPreferencesForWorker(
      RenderProcessHost::FromID(worker_process_id_)->GetBrowserContext(),
      renderer_preferences.get());

  // Create a RendererPreferenceWatcher to observe updates in the preferences.
  mojo::PendingRemote<blink::mojom::RendererPreferenceWatcher> watcher_remote;
  mojo::PendingReceiver<blink::mojom::RendererPreferenceWatcher>
      preference_watcher_receiver =
          watcher_remote.InitWithNewPipeAndPassReceiver();
  GetContentClient()->browser()->RegisterRendererPreferenceWatcher(
      RenderProcessHost::FromID(worker_process_id_)->GetBrowserContext(),
      std::move(watcher_remote));

  // Set up content settings interface.
  mojo::PendingRemote<blink::mojom::WorkerContentSettingsProxy>
      content_settings;
  content_settings_ = std::make_unique<SharedWorkerContentSettingsProxyImpl>(
      instance_.url(), this, content_settings.InitWithNewPipeAndPassReceiver());

  // Set up interface provider interface.
  service_manager::mojom::InterfaceProviderPtr interface_provider;
  interface_provider_binding_.Bind(FilterRendererExposedInterfaces(
      blink::mojom::kNavigation_SharedWorkerSpec, worker_process_id_,
      mojo::MakeRequest(&interface_provider)));

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
      std::move(info), GetContentClient()->browser()->GetUserAgent(),
      pause_on_start, devtools_worker_token, std::move(renderer_preferences),
      std::move(preference_watcher_receiver), std::move(content_settings),
      service_worker_handle_->TakeProviderInfo(),
      appcache_handle_
          ? base::make_optional(appcache_handle_->appcache_host_id())
          : base::nullopt,
      std::move(main_script_load_params),
      std::move(subresource_loader_factories), std::move(controller),
      receiver_.BindNewPipeAndPassRemote(), std::move(worker_receiver_),
      std::move(interface_provider), std::move(browser_interface_broker));

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

  // Notify the service that the worker was started and that some clients were
  // already connected.
  service_->NotifyWorkerStarted(instance_, worker_process_id_,
                                devtools_worker_token);
  for (const auto& client : clients_) {
    service_->NotifyClientAdded(instance_, client.client_process_id,
                                client.frame_id);
  }
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

  auto* worker_process_host = RenderProcessHost::FromID(worker_process_id_);
  auto* storage_partition_impl = static_cast<StoragePartitionImpl*>(
      worker_process_host->GetStoragePartition());
  url::Origin origin = instance_.constructor_origin();

  mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_default_factory;
  mojo::PendingReceiver<network::mojom::URLLoaderFactory>
      default_factory_receiver =
          pending_default_factory.InitWithNewPipeAndPassReceiver();

  mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
      default_header_client;
  GetContentClient()->browser()->WillCreateURLLoaderFactory(
      storage_partition_impl->browser_context(),
      /*frame=*/nullptr, worker_process_id_,
      ContentBrowserClient::URLLoaderFactoryType::kWorkerSubResource, origin,
      &default_factory_receiver, &default_header_client,
      bypass_redirect_checks);

  // TODO(nhiroki): Call devtools_instrumentation::WillCreateURLLoaderFactory()
  // here.

  // TODO(yhirano): Support COEP.
  if (GetCreateNetworkFactoryCallbackForSharedWorker().is_null()) {
    worker_process_host->CreateURLLoaderFactory(
        origin, origin, network::mojom::CrossOriginEmbedderPolicy::kNone,
        nullptr /* preferences */, net::NetworkIsolationKey(origin, origin),
        std::move(default_header_client), std::move(default_factory_receiver));
  } else {
    mojo::PendingRemote<network::mojom::URLLoaderFactory> original_factory;
    worker_process_host->CreateURLLoaderFactory(
        origin, origin, network::mojom::CrossOriginEmbedderPolicy::kNone,
        nullptr /* preferences */, net::NetworkIsolationKey(origin, origin),
        std::move(default_header_client),
        original_factory.InitWithNewPipeAndPassReceiver());
    GetCreateNetworkFactoryCallbackForSharedWorker().Run(
        std::move(default_factory_receiver), worker_process_id_,
        std::move(original_factory));
  }

  return pending_default_factory;
}

void SharedWorkerHost::AllowFileSystem(
    const GURL& url,
    base::OnceCallback<void(bool)> callback) {
  GetContentClient()->browser()->AllowWorkerFileSystem(
      url, RenderProcessHost::FromID(worker_process_id_)->GetBrowserContext(),
      GetRenderFrameIDsForWorker(), std::move(callback));
}

void SharedWorkerHost::AllowIndexedDB(const GURL& url,
                                      base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(GetContentClient()->browser()->AllowWorkerIndexedDB(
      url, RenderProcessHost::FromID(worker_process_id_)->GetBrowserContext(),
      GetRenderFrameIDsForWorker()));
}

void SharedWorkerHost::AllowCacheStorage(
    const GURL& url,
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(
      GetContentClient()->browser()->AllowWorkerCacheStorage(
          url,
          RenderProcessHost::FromID(worker_process_id_)->GetBrowserContext(),
          GetRenderFrameIDsForWorker()));
}

void SharedWorkerHost::AllowWebLocks(const GURL& url,
                                     base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(GetContentClient()->browser()->AllowWorkerWebLocks(
      url, RenderProcessHost::FromID(worker_process_id_)->GetBrowserContext(),
      GetRenderFrameIDsForWorker()));
}

void SharedWorkerHost::CreateAppCacheBackend(
    mojo::PendingReceiver<blink::mojom::AppCacheBackend> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderProcessHost* worker_process_host = GetProcessHost();
  if (!worker_process_host)
    return;
  auto* storage_partition_impl = static_cast<StoragePartitionImpl*>(
      worker_process_host->GetStoragePartition());
  if (!storage_partition_impl)
    return;
  storage_partition_impl->GetAppCacheService()->CreateBackend(
      worker_process_host->GetID(), MSG_ROUTING_NONE, std::move(receiver));
}

void SharedWorkerHost::CreateIDBFactory(
    mojo::PendingReceiver<blink::mojom::IDBFactory> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderProcessHost* worker_process_host = GetProcessHost();
  if (!worker_process_host)
    return;
  worker_process_host->BindIndexedDB(MSG_ROUTING_NONE,
                                     url::Origin::Create(instance().url()),
                                     std::move(receiver));
}

void SharedWorkerHost::CreateQuicTransportConnector(
    mojo::PendingReceiver<blink::mojom::QuicTransportConnector> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderProcessHost* worker_process_host = GetProcessHost();
  if (!worker_process_host)
    return;

  const url::Origin origin = url::Origin::Create(instance().url());
  mojo::MakeSelfOwnedReceiver(std::make_unique<QuicTransportConnectorImpl>(
                                  worker_process_host->GetID(), origin,
                                  net::NetworkIsolationKey(origin, origin)),
                              std::move(receiver));
}

void SharedWorkerHost::Destruct() {
  // Ask the service to destroy |this| which will terminate the worker.
  service_->DestroyHost(this);
}

SharedWorkerHost::ClientInfo::ClientInfo(
    mojo::Remote<blink::mojom::SharedWorkerClient> client,
    int connection_request_id,
    int client_process_id,
    int frame_id)
    : client(std::move(client)),
      connection_request_id(connection_request_id),
      client_process_id(client_process_id),
      frame_id(frame_id) {}

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
  if (devtools_handle_) {
    devtools_handle_->WorkerReadyForInspection(std::move(agent_remote),
                                               std::move(agent_host_receiver));
  }
}

void SharedWorkerHost::OnScriptLoadFailed() {
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
  for (const ClientInfo& info : clients_) {
    result.push_back(
        GlobalFrameRoutingId(info.client_process_id, info.frame_id));
  }
  return result;
}

base::WeakPtr<SharedWorkerHost> SharedWorkerHost::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void SharedWorkerHost::AddClient(
    mojo::PendingRemote<blink::mojom::SharedWorkerClient> client,
    int client_process_id,
    int frame_id,
    const blink::MessagePortChannel& port) {
  mojo::Remote<blink::mojom::SharedWorkerClient> remote_client(
      std::move(client));

  // Pass the actual creation context type, so the client can understand if
  // there is a mismatch between security levels.
  remote_client->OnCreated(instance_.creation_context_type());

  clients_.emplace_back(std::move(remote_client), next_connection_request_id_++,
                        client_process_id, frame_id);
  ClientInfo& info = clients_.back();

  // Observe when the client goes away.
  info.client.set_disconnect_handler(base::BindOnce(
      &SharedWorkerHost::OnClientConnectionLost, weak_factory_.GetWeakPtr()));

  worker_->Connect(info.connection_request_id, port.ReleaseHandle());

  // Notify that a new client was added now. If the worker is not started, the
  // Start() function will handle sending a notification for each existing
  // client.
  if (started_)
    service_->NotifyClientAdded(instance_, client_process_id, frame_id);
}

void SharedWorkerHost::SetAppCacheHandle(
    std::unique_ptr<AppCacheNavigationHandle> appcache_handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  appcache_handle_ = std::move(appcache_handle);
}

void SharedWorkerHost::SetServiceWorkerHandle(
    std::unique_ptr<ServiceWorkerNavigationHandle> service_worker_handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  service_worker_handle_ = std::move(service_worker_handle);
}

bool SharedWorkerHost::HasClients() const {
  return !clients_.empty();
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
      // Notify the service that a client was removed while the worker was
      // running.
      if (started_) {
        service_->NotifyClientRemoved(instance_, it->client_process_id,
                                      it->frame_id);
      }
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

void SharedWorkerHost::GetInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto* worker_process_host = RenderProcessHost::FromID(worker_process_id_);
  if (!worker_process_host)
    return;

  BindWorkerInterface(interface_name, std::move(interface_pipe),
                      worker_process_host,
                      url::Origin::Create(instance_.url()));
}

}  // namespace content
