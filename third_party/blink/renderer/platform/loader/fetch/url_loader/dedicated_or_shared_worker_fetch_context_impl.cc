// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/dedicated_or_shared_worker_fetch_context_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/loader_constants.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_fetch_handler_bypass_option.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"
#include "third_party/blink/public/platform/child_url_loader_factory_bundle.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_provider_context.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/public/platform/url_loader_throttle_provider.h"
#include "third_party/blink/public/platform/weak_wrapper_resource_load_info_notifier.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url_request_extra_data.h"
#include "third_party/blink/public/platform/websocket_handshake_throttle_provider.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader_factory.h"
#include "url/url_constants.h"

namespace blink {

DedicatedOrSharedWorkerFetchContextImpl::RewriteURLFunction g_rewrite_url =
    nullptr;

namespace {

// Runs on a background thread created in ResetServiceWorkerURLLoaderFactory().
void CreateServiceWorkerSubresourceLoaderFactory(
    CrossVariantMojoRemote<mojom::ServiceWorkerContainerHostInterfaceBase>
        service_worker_container_host,
    const WebString& client_id,
    std::unique_ptr<network::PendingSharedURLLoaderFactory> fallback_factory,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  Platform::Current()->CreateServiceWorkerSubresourceLoaderFactory(
      std::move(service_worker_container_host), client_id,
      std::move(fallback_factory), std::move(receiver), std::move(task_runner));
}

}  // namespace

// An implementation of URLLoaderFactory that is aware of service workers. In
// the usual case, it creates a loader that uses |loader_factory_|. But if the
// worker fetch context is controlled by a service worker, it creates a loader
// that uses |service_worker_loader_factory_| for requests that should be
// intercepted by the service worker.
class DedicatedOrSharedWorkerFetchContextImpl::Factory
    : public URLLoaderFactory {
 public:
  Factory(scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
          const Vector<String>& cors_exempt_header_list,
          base::WaitableEvent* terminate_sync_load_event)
      : URLLoaderFactory(std::move(loader_factory),
                         cors_exempt_header_list,
                         terminate_sync_load_event) {}
  Factory(const Factory&) = delete;
  Factory& operator=(const Factory&) = delete;
  ~Factory() override = default;

  std::unique_ptr<URLLoader> CreateURLLoader(
      const network::ResourceRequest& request,
      scoped_refptr<base::SingleThreadTaskRunner> freezable_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> unfreezable_task_runner,
      mojo::PendingRemote<mojom::blink::KeepAliveHandle> keep_alive_handle,
      BackForwardCacheLoaderHelper* back_forward_cache_loader_helper,
      Vector<std::unique_ptr<URLLoaderThrottle>> throttles) override {
    DCHECK(freezable_task_runner);
    DCHECK(unfreezable_task_runner);

    // Create our own URLLoader to route the request to the controller service
    // worker.
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory =
        CanCreateServiceWorkerURLLoader(request)
            ? service_worker_loader_factory_
            : loader_factory_;

    return std::make_unique<URLLoader>(
        cors_exempt_header_list_, terminate_sync_load_event_,
        std::move(freezable_task_runner), std::move(unfreezable_task_runner),
        std::move(loader_factory), std::move(keep_alive_handle),
        back_forward_cache_loader_helper, std::move(throttles));
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
  bool CanCreateServiceWorkerURLLoader(
      const network::ResourceRequest& request) {
    // TODO(horo): Unify this code path with
    // ServiceWorkerNetworkProviderForFrame::CreateURLLoader that is used
    // for document cases.

    // We need the service worker loader factory populated in order to create
    // our own URLLoader for subresource loading via a service worker.
    if (!service_worker_loader_factory_)
      return false;

    // If the URL is not http(s) or otherwise allowed, do not intercept the
    // request. Schemes like 'blob' and 'file' are not eligible to be
    // intercepted by service workers.
    // TODO(falken): Let ServiceWorkerSubresourceLoaderFactory handle the
    // request and move this check there (i.e., for such URLs, it should use
    // its fallback factory).
    if (!request.url.SchemeIsHTTPOrHTTPS() &&
        !Platform::Current()->OriginCanAccessServiceWorkers(request.url)) {
      return false;
    }

    // If `skip_service_worker` is true, no need to intercept the request.
    if (request.skip_service_worker) {
      return false;
    }

    return true;
  }

  scoped_refptr<network::SharedURLLoaderFactory> service_worker_loader_factory_;
  base::WeakPtrFactory<Factory> weak_ptr_factory_{this};
};

DedicatedOrSharedWorkerFetchContextImpl::
    DedicatedOrSharedWorkerFetchContextImpl(
        const RendererPreferences& renderer_preferences,
        mojo::PendingReceiver<mojom::blink::RendererPreferenceWatcher>
            preference_watcher_receiver,
        mojo::PendingReceiver<mojom::blink::ServiceWorkerWorkerClient>
            service_worker_client_receiver,
        mojo::PendingRemote<mojom::blink::ServiceWorkerWorkerClientRegistry>
            pending_service_worker_worker_client_registry,
        CrossVariantMojoRemote<mojom::ServiceWorkerContainerHostInterfaceBase>
            service_worker_container_host,
        std::unique_ptr<network::PendingSharedURLLoaderFactory>
            pending_loader_factory,
        std::unique_ptr<network::PendingSharedURLLoaderFactory>
            pending_fallback_factory,
        mojo::PendingReceiver<mojom::blink::SubresourceLoaderUpdater>
            pending_subresource_loader_updater,
        std::unique_ptr<URLLoaderThrottleProvider> throttle_provider,
        std::unique_ptr<WebSocketHandshakeThrottleProvider>
            websocket_handshake_throttle_provider,
        Vector<String> cors_exempt_header_list,
        mojo::PendingRemote<mojom::ResourceLoadInfoNotifier>
            pending_resource_load_info_notifier)
    : service_worker_client_receiver_(
          std::move(service_worker_client_receiver)),
      pending_service_worker_worker_client_registry_(
          std::move(pending_service_worker_worker_client_registry)),
      pending_loader_factory_(std::move(pending_loader_factory)),
      pending_fallback_factory_(std::move(pending_fallback_factory)),
      service_worker_container_host_(std::move(service_worker_container_host)),
      pending_subresource_loader_updater_(
          std::move(pending_subresource_loader_updater)),
      renderer_preferences_(renderer_preferences),
      preference_watcher_pending_receiver_(
          std::move(preference_watcher_receiver)),
      throttle_provider_(std::move(throttle_provider)),
      websocket_handshake_throttle_provider_(
          std::move(websocket_handshake_throttle_provider)),
      cors_exempt_header_list_(std::move(cors_exempt_header_list)),
      pending_resource_load_info_notifier_(
          std::move(pending_resource_load_info_notifier)) {}

scoped_refptr<WebDedicatedOrSharedWorkerFetchContext>
DedicatedOrSharedWorkerFetchContextImpl::CloneForNestedWorkerDeprecated(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(!base::FeatureList::IsEnabled(features::kPlzDedicatedWorker));

  mojo::PendingReceiver<mojom::blink::ServiceWorkerWorkerClient>
      service_worker_client_receiver;
  mojo::PendingRemote<mojom::blink::ServiceWorkerWorkerClientRegistry>
      service_worker_worker_client_registry;
  if (service_worker_worker_client_registry_) {
    mojo::PendingRemote<mojom::blink::ServiceWorkerWorkerClient>
        service_worker_client;
    service_worker_client_receiver =
        service_worker_client.InitWithNewPipeAndPassReceiver();
    service_worker_worker_client_registry_->RegisterWorkerClient(
        std::move(service_worker_client));
    service_worker_worker_client_registry_->CloneWorkerClientRegistry(
        service_worker_worker_client_registry.InitWithNewPipeAndPassReceiver());
  }

  CrossVariantMojoRemote<mojom::ServiceWorkerContainerHostInterfaceBase>
      cloned_service_worker_container_host;
  if (service_worker_container_host_) {
    std::tie(service_worker_container_host_,
             cloned_service_worker_container_host) =
        Platform::Current()->CloneServiceWorkerContainerHost(
            std::move(service_worker_container_host_));
  }

  // |pending_subresource_loader_updater| is not used for
  // non-PlzDedicatedWorker.
  scoped_refptr<DedicatedOrSharedWorkerFetchContextImpl> new_context =
      CloneForNestedWorkerInternal(
          std::move(service_worker_client_receiver),
          std::move(service_worker_worker_client_registry),
          std::move(cloned_service_worker_container_host),
          loader_factory_->Clone(), fallback_factory_->Clone(),
          /*pending_subresource_loader_updater=*/mojo::NullReceiver(),
          std::move(task_runner));
  new_context->controller_service_worker_mode_ =
      controller_service_worker_mode_;

  return new_context;
}

scoped_refptr<WebDedicatedOrSharedWorkerFetchContext>
DedicatedOrSharedWorkerFetchContextImpl::CloneForNestedWorker(
    WebServiceWorkerProviderContext* service_worker_provider_context,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_loader_factory,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_fallback_factory,
    CrossVariantMojoReceiver<mojom::SubresourceLoaderUpdaterInterfaceBase>
        pending_subresource_loader_updater,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(base::FeatureList::IsEnabled(features::kPlzDedicatedWorker));
  DCHECK(pending_loader_factory);
  DCHECK(pending_fallback_factory);
  DCHECK(task_runner);

  if (!service_worker_provider_context) {
    return CloneForNestedWorkerInternal(
        /*service_worker_client_receiver=*/mojo::NullReceiver(),
        /*service_worker_worker_client_registry=*/mojo::NullRemote(),
        /*container_host=*/
        CrossVariantMojoRemote<
            mojom::ServiceWorkerContainerHostInterfaceBase>(),
        std::move(pending_loader_factory), std::move(pending_fallback_factory),
        std::move(pending_subresource_loader_updater), std::move(task_runner));
  }

  mojo::PendingRemote<mojom::blink::ServiceWorkerWorkerClientRegistry>
      service_worker_worker_client_registry;
  service_worker_provider_context
      ->BindServiceWorkerWorkerClientRegistryReceiver(
          service_worker_worker_client_registry
              .InitWithNewPipeAndPassReceiver());

  mojo::PendingRemote<mojom::blink::ServiceWorkerWorkerClient> worker_client;
  mojo::PendingReceiver<mojom::blink::ServiceWorkerWorkerClient>
      service_worker_client_receiver =
          worker_client.InitWithNewPipeAndPassReceiver();
  service_worker_provider_context->BindServiceWorkerWorkerClientRemote(
      std::move(worker_client));

  CrossVariantMojoRemote<mojom::ServiceWorkerContainerHostInterfaceBase>
      service_worker_container_host =
          service_worker_provider_context->CloneRemoteContainerHost();

  scoped_refptr<DedicatedOrSharedWorkerFetchContextImpl> new_context =
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

void DedicatedOrSharedWorkerFetchContextImpl::SetAncestorFrameToken(
    const LocalFrameToken& token) {
  ancestor_frame_token_ = token;
}

void DedicatedOrSharedWorkerFetchContextImpl::set_site_for_cookies(
    const net::SiteForCookies& site_for_cookies) {
  site_for_cookies_ = site_for_cookies;
}

void DedicatedOrSharedWorkerFetchContextImpl::set_top_frame_origin(
    const WebSecurityOrigin& top_frame_origin) {
  top_frame_origin_ = top_frame_origin;
}

void DedicatedOrSharedWorkerFetchContextImpl::SetTerminateSyncLoadEvent(
    base::WaitableEvent* terminate_sync_load_event) {
  DCHECK(!terminate_sync_load_event_);
  terminate_sync_load_event_ = terminate_sync_load_event;
}

void DedicatedOrSharedWorkerFetchContextImpl::InitializeOnWorkerThread(
    AcceptLanguagesWatcher* watcher) {
  DCHECK(!receiver_.is_bound());
  DCHECK(!preference_watcher_receiver_.is_bound());

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

  if (pending_resource_load_info_notifier_) {
    resource_load_info_notifier_.Bind(
        std::move(pending_resource_load_info_notifier_));
    resource_load_info_notifier_.set_disconnect_handler(
        base::BindOnce(&DedicatedOrSharedWorkerFetchContextImpl::
                           ResetWeakWrapperResourceLoadInfoNotifier,
                       base::Unretained(this)));
  }

  accept_languages_watcher_ = watcher;

  DCHECK(loader_factory_);
  DCHECK(!web_loader_factory_);
  web_loader_factory_ = std::make_unique<Factory>(
      loader_factory_, cors_exempt_header_list_, terminate_sync_load_event_);

  ResetServiceWorkerURLLoaderFactory();
}

URLLoaderFactory*
DedicatedOrSharedWorkerFetchContextImpl::GetURLLoaderFactory() {
  return web_loader_factory_.get();
}

std::unique_ptr<URLLoaderFactory>
DedicatedOrSharedWorkerFetchContextImpl::WrapURLLoaderFactory(
    CrossVariantMojoRemote<network::mojom::URLLoaderFactoryInterfaceBase>
        url_loader_factory) {
  return std::make_unique<URLLoaderFactory>(
      base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
          std::move(url_loader_factory)),
      cors_exempt_header_list_, terminate_sync_load_event_);
}

std::optional<WebURL> DedicatedOrSharedWorkerFetchContextImpl::WillSendRequest(
    const WebURL& url) {
  if (g_rewrite_url) {
    return g_rewrite_url(url.GetString().Utf8(), false);
  }
  return std::nullopt;
}

void DedicatedOrSharedWorkerFetchContextImpl::FinalizeRequest(
    WebURLRequest& request) {
  if (renderer_preferences_.enable_do_not_track) {
    request.SetHttpHeaderField(WebString::FromUTF8(kDoNotTrackHeader), "1");
  }

  auto url_request_extra_data = base::MakeRefCounted<WebURLRequestExtraData>();
  request.SetURLRequestExtraData(std::move(url_request_extra_data));

  if (!renderer_preferences_.enable_referrers) {
    request.SetReferrerString(WebString());
    request.SetReferrerPolicy(network::mojom::ReferrerPolicy::kNever);
  }
}

WebVector<std::unique_ptr<URLLoaderThrottle>>
DedicatedOrSharedWorkerFetchContextImpl::CreateThrottles(
    const network::ResourceRequest& request) {
  if (throttle_provider_) {
    return throttle_provider_->CreateThrottles(ancestor_frame_token_, request);
  }
  return {};
}

mojom::ControllerServiceWorkerMode
DedicatedOrSharedWorkerFetchContextImpl::GetControllerServiceWorkerMode()
    const {
  return controller_service_worker_mode_;
}

void DedicatedOrSharedWorkerFetchContextImpl::SetIsOnSubframe(
    bool is_on_sub_frame) {
  is_on_sub_frame_ = is_on_sub_frame;
}

bool DedicatedOrSharedWorkerFetchContextImpl::IsOnSubframe() const {
  return is_on_sub_frame_;
}

net::SiteForCookies DedicatedOrSharedWorkerFetchContextImpl::SiteForCookies()
    const {
  return site_for_cookies_;
}

std::optional<WebSecurityOrigin>
DedicatedOrSharedWorkerFetchContextImpl::TopFrameOrigin() const {
  // TODO(jkarlin): set_top_frame_origin is only called for dedicated workers.
  // Determine the top-frame-origin of a shared worker as well. See
  // https://crbug.com/918868.
  return top_frame_origin_;
}

void DedicatedOrSharedWorkerFetchContextImpl::SetSubresourceFilterBuilder(
    std::unique_ptr<WebDocumentSubresourceFilter::Builder>
        subresource_filter_builder) {
  subresource_filter_builder_ = std::move(subresource_filter_builder);
}

std::unique_ptr<WebDocumentSubresourceFilter>
DedicatedOrSharedWorkerFetchContextImpl::TakeSubresourceFilter() {
  if (!subresource_filter_builder_)
    return nullptr;
  return std::move(subresource_filter_builder_)->Build();
}

std::unique_ptr<WebSocketHandshakeThrottle>
DedicatedOrSharedWorkerFetchContextImpl::CreateWebSocketHandshakeThrottle(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  if (!websocket_handshake_throttle_provider_)
    return nullptr;
  return websocket_handshake_throttle_provider_->CreateThrottle(
      ancestor_frame_token_, std::move(task_runner));
}

void DedicatedOrSharedWorkerFetchContextImpl::SetIsOfflineMode(
    bool is_offline_mode) {
  // Worker doesn't support offline mode. There should be no callers.
  NOTREACHED_IN_MIGRATION();
}

void DedicatedOrSharedWorkerFetchContextImpl::OnControllerChanged(
    mojom::ControllerServiceWorkerMode mode) {
  set_controller_service_worker_mode(mode);
  ResetServiceWorkerURLLoaderFactory();
}

void DedicatedOrSharedWorkerFetchContextImpl::
    set_controller_service_worker_mode(
        mojom::ControllerServiceWorkerMode mode) {
  controller_service_worker_mode_ = mode;
}

void DedicatedOrSharedWorkerFetchContextImpl::set_client_id(
    const WebString& client_id) {
  client_id_ = client_id;
}

WebString DedicatedOrSharedWorkerFetchContextImpl::GetAcceptLanguages() const {
  return WebString::FromUTF8(renderer_preferences_.accept_languages);
}

std::unique_ptr<ResourceLoadInfoNotifierWrapper>
DedicatedOrSharedWorkerFetchContextImpl::
    CreateResourceLoadInfoNotifierWrapper() {
  // If |resource_load_info_notifier_| is unbound, we will create
  // ResourceLoadInfoNotifierWrapper without wrapping a ResourceLoadInfoNotifier
  // and only collect histograms.
  if (!resource_load_info_notifier_) {
    return std::make_unique<ResourceLoadInfoNotifierWrapper>(
        /*resource_load_info_notifier=*/nullptr);
  }

  if (!weak_wrapper_resource_load_info_notifier_) {
    weak_wrapper_resource_load_info_notifier_ =
        std::make_unique<WeakWrapperResourceLoadInfoNotifier>(
            resource_load_info_notifier_.get());
  }
  return std::make_unique<ResourceLoadInfoNotifierWrapper>(
      weak_wrapper_resource_load_info_notifier_->AsWeakPtr());
}

DedicatedOrSharedWorkerFetchContextImpl::
    ~DedicatedOrSharedWorkerFetchContextImpl() = default;

scoped_refptr<DedicatedOrSharedWorkerFetchContextImpl>
DedicatedOrSharedWorkerFetchContextImpl::CloneForNestedWorkerInternal(
    mojo::PendingReceiver<mojom::blink::ServiceWorkerWorkerClient>
        service_worker_client_receiver,
    mojo::PendingRemote<mojom::blink::ServiceWorkerWorkerClientRegistry>
        service_worker_worker_client_registry,
    CrossVariantMojoRemote<mojom::ServiceWorkerContainerHostInterfaceBase>
        service_worker_container_host,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_loader_factory,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_fallback_factory,
    mojo::PendingReceiver<mojom::blink::SubresourceLoaderUpdater>
        pending_subresource_loader_updater,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  mojo::PendingRemote<mojom::ResourceLoadInfoNotifier>
      pending_resource_load_info_notifier;
  if (resource_load_info_notifier_) {
    resource_load_info_notifier_->Clone(
        pending_resource_load_info_notifier.InitWithNewPipeAndPassReceiver());
  }

  mojo::PendingRemote<mojom::blink::RendererPreferenceWatcher>
      preference_watcher;
  auto new_context = base::AdoptRef(new DedicatedOrSharedWorkerFetchContextImpl(
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
  new_context->ancestor_frame_token_ = ancestor_frame_token_;
  new_context->site_for_cookies_ = site_for_cookies_;
  new_context->top_frame_origin_ = top_frame_origin_;
  child_preference_watchers_.Add(std::move(preference_watcher));
  return new_context;
}

void DedicatedOrSharedWorkerFetchContextImpl::
    ResetServiceWorkerURLLoaderFactory() {
  if (!web_loader_factory_)
    return;
  if (GetControllerServiceWorkerMode() !=
      mojom::ControllerServiceWorkerMode::kControlled) {
    web_loader_factory_->SetServiceWorkerURLLoaderFactory(mojo::NullRemote());
    return;
  }
  if (!service_worker_container_host_)
    return;

  mojo::PendingRemote<network::mojom::URLLoaderFactory>
      service_worker_url_loader_factory;
  CrossVariantMojoRemote<mojom::ServiceWorkerContainerHostInterfaceBase>
      cloned_service_worker_container_host;
  std::tie(service_worker_container_host_,
           cloned_service_worker_container_host) =
      Platform::Current()->CloneServiceWorkerContainerHost(
          std::move(service_worker_container_host_));

  // To avoid potential dead-lock while synchronous loading, create the
  // SubresourceLoaderFactory on a background thread.
  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CreateServiceWorkerSubresourceLoaderFactory,
          std::move(cloned_service_worker_container_host), client_id_,
          fallback_factory_->Clone(),
          service_worker_url_loader_factory.InitWithNewPipeAndPassReceiver(),
          task_runner));
  web_loader_factory_->SetServiceWorkerURLLoaderFactory(
      std::move(service_worker_url_loader_factory));
}

void DedicatedOrSharedWorkerFetchContextImpl::UpdateSubresourceLoaderFactories(
    std::unique_ptr<PendingURLLoaderFactoryBundle>
        subresource_loader_factories) {
  auto subresource_loader_factory_bundle =
      base::MakeRefCounted<ChildURLLoaderFactoryBundle>(
          std::make_unique<ChildPendingURLLoaderFactoryBundle>(
              std::move(subresource_loader_factories)));
  loader_factory_ = network::SharedURLLoaderFactory::Create(
      subresource_loader_factory_bundle->Clone());
  fallback_factory_ = network::SharedURLLoaderFactory::Create(
      subresource_loader_factory_bundle->Clone());
  web_loader_factory_ = std::make_unique<Factory>(
      loader_factory_, cors_exempt_header_list_, terminate_sync_load_event_);
  ResetServiceWorkerURLLoaderFactory();
}

void DedicatedOrSharedWorkerFetchContextImpl::NotifyUpdate(
    const RendererPreferences& new_prefs) {
  if (accept_languages_watcher_ &&
      renderer_preferences_.accept_languages != new_prefs.accept_languages)
    accept_languages_watcher_->NotifyUpdate();
  renderer_preferences_ = new_prefs;
  for (auto& watcher : child_preference_watchers_)
    watcher->NotifyUpdate(new_prefs);
}

void DedicatedOrSharedWorkerFetchContextImpl::
    ResetWeakWrapperResourceLoadInfoNotifier() {
  weak_wrapper_resource_load_info_notifier_.reset();
}

// static
scoped_refptr<WebDedicatedOrSharedWorkerFetchContext>
WebDedicatedOrSharedWorkerFetchContext::Create(
    WebServiceWorkerProviderContext* provider_context,
    const RendererPreferences& renderer_preferences,
    CrossVariantMojoReceiver<mojom::RendererPreferenceWatcherInterfaceBase>
        watcher_receiver,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_loader_factory,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_fallback_factory,
    CrossVariantMojoReceiver<mojom::SubresourceLoaderUpdaterInterfaceBase>
        pending_subresource_loader_updater,
    const WebVector<WebString>& web_cors_exempt_header_list,
    mojo::PendingRemote<mojom::ResourceLoadInfoNotifier>
        pending_resource_load_info_notifier) {
  mojo::PendingReceiver<mojom::blink::ServiceWorkerWorkerClient>
      service_worker_client_receiver;
  mojo::PendingRemote<mojom::blink::ServiceWorkerWorkerClientRegistry>
      service_worker_worker_client_registry;
  CrossVariantMojoRemote<mojom::ServiceWorkerContainerHostInterfaceBase>
      service_worker_container_host;
  // Some sandboxed iframes are not allowed to use service worker so don't have
  // a real service worker provider, so the provider context is null.
  if (provider_context) {
    provider_context->BindServiceWorkerWorkerClientRegistryReceiver(
        service_worker_worker_client_registry.InitWithNewPipeAndPassReceiver());

    mojo::PendingRemote<mojom::blink::ServiceWorkerWorkerClient> worker_client;
    service_worker_client_receiver =
        worker_client.InitWithNewPipeAndPassReceiver();
    provider_context->BindServiceWorkerWorkerClientRemote(
        std::move(worker_client));

    service_worker_container_host =
        provider_context->CloneRemoteContainerHost();
  }

  Vector<String> cors_exempt_header_list(
      base::checked_cast<wtf_size_t>(web_cors_exempt_header_list.size()));
  base::ranges::transform(web_cors_exempt_header_list,
                          cors_exempt_header_list.begin(),
                          &WebString::operator WTF::String);

  scoped_refptr<DedicatedOrSharedWorkerFetchContextImpl> worker_fetch_context =
      base::AdoptRef(new DedicatedOrSharedWorkerFetchContextImpl(
          renderer_preferences, std::move(watcher_receiver),
          std::move(service_worker_client_receiver),
          std::move(service_worker_worker_client_registry),
          std::move(service_worker_container_host),
          std::move(pending_loader_factory),
          std::move(pending_fallback_factory),
          std::move(pending_subresource_loader_updater),
          Platform::Current()->CreateURLLoaderThrottleProviderForWorker(
              URLLoaderThrottleProviderType::kWorker),
          Platform::Current()->CreateWebSocketHandshakeThrottleProvider(),
          std::move(cors_exempt_header_list),
          std::move(pending_resource_load_info_notifier)));
  if (provider_context) {
    worker_fetch_context->set_controller_service_worker_mode(
        provider_context->GetControllerServiceWorkerMode());
    worker_fetch_context->set_client_id(provider_context->client_id());
  } else {
    worker_fetch_context->set_controller_service_worker_mode(
        mojom::ControllerServiceWorkerMode::kNoController);
  }
  return worker_fetch_context;
}

// static
void WebDedicatedOrSharedWorkerFetchContext::InstallRewriteURLFunction(
    RewriteURLFunction rewrite_url) {
  CHECK(!g_rewrite_url);
  g_rewrite_url = rewrite_url;
}

}  // namespace blink
