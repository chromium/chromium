// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/loader/web_worker_fetch_context_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "content/child/child_thread_impl.h"
#include "content/common/content_constants_internal.h"
#include "content/common/frame_messages.h"
#include "content/public/common/content_client.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/url_loader_throttle_provider.h"
#include "content/public/renderer/websocket_handshake_throttle_provider.h"
#include "content/renderer/loader/child_url_loader_factory_bundle.h"
#include "content/renderer/loader/request_extra_data.h"
#include "content/renderer/loader/resource_dispatcher.h"
#include "content/renderer/loader/web_url_loader_impl.h"
#include "content/renderer/loader/web_url_request_util.h"
#include "content/renderer/service_worker/controller_service_worker_connector.h"
#include "content/renderer/service_worker/service_worker_provider_context.h"
#include "content/renderer/service_worker/service_worker_subresource_loader.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"
#include "third_party/blink/public/platform/web_code_cache_loader.h"
#include "third_party/blink/public/platform/web_frame_request_blocker.h"
#include "third_party/blink/public/platform/web_security_origin.h"

namespace content {

WebWorkerFetchContextImpl::RewriteURLFunction g_rewrite_url = nullptr;

namespace {

// Runs on a background thread created in ResetServiceWorkerURLLoaderFactory().
void CreateServiceWorkerSubresourceLoaderFactory(
    mojo::PendingRemote<blink::mojom::ServiceWorkerContainerHost>
        remote_container_host,
    const std::string& client_id,
    std::unique_ptr<network::PendingSharedURLLoaderFactory> fallback_factory,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_refptr<base::SequencedTaskRunner> worker_timing_callback_task_runner,
    base::RepeatingCallback<
        void(int, mojo::PendingReceiver<blink::mojom::WorkerTimingContainer>)>
        worker_timing_callback) {
  ServiceWorkerSubresourceLoaderFactory::Create(
      base::MakeRefCounted<ControllerServiceWorkerConnector>(
          std::move(remote_container_host),
          mojo::NullRemote() /* remote_controller */, client_id),
      network::SharedURLLoaderFactory::Create(std::move(fallback_factory)),
      std::move(receiver), std::move(task_runner),
      std::move(worker_timing_callback_task_runner),
      std::move(worker_timing_callback));
}

}  // namespace

// static
void WebWorkerFetchContextImpl::InstallRewriteURLFunction(
    RewriteURLFunction rewrite_url) {
  CHECK(!g_rewrite_url);
  g_rewrite_url = rewrite_url;
}

// An implementation of WebURLLoaderFactory that is aware of service workers. In
// the usual case, it creates a loader that uses |loader_factory_|. But if the
// worker fetch context is controlled by a service worker, it creates a loader
// that uses |service_worker_loader_factory_| for requests that should be
// intercepted by the service worker.
class WebWorkerFetchContextImpl::Factory : public blink::WebURLLoaderFactory {
 public:
  Factory(base::WeakPtr<ResourceDispatcher> resource_dispatcher,
          scoped_refptr<network::SharedURLLoaderFactory> loader_factory)
      : resource_dispatcher_(std::move(resource_dispatcher)),
        loader_factory_(std::move(loader_factory)) {}
  ~Factory() override = default;

  std::unique_ptr<blink::WebURLLoader> CreateURLLoader(
      const blink::WebURLRequest& request,
      std::unique_ptr<blink::scheduler::WebResourceLoadingTaskRunnerHandle>
          task_runner_handle) override {
    DCHECK(task_runner_handle);
    DCHECK(resource_dispatcher_);

    // KeepAlive is not yet supported in web workers.
    mojo::PendingRemote<mojom::KeepAliveHandle> keep_alive_handle =
        mojo::NullRemote();

    if (CanCreateServiceWorkerURLLoader(request)) {
      // Create our own URLLoader to route the request to the controller service
      // worker.
      return std::make_unique<WebURLLoaderImpl>(
          resource_dispatcher_.get(), std::move(task_runner_handle),
          service_worker_loader_factory_, std::move(keep_alive_handle));
    }

    return std::make_unique<WebURLLoaderImpl>(
        resource_dispatcher_.get(), std::move(task_runner_handle),
        loader_factory_, std::move(keep_alive_handle));
  }

