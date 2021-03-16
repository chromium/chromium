// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_fetch_context_impl.h"

#include "base/feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/common/content_constants_internal.h"
#include "content/public/common/content_features.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "third_party/blink/public/platform/internet_disconnected_web_url_loader.h"
#include "third_party/blink/public/platform/url_loader_throttle_provider.h"
#include "third_party/blink/public/platform/web_url_loader_factory.h"
#include "third_party/blink/public/platform/web_url_request_extra_data.h"
#include "third_party/blink/public/platform/websocket_handshake_throttle_provider.h"

namespace content {

ServiceWorkerFetchContextImpl::ServiceWorkerFetchContextImpl(
    const blink::RendererPreferences& renderer_preferences,
    const GURL& worker_script_url,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_script_loader_factory,
    const GURL& script_url_to_skip_throttling,
    std::unique_ptr<blink::URLLoaderThrottleProvider> throttle_provider,
    std::unique_ptr<blink::WebSocketHandshakeThrottleProvider>
        websocket_handshake_throttle_provider,
    mojo::PendingReceiver<blink::mojom::RendererPreferenceWatcher>
        preference_watcher_receiver,
    mojo::PendingReceiver<blink::mojom::SubresourceLoaderUpdater>
        pending_subresource_loader_updater,
    const std::vector<std::string>& cors_exempt_header_list)
    : renderer_preferences_(renderer_preferences),
      worker_script_url_(worker_script_url),
      pending_url_loader_factory_(std::move(pending_url_loader_factory)),
      pending_script_loader_factory_(std::move(pending_script_loader_factory)),
      script_url_to_skip_throttling_(script_url_to_skip_throttling),
      throttle_provider_(std::move(throttle_provider)),
      websocket_handshake_throttle_provider_(
          std::move(websocket_handshake_throttle_provider)),
      preference_watcher_pending_receiver_(
          std::move(preference_watcher_receiver)),
      pending_subresource_loader_updater_(
          std::move(pending_subresource_loader_updater)),
      cors_exempt_header_list_(cors_exempt_header_list) {}

ServiceWorkerFetchContextImpl::~ServiceWorkerFetchContextImpl() {}

void ServiceWorkerFetchContextImpl::SetTerminateSyncLoadEvent(
    base::WaitableEvent* terminate_sync_load_event) {
  DCHECK(!terminate_sync_load_event_);
  terminate_sync_load_event_ = terminate_sync_load_event;
}

void ServiceWorkerFetchContextImpl::InitializeOnWorkerThread(
    blink::AcceptLanguagesWatcher* watcher) {
  preference_watcher_receiver_.Bind(
      std::move(preference_watcher_pending_receiver_));
  subresource_loader_updater_.Bind(
      std::move(pending_subresource_loader_updater_));

  web_url_loader_factory_ = std::make_unique<blink::WebURLLoaderFactory>(
      network::SharedURLLoaderFactory::Create(
          std::move(pending_url_loader_factory_)),
      cors_exempt_header_list(), terminate_sync_load_event_);

  internet_disconnected_web_url_loader_factory_ =
      std::make_unique<blink::InternetDisconnectedWebURLLoaderFactory>();

  if (pending_script_loader_factory_) {
    web_script_loader_factory_ = std::make_unique<blink::WebURLLoaderFactory>(
        network::SharedURLLoaderFactory::Create(
            std::move(pending_script_loader_factory_)),
        cors_exempt_header_list(), terminate_sync_load_event_);
  }

  accept_languages_watcher_ = watcher;
}

blink::WebURLLoaderFactory*
ServiceWorkerFetchContextImpl::GetURLLoaderFactory() {
  if (is_offline_mode_)
    return internet_disconnected_web_url_loader_factory_.get();
  return web_url_loader_factory_.get();
}

std::unique_ptr<blink::WebURLLoaderFactory>
ServiceWorkerFetchContextImpl::WrapURLLoaderFactory(
    blink::CrossVariantMojoRemote<network::mojom::URLLoaderFactoryInterfaceBase>
        url_loader_factory) {
  return std::make_unique<blink::WebURLLoaderFactory>(
      base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
          std::move(url_loader_factory)),
      cors_exempt_header_list(), terminate_sync_load_event_);
}

blink::WebURLLoaderFactory*
ServiceWorkerFetchContextImpl::GetScriptLoaderFactory() {
  return web_script_loader_factory_.get();
}