  void SetServiceWorkerURLLoaderFactory(
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          service_worker_loader_factory) {
    if (!service_worker_loader_factory) {
      service_worker_loader_factory_ = nullptr;
      return;
    }
    service_worker_loader_factory_ =
        base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
            std::move(service_worker_loader_factory));
  }

  base::WeakPtr<Factory> GetWeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

 private:
  bool CanCreateServiceWorkerURLLoader(const blink::WebURLRequest& request) {
    // TODO(horo): Unify this code path with
    // ServiceWorkerNetworkProviderForFrame::CreateURLLoader that is used
    // for document cases.

    // We need the service worker loader factory populated in order to create
    // our own URLLoader for subresource loading via a service worker.
    if (!service_worker_loader_factory_)
      return false;

    // If the URL is not http(s) or otherwise whitelisted, do not intercept the
    // request. Schemes like 'blob' and 'file' are not eligible to be
    // intercepted by service workers.
    // TODO(falken): Let ServiceWorkerSubresourceLoaderFactory handle the
    // request and move this check there (i.e., for such URLs, it should use
    // its fallback factory).
    if (!GURL(request.Url()).SchemeIsHTTPOrHTTPS() &&
        !OriginCanAccessServiceWorkers(request.Url())) {
      return false;
    }

    // If GetSkipServiceWorker() returns true, no need to intercept the request.
    if (request.GetSkipServiceWorker())
      return false;

    return true;
  }

  base::WeakPtr<ResourceDispatcher> resource_dispatcher_;
  scoped_refptr<network::SharedURLLoaderFactory> loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> service_worker_loader_factory_;
  base::WeakPtrFactory<Factory> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(Factory);
};

scoped_refptr<WebWorkerFetchContextImpl> WebWorkerFetchContextImpl::Create(
    ServiceWorkerProviderContext* provider_context,
    blink::mojom::RendererPreferences renderer_preferences,
    mojo::PendingReceiver<blink::mojom::RendererPreferenceWatcher>
        watcher_receiver,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_loader_factory,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_fallback_factory,
    mojo::PendingReceiver<blink::mojom::SubresourceLoaderUpdater>
        pending_subresource_loader_updater,
    const std::vector<std::string>& cors_exempt_header_list,
    mojo::PendingRemote<blink::mojom::ResourceLoadInfoNotifier>
        pending_resource_load_info_notifier) {
  mojo::PendingReceiver<blink::mojom::ServiceWorkerWorkerClient>
      service_worker_client_receiver;
  mojo::PendingRemote<blink::mojom::ServiceWorkerWorkerClientRegistry>
      service_worker_worker_client_registry;
  mojo::PendingRemote<blink::mojom::ServiceWorkerContainerHost>
      service_worker_container_host;

  // Some sandboxed iframes are not allowed to use service worker so don't have
  // a real service worker provider, so the provider context is null.
  if (provider_context) {
    provider_context->CloneWorkerClientRegistry(
        service_worker_worker_client_registry.InitWithNewPipeAndPassReceiver());

    mojo::PendingRemote<blink::mojom::ServiceWorkerWorkerClient> worker_client;
    service_worker_client_receiver =
        worker_client.InitWithNewPipeAndPassReceiver();
    provider_context->RegisterWorkerClient(std::move(worker_client));

    service_worker_container_host =
        provider_context->CloneRemoteContainerHost();
  }

  scoped_refptr<WebWorkerFetchContextImpl> worker_fetch_context =
      base::AdoptRef(new WebWorkerFetchContextImpl(
          std::move(renderer_preferences), std::move(watcher_receiver),
          std::move(service_worker_client_receiver),
          std::move(service_worker_worker_client_registry),
          std::move(service_worker_container_host),
          std::move(pending_loader_factory),
          std::move(pending_fallback_factory),
          std::move(pending_subresource_loader_updater),
          GetContentClient()->renderer()->CreateURLLoaderThrottleProvider(
              URLLoaderThrottleProviderType::kWorker),
          GetContentClient()
              ->renderer()
              ->CreateWebSocketHandshakeThrottleProvider(),
          cors_exempt_header_list,
          std::move(pending_resource_load_info_notifier)));
  if (provider_context) {
    worker_fetch_context->set_controller_service_worker_mode(
        provider_context->GetControllerServiceWorkerMode());
    worker_fetch_context->set_client_id(provider_context->client_id());
  } else {
    worker_fetch_context->set_controller_service_worker_mode(
        blink::mojom::ControllerServiceWorkerMode::kNoController);
  }
  return worker_fetch_context;
}

WebWorkerFetchContextImpl::WebWorkerFetchContextImpl(
    blink::mojom::RendererPreferences renderer_preferences,
    mojo::PendingReceiver<blink::mojom::RendererPreferenceWatcher>
        preference_watcher_receiver,
    mojo::PendingReceiver<blink::mojom::ServiceWorkerWorkerClient>
        service_worker_client_receiver,
    mojo::PendingRemote<blink::mojom::ServiceWorkerWorkerClientRegistry>
        pending_service_worker_worker_client_registry,
    mojo::PendingRemote<blink::mojom::ServiceWorkerContainerHost>
        service_worker_container_host,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_loader_factory,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_fallback_factory,
    mojo::PendingReceiver<blink::mojom::SubresourceLoaderUpdater>
        pending_subresource_loader_updater,
    std::unique_ptr<URLLoaderThrottleProvider> throttle_provider,
    std::unique_ptr<WebSocketHandshakeThrottleProvider>
        websocket_handshake_throttle_provider,
    const std::vector<std::string>& cors_exempt_header_list,
    mojo::PendingRemote<blink::mojom::ResourceLoadInfoNotifier>
        pending_resource_load_info_notifier)
    : service_worker_client_receiver_(
          std::move(service_worker_client_receiver)),
      pending_service_worker_worker_client_registry_(
          std::move(pending_service_worker_worker_client_registry)),
      pending_service_worker_container_host_(
          std::move(service_worker_container_host)),
      pending_loader_factory_(std::move(pending_loader_factory)),
      pending_fallback_factory_(std::move(pending_fallback_factory)),
      pending_subresource_loader_updater_(
          std::move(pending_subresource_loader_updater)),
      renderer_preferences_(std::move(renderer_preferences)),
      preference_watcher_pending_receiver_(
          std::move(preference_watcher_receiver)),
      throttle_provider_(std::move(throttle_provider)),
      websocket_handshake_throttle_provider_(
          std::move(websocket_handshake_throttle_provider)),
      cors_exempt_header_list_(cors_exempt_header_list),
      pending_resource_load_info_notifier_(
          std::move(pending_resource_load_info_notifier)) {}

WebWorkerFetchContextImpl::~WebWorkerFetchContextImpl() = default;

void WebWorkerFetchContextImpl::SetTerminateSyncLoadEvent(
    base::WaitableEvent* terminate_sync_load_event) {
  DCHECK(!terminate_sync_load_event_);
  terminate_sync_load_event_ = terminate_sync_load_event;
}

scoped_refptr<WebWorkerFetchContextImpl>
WebWorkerFetchContextImpl::CloneForNestedWorkerDeprecated(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(!base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker));

  mojo::PendingReceiver<blink::mojom::ServiceWorkerWorkerClient>
      service_worker_client_receiver;
  mojo::PendingRemote<blink::mojom::ServiceWorkerWorkerClientRegistry>
      service_worker_worker_client_registry;
  if (service_worker_worker_client_registry_) {
    mojo::PendingRemote<blink::mojom::ServiceWorkerWorkerClient>
        service_worker_client;
    service_worker_client_receiver =
        service_worker_client.InitWithNewPipeAndPassReceiver();
    service_worker_worker_client_registry_->RegisterWorkerClient(
        std::move(service_worker_client));
    service_worker_worker_client_registry_->CloneWorkerClientRegistry(
        service_worker_worker_client_registry.InitWithNewPipeAndPassReceiver());
  }

  mojo::PendingRemote<blink::mojom::ServiceWorkerContainerHost>
      service_worker_container_host;
  if (service_worker_container_host_) {
    service_worker_container_host_->CloneContainerHost(
        service_worker_container_host.InitWithNewPipeAndPassReceiver());
  }

  // |pending_subresource_loader_updater| is not used for
  // non-PlzDedicatedWorker.
  scoped_refptr<WebWorkerFetchContextImpl> new_context =
      CloneForNestedWorkerInternal(
          std::move(service_worker_client_receiver),
          std::move(service_worker_worker_client_registry),
          std::move(service_worker_container_host), loader_factory_->Clone(),
          fallback_factory_->Clone(),
          /*pending_subresource_loader_updater=*/mojo::NullReceiver(),
          std::move(task_runner));
  new_context->controller_service_worker_mode_ =
      controller_service_worker_mode_;

  return new_context;
}