void ServiceWorkerFetchContextImpl::WillSendRequest(
    blink::WebURLRequest& request) {
  if (renderer_preferences_.enable_do_not_track) {
    request.SetHttpHeaderField(blink::WebString::FromUTF8(kDoNotTrackHeader),
                               "1");
  }
  auto url_request_extra_data =
      base::MakeRefCounted<blink::WebURLRequestExtraData>();
  url_request_extra_data->set_originated_from_service_worker(true);

  const bool needs_to_skip_throttling =
      static_cast<GURL>(request.Url()) == script_url_to_skip_throttling_ &&
      (request.GetRequestContext() ==
           blink::mojom::RequestContextType::SERVICE_WORKER ||
       request.GetRequestContext() == blink::mojom::RequestContextType::SCRIPT);
  if (needs_to_skip_throttling) {
    // Throttling is needed when the skipped script is loaded again because it's
    // served from ServiceWorkerInstalledScriptLoader after the second time,
    // while at the first time the script comes from
    // ServiceWorkerUpdatedScriptLoader which uses ThrottlingURLLoader in the
    // browser process. See also comments at
    // EmbeddedWorkerStartParams::script_url_to_skip_throttling.
    // TODO(https://crbug.com/993641): need to simplify throttling for service
    // worker scripts.
    script_url_to_skip_throttling_ = GURL();
  } else if (throttle_provider_) {
    url_request_extra_data->set_url_loader_throttles(
        throttle_provider_->CreateThrottles(MSG_ROUTING_NONE, request));
  }

  request.SetURLRequestExtraData(std::move(url_request_extra_data));

  if (!renderer_preferences_.enable_referrers) {
    request.SetReferrerString(blink::WebString());
    request.SetReferrerPolicy(network::mojom::ReferrerPolicy::kNever);
  }
}

blink::mojom::ControllerServiceWorkerMode
ServiceWorkerFetchContextImpl::GetControllerServiceWorkerMode() const {
  return blink::mojom::ControllerServiceWorkerMode::kNoController;
}

net::SiteForCookies ServiceWorkerFetchContextImpl::SiteForCookies() const {
  // According to the spec, we can use the |worker_script_url_| for
  // SiteForCookies, because "site for cookies" for the service worker is
  // the service worker's origin's host's registrable domain.
  // https://tools.ietf.org/html/draft-ietf-httpbis-cookie-same-site-07#section-2.1.2
  return net::SiteForCookies::FromUrl(worker_script_url_);
}

base::Optional<blink::WebSecurityOrigin>
ServiceWorkerFetchContextImpl::TopFrameOrigin() const {
  return base::nullopt;
}

std::unique_ptr<blink::WebSocketHandshakeThrottle>
ServiceWorkerFetchContextImpl::CreateWebSocketHandshakeThrottle(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  if (!websocket_handshake_throttle_provider_)
    return nullptr;
  return websocket_handshake_throttle_provider_->CreateThrottle(
      MSG_ROUTING_NONE, std::move(task_runner));
}

blink::mojom::SubresourceLoaderUpdater*
ServiceWorkerFetchContextImpl::GetSubresourceLoaderUpdater() {
  return this;
}

void ServiceWorkerFetchContextImpl::UpdateSubresourceLoaderFactories(
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
        subresource_loader_factories) {
  web_url_loader_factory_ = std::make_unique<blink::WebURLLoaderFactory>(
      network::SharedURLLoaderFactory::Create(
          std::move(subresource_loader_factories)),
      cors_exempt_header_list(), terminate_sync_load_event_);
}

void ServiceWorkerFetchContextImpl::NotifyUpdate(
    const blink::RendererPreferences& new_prefs) {
  DCHECK(accept_languages_watcher_);
  if (renderer_preferences_.accept_languages != new_prefs.accept_languages)
    accept_languages_watcher_->NotifyUpdate();
  renderer_preferences_ = new_prefs;
}

blink::WebVector<blink::WebString>
ServiceWorkerFetchContextImpl::cors_exempt_header_list() {
  blink::WebVector<blink::WebString> web_cors_exempt_header_list(
      cors_exempt_header_list_.size());
  std::transform(
      cors_exempt_header_list_.begin(), cors_exempt_header_list_.end(),
      web_cors_exempt_header_list.begin(),
      [](const std::string& h) { return blink::WebString::FromLatin1(h); });
  return web_cors_exempt_header_list;
}

blink::WebString ServiceWorkerFetchContextImpl::GetAcceptLanguages() const {
  return blink::WebString::FromUTF8(renderer_preferences_.accept_languages);
}

blink::CrossVariantMojoReceiver<
    blink::mojom::WorkerTimingContainerInterfaceBase>
ServiceWorkerFetchContextImpl::TakePendingWorkerTimingReceiver(int request_id) {
  // No receiver exists because requests from service workers are never handled
  // by a service worker.
  return {};
}

void ServiceWorkerFetchContextImpl::SetIsOfflineMode(bool is_offline_mode) {
  is_offline_mode_ = is_offline_mode;
}

}  // namespace content