scoped_refptr<WebWorkerFetchContextImpl>
WebWorkerFetchContextImpl::CloneForNestedWorker(
    ServiceWorkerProviderContext* service_worker_provider_context,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_loader_factory,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_fallback_factory,
    mojo::PendingReceiver<blink::mojom::SubresourceLoaderUpdater>
        pending_subresource_loader_updater,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker));
  DCHECK(pending_loader_factory);
  DCHECK(pending_fallback_factory);
  DCHECK(task_runner);

  if (!service_worker_provider_context) {
    return CloneForNestedWorkerInternal(
        /*service_worker_client_receiver=*/mojo::NullReceiver(),
        /*service_worker_worker_client_registry=*/mojo::NullRemote(),
        /*container_host=*/mojo::NullRemote(),
        std::move(pending_loader_factory), std::move(pending_fallback_factory),
        std::move(pending_subresource_loader_updater), std::move(task_runner));
  }

  mojo::PendingRemote<blink::mojom::ServiceWorkerWorkerClientRegistry>
      service_worker_worker_client_registry;
  service_worker_provider_context->CloneWorkerClientRegistry(
      service_worker_worker_client_registry.InitWithNewPipeAndPassReceiver());

  mojo::PendingRemote<blink::mojom::ServiceWorkerWorkerClient> worker_client;
  mojo::PendingReceiver<blink::mojom::ServiceWorkerWorkerClient>
      service_worker_client_receiver =
          worker_client.InitWithNewPipeAndPassReceiver();
  service_worker_provider_context->RegisterWorkerClient(
      std::move(worker_client));

  mojo::PendingRemote<blink::mojom::ServiceWorkerContainerHost>
      service_worker_container_host =
          service_worker_provider_context->CloneRemoteContainerHost();

  scoped_refptr<WebWorkerFetchContextImpl> new_context =
      CloneForNestedWorkerInternal(
          std::move(service_worker_client_receiver),
          std::move(service_worker_worker_client_registry),
          std::move(service_worker_container_host),
          std::move(pending_loader_factory),
          std::move(pending_fallback_factory),
          std::move(pending_subresource_loader_updater),
          std::move(task_runner));
  new_context->controller_service_worker_mode_ =
      service_worker_provider_context->GetControllerServiceWorkerMode();

  return new_context;
}

void WebWorkerFetchContextImpl::InitializeOnWorkerThread(
    blink::AcceptLanguagesWatcher* watcher) {
  DCHECK(!resource_dispatcher_);
  DCHECK(!receiver_.is_bound());
  DCHECK(!preference_watcher_receiver_.is_bound());
  resource_dispatcher_ = std::make_unique<ResourceDispatcher>();
  resource_dispatcher_->SetCorsExemptHeaderList(cors_exempt_header_list_);
  resource_dispatcher_->set_terminate_sync_load_event(
      terminate_sync_load_event_);

  loader_factory_ = network::SharedURLLoaderFactory::Create(
      std::move(pending_loader_factory_));
  fallback_factory_ = network::SharedURLLoaderFactory::Create(
      std::move(pending_fallback_factory_));
  subresource_loader_updater_.Bind(
      std::move(pending_subresource_loader_updater_));

  if (service_worker_client_receiver_.is_valid())
    receiver_.Bind(std::move(service_worker_client_receiver_));

  if (pending_service_worker_worker_client_registry_) {
    service_worker_worker_client_registry_.Bind(
        std::move(pending_service_worker_worker_client_registry_));
  }

  if (preference_watcher_pending_receiver_.is_valid()) {
    preference_watcher_receiver_.Bind(
        std::move(preference_watcher_pending_receiver_));
  }

  if (pending_service_worker_container_host_) {
    service_worker_container_host_.Bind(
        std::move(pending_service_worker_container_host_));
  }

  if (pending_resource_load_info_notifier_) {
    resource_load_info_notifier_.Bind(
        std::move(pending_resource_load_info_notifier_));
  }

  accept_languages_watcher_ = watcher;

  DCHECK(loader_factory_);
  DCHECK(!web_loader_factory_);
  web_loader_factory_ = std::make_unique<Factory>(
      resource_dispatcher_->GetWeakPtr(), loader_factory_);

  ResetServiceWorkerURLLoaderFactory();
}

blink::WebURLLoaderFactory* WebWorkerFetchContextImpl::GetURLLoaderFactory() {
  return web_loader_factory_.get();
}

std::unique_ptr<blink::WebURLLoaderFactory>
WebWorkerFetchContextImpl::WrapURLLoaderFactory(
    blink::CrossVariantMojoRemote<network::mojom::URLLoaderFactoryInterfaceBase>
        url_loader_factory) {
  return std::make_unique<WebURLLoaderFactoryImpl>(
      resource_dispatcher_->GetWeakPtr(),
      base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
          std::move(url_loader_factory)));
}

std::unique_ptr<blink::WebCodeCacheLoader>
WebWorkerFetchContextImpl::CreateCodeCacheLoader() {
  return blink::WebCodeCacheLoader::Create(terminate_sync_load_event_);
}

void WebWorkerFetchContextImpl::WillSendRequest(blink::WebURLRequest& request) {
  if (renderer_preferences_.enable_do_not_track) {
    request.SetHttpHeaderField(blink::WebString::FromUTF8(kDoNotTrackHeader),
                               "1");
  }

  auto extra_data = base::MakeRefCounted<RequestExtraData>();
  extra_data->set_render_frame_id(ancestor_frame_id_);
  extra_data->set_frame_request_blocker(frame_request_blocker_);
  if (throttle_provider_) {
    extra_data->set_url_loader_throttles(
        throttle_provider_->CreateThrottles(ancestor_frame_id_, request));
  }
  request.SetExtraData(std::move(extra_data));

  if (g_rewrite_url)
    request.SetUrl(g_rewrite_url(request.Url().GetString().Utf8(), false));

  if (!renderer_preferences_.enable_referrers) {
    request.SetReferrerString(blink::WebString());
    request.SetReferrerPolicy(network::mojom::ReferrerPolicy::kNever);
  }
}

blink::mojom::ControllerServiceWorkerMode
WebWorkerFetchContextImpl::GetControllerServiceWorkerMode() const {
  return controller_service_worker_mode_;
}

void WebWorkerFetchContextImpl::SetIsOnSubframe(bool is_on_sub_frame) {
  is_on_sub_frame_ = is_on_sub_frame;
}

bool WebWorkerFetchContextImpl::IsOnSubframe() const {
  return is_on_sub_frame_;
}

net::SiteForCookies WebWorkerFetchContextImpl::SiteForCookies() const {
  return site_for_cookies_;
}

base::Optional<blink::WebSecurityOrigin>
WebWorkerFetchContextImpl::TopFrameOrigin() const {
  // TODO(jkarlin): set_top_frame_origin is only called for dedicated workers.
  // Determine the top-frame-origin of a shared worker as well. See
  // https://crbug.com/918868.
  return top_frame_origin_;
}

void WebWorkerFetchContextImpl::SetSubresourceFilterBuilder(
    std::unique_ptr<blink::WebDocumentSubresourceFilter::Builder>
        subresource_filter_builder) {
  subresource_filter_builder_ = std::move(subresource_filter_builder);
}

std::unique_ptr<blink::WebDocumentSubresourceFilter>
WebWorkerFetchContextImpl::TakeSubresourceFilter() {
  if (!subresource_filter_builder_)
    return nullptr;
  return std::move(subresource_filter_builder_)->Build();
}

std::unique_ptr<blink::WebSocketHandshakeThrottle>
WebWorkerFetchContextImpl::CreateWebSocketHandshakeThrottle(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  if (!websocket_handshake_throttle_provider_)
    return nullptr;
  return websocket_handshake_throttle_provider_->CreateThrottle(
      ancestor_frame_id_, std::move(task_runner));
}

blink::CrossVariantMojoReceiver<
    blink::mojom::WorkerTimingContainerInterfaceBase>
WebWorkerFetchContextImpl::TakePendingWorkerTimingReceiver(int request_id) {
  auto iter = worker_timing_container_receivers_.find(request_id);
  if (iter == worker_timing_container_receivers_.end()) {
    return {};
  }
  auto receiver = std::move(iter->second);
  worker_timing_container_receivers_.erase(iter);
  return std::move(receiver);
}

void WebWorkerFetchContextImpl::SetIsOfflineMode(bool is_offline_mode) {
  // Worker doesn't support offline mode. There should be no callers.
  NOTREACHED();
}

void WebWorkerFetchContextImpl::set_controller_service_worker_mode(
    blink::mojom::ControllerServiceWorkerMode mode) {
  controller_service_worker_mode_ = mode;
}

void WebWorkerFetchContextImpl::set_ancestor_frame_id(int id) {
  ancestor_frame_id_ = id;
}

void WebWorkerFetchContextImpl::set_frame_request_blocker(
    scoped_refptr<blink::WebFrameRequestBlocker> frame_request_blocker) {
  frame_request_blocker_ = frame_request_blocker;
}

void WebWorkerFetchContextImpl::set_site_for_cookies(
    const net::SiteForCookies& site_for_cookies) {
  site_for_cookies_ = site_for_cookies;
}

void WebWorkerFetchContextImpl::set_top_frame_origin(
    const blink::WebSecurityOrigin& top_frame_origin) {
  top_frame_origin_ = top_frame_origin;
}

void WebWorkerFetchContextImpl::set_client_id(const std::string& client_id) {
  client_id_ = client_id;
}

void WebWorkerFetchContextImpl::OnControllerChanged(
    blink::mojom::ControllerServiceWorkerMode mode) {
  set_controller_service_worker_mode(mode);
  ResetServiceWorkerURLLoaderFactory();
}

scoped_refptr<WebWorkerFetchContextImpl>
WebWorkerFetchContextImpl::CloneForNestedWorkerInternal(
    mojo::PendingReceiver<blink::mojom::ServiceWorkerWorkerClient>
        service_worker_client_receiver,
    mojo::PendingRemote<blink::mojom::ServiceWorkerWorkerClientRegistry>
        service_worker_worker_client_registry,
    mojo::PendingRemote<blink::mojom::ServiceWorkerContainerHost>
        service_worker_container_host,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_loader_factory,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_fallback_factory,
    mojo::PendingReceiver<blink::mojom::SubresourceLoaderUpdater>
        pending_subresource_loader_updater,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  mojo::PendingRemote<blink::mojom::ResourceLoadInfoNotifier>
      pending_resource_load_info_notifier;
  if (resource_load_info_notifier_) {
    resource_load_info_notifier_->Clone(
        pending_resource_load_info_notifier.InitWithNewPipeAndPassReceiver());
  }

  mojo::PendingRemote<blink::mojom::RendererPreferenceWatcher>
      preference_watcher;
  auto new_context = base::AdoptRef(new WebWorkerFetchContextImpl(
      renderer_preferences_,
      preference_watcher.InitWithNewPipeAndPassReceiver(),
      std::move(service_worker_client_receiver),
      std::move(service_worker_worker_client_registry),
      std::move(service_worker_container_host),
      std::move(pending_loader_factory), std::move(pending_fallback_factory),
      std::move(pending_subresource_loader_updater),
      throttle_provider_ ? throttle_provider_->Clone() : nullptr,
      websocket_handshake_throttle_provider_
          ? websocket_handshake_throttle_provider_->Clone(
                std::move(task_runner))
          : nullptr,
      cors_exempt_header_list_,
      std::move(pending_resource_load_info_notifier)));
  new_context->is_on_sub_frame_ = is_on_sub_frame_;
  new_context->ancestor_frame_id_ = ancestor_frame_id_;
  new_context->frame_request_blocker_ = frame_request_blocker_;
  new_context->site_for_cookies_ = site_for_cookies_;
  new_context->top_frame_origin_ = top_frame_origin_;
  child_preference_watchers_.Add(std::move(preference_watcher));
  return new_context;
}

void WebWorkerFetchContextImpl::ResetServiceWorkerURLLoaderFactory() {
  if (!web_loader_factory_)
    return;
  if (GetControllerServiceWorkerMode() !=
      blink::mojom::ControllerServiceWorkerMode::kControlled) {
    web_loader_factory_->SetServiceWorkerURLLoaderFactory(mojo::NullRemote());
    return;
  }
  if (!service_worker_container_host_)
    return;

  mojo::PendingRemote<network::mojom::URLLoaderFactory>
      service_worker_url_loader_factory;
  mojo::PendingRemote<blink::mojom::ServiceWorkerContainerHost>
      service_worker_container_host;
  service_worker_container_host_->CloneContainerHost(
      service_worker_container_host.InitWithNewPipeAndPassReceiver());
  // To avoid potential dead-lock while synchronous loading, create the
  // SubresourceLoaderFactory on a background thread.
  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CreateServiceWorkerSubresourceLoaderFactory,
          std::move(service_worker_container_host), client_id_,
          fallback_factory_->Clone(),
          service_worker_url_loader_factory.InitWithNewPipeAndPassReceiver(),
          task_runner, base::SequencedTaskRunnerHandle::Get(),
          base::BindRepeating(
              &WebWorkerFetchContextImpl::AddPendingWorkerTimingReceiver,
              weak_factory_.GetWeakPtr())));
  web_loader_factory_->SetServiceWorkerURLLoaderFactory(
      std::move(service_worker_url_loader_factory));
}

void WebWorkerFetchContextImpl::UpdateSubresourceLoaderFactories(
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
        subresource_loader_factories) {
  auto subresource_loader_factory_bundle =
      base::MakeRefCounted<ChildURLLoaderFactoryBundle>(
          std::make_unique<ChildPendingURLLoaderFactoryBundle>(
              std::move(subresource_loader_factories)));
  loader_factory_ = network::SharedURLLoaderFactory::Create(
      subresource_loader_factory_bundle->Clone());
  fallback_factory_ = network::SharedURLLoaderFactory::Create(
      subresource_loader_factory_bundle->CloneWithoutAppCacheFactory());
  web_loader_factory_ = std::make_unique<Factory>(
      resource_dispatcher_->GetWeakPtr(), loader_factory_);
  ResetServiceWorkerURLLoaderFactory();
}

void WebWorkerFetchContextImpl::NotifyUpdate(
    blink::mojom::RendererPreferencesPtr new_prefs) {
  if (accept_languages_watcher_ &&
      renderer_preferences_.accept_languages != new_prefs->accept_languages)
    accept_languages_watcher_->NotifyUpdate();
  renderer_preferences_ = *new_prefs;
  for (auto& watcher : child_preference_watchers_)
    watcher->NotifyUpdate(new_prefs.Clone());
}

blink::WebString WebWorkerFetchContextImpl::GetAcceptLanguages() const {
  return blink::WebString::FromUTF8(renderer_preferences_.accept_languages);
}

void WebWorkerFetchContextImpl::AddPendingWorkerTimingReceiver(
    int request_id,
    mojo::PendingReceiver<blink::mojom::WorkerTimingContainer> receiver) {
  // TODO(https://crbug.com/900700): Handle redirects properly. Currently on
  // redirect, the receiver is replaced with a new one, discarding the timings
  // before the redirect.
  worker_timing_container_receivers_[request_id] = std::move(receiver);
}

blink::CrossVariantMojoRemote<
    blink::mojom::ResourceLoadInfoNotifierInterfaceBase>
WebWorkerFetchContextImpl::CloneResourceLoadInfoNotifier() {
  if (!pending_resource_load_info_notifier_ && !resource_load_info_notifier_) {
    return blink::CrossVariantMojoRemote<
        blink::mojom::ResourceLoadInfoNotifierInterfaceBase>(
        mojo::NullRemote());
  }

  if (pending_resource_load_info_notifier_) {
    resource_load_info_notifier_.Bind(
        std::move(pending_resource_load_info_notifier_));
  }

  mojo::PendingRemote<blink::mojom::ResourceLoadInfoNotifier> pending_remote;
  resource_load_info_notifier_->Clone(
      pending_remote.InitWithNewPipeAndPassReceiver());
  return std::move(pending_remote);
}

}  // namespace content
