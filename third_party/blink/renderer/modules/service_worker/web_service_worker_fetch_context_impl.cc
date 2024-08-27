// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/web_service_worker_fetch_context_impl.h"

#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/cookies/site_for_cookies.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "third_party/blink/public/common/loader/loader_constants.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/url_loader_throttle_provider.h"
#include "third_party/blink/public/platform/web_url_request_extra_data.h"
#include "third_party/blink/public/platform/websocket_handshake_throttle_provider.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader_factory.h"
#include "third_party/blink/renderer/platform/loader/internet_disconnected_url_loader.h"

namespace blink {

// static
scoped_refptr<WebServiceWorkerFetchContext>
WebServiceWorkerFetchContext::Create(
    const RendererPreferences& renderer_preferences,
    const WebURL& worker_script_url,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_script_loader_factory,
    const WebURL& script_url_to_skip_throttling,
    std::unique_ptr<URLLoaderThrottleProvider> throttle_provider,
    std::unique_ptr<WebSocketHandshakeThrottleProvider>
        websocket_handshake_throttle_provider,
    CrossVariantMojoReceiver<
        mojom::blink::RendererPreferenceWatcherInterfaceBase>
        preference_watcher_receiver,
    CrossVariantMojoReceiver<
        mojom::blink::SubresourceLoaderUpdaterInterfaceBase>
        pending_subresource_loader_updater,
    const WebVector<WebString>& web_cors_exempt_header_list,
    const bool is_third_party_context) {
  base::UmaHistogramCounts100(
      "ServiceWorker.CorsExemptHeaderListSize",
      base::saturated_cast<int>(web_cors_exempt_header_list.size()));

  Vector<String> cors_exempt_header_list(
      base::checked_cast<wtf_size_t>(web_cors_exempt_header_list.size()));
  base::ranges::transform(web_cors_exempt_header_list,
                          cors_exempt_header_list.begin(),
                          &WebString::operator WTF::String);
  return base::MakeRefCounted<WebServiceWorkerFetchContextImpl>(
      renderer_preferences, KURL(worker_script_url.GetString()),
      std::move(pending_url_loader_factory),
      std::move(pending_script_loader_factory),
      KURL(script_url_to_skip_throttling.GetString()),
      std::move(throttle_provider),
      std::move(websocket_handshake_throttle_provider),
      std::move(preference_watcher_receiver),
      std::move(pending_subresource_loader_updater),
      std::move(cors_exempt_header_list), is_third_party_context);
}

WebServiceWorkerFetchContextImpl::WebServiceWorkerFetchContextImpl(
    const RendererPreferences& renderer_preferences,
    const KURL& worker_script_url,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_script_loader_factory,
    const KURL& script_url_to_skip_throttling,
    std::unique_ptr<URLLoaderThrottleProvider> throttle_provider,
    std::unique_ptr<WebSocketHandshakeThrottleProvider>
        websocket_handshake_throttle_provider,
    mojo::PendingReceiver<mojom::blink::RendererPreferenceWatcher>
        preference_watcher_receiver,
    mojo::PendingReceiver<mojom::blink::SubresourceLoaderUpdater>
        pending_subresource_loader_updater,
    Vector<String> cors_exempt_header_list,
    const bool is_third_party_context)
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
      cors_exempt_header_list_(std::move(cors_exempt_header_list)),
      is_third_party_context_(is_third_party_context) {}

WebServiceWorkerFetchContextImpl::~WebServiceWorkerFetchContextImpl() = default;

void WebServiceWorkerFetchContextImpl::SetTerminateSyncLoadEvent(
    base::WaitableEvent* terminate_sync_load_event) {
  DCHECK(!terminate_sync_load_event_);
  terminate_sync_load_event_ = terminate_sync_load_event;
}

void WebServiceWorkerFetchContextImpl::InitializeOnWorkerThread(
    AcceptLanguagesWatcher* watcher) {
  preference_watcher_receiver_.Bind(
      std::move(preference_watcher_pending_receiver_));
  subresource_loader_updater_.Bind(
      std::move(pending_subresource_loader_updater_));

  url_loader_factory_ = std::make_unique<URLLoaderFactory>(
      network::SharedURLLoaderFactory::Create(
          std::move(pending_url_loader_factory_)),
      cors_exempt_header_list_, terminate_sync_load_event_);

  internet_disconnected_url_loader_factory_ =
      std::make_unique<InternetDisconnectedURLLoaderFactory>();

  if (pending_script_loader_factory_) {
    web_script_loader_factory_ = std::make_unique<URLLoaderFactory>(
        network::SharedURLLoaderFactory::Create(
            std::move(pending_script_loader_factory_)),
        cors_exempt_header_list_, terminate_sync_load_event_);
  }

  accept_languages_watcher_ = watcher;
}

URLLoaderFactory* WebServiceWorkerFetchContextImpl::GetURLLoaderFactory() {
  if (is_offline_mode_)
    return internet_disconnected_url_loader_factory_.get();
  return url_loader_factory_.get();
}

std::unique_ptr<URLLoaderFactory>
WebServiceWorkerFetchContextImpl::WrapURLLoaderFactory(
    CrossVariantMojoRemote<network::mojom::URLLoaderFactoryInterfaceBase>
        url_loader_factory) {
  return std::make_unique<URLLoaderFactory>(
      base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
          std::move(url_loader_factory)),
      cors_exempt_header_list_, terminate_sync_load_event_);
}

URLLoaderFactory* WebServiceWorkerFetchContextImpl::GetScriptLoaderFactory() {
  return web_script_loader_factory_.get();
}

void WebServiceWorkerFetchContextImpl::FinalizeRequest(WebURLRequest& request) {
  if (renderer_preferences_.enable_do_not_track) {
    request.SetHttpHeaderField(WebString::FromUTF8(kDoNotTrackHeader), "1");
  }
  auto url_request_extra_data = base::MakeRefCounted<WebURLRequestExtraData>();
  url_request_extra_data->set_originated_from_service_worker(true);

  request.SetURLRequestExtraData(std::move(url_request_extra_data));

  if (!renderer_preferences_.enable_referrers) {
    request.SetReferrerString(WebString());
    request.SetReferrerPolicy(network::mojom::ReferrerPolicy::kNever);
  }
}

WebVector<std::unique_ptr<URLLoaderThrottle>>
WebServiceWorkerFetchContextImpl::CreateThrottles(
    const network::ResourceRequest& request) {
  const bool needs_to_skip_throttling =
      KURL(request.url) == script_url_to_skip_throttling_ &&
      (request.destination ==
           network::mojom::RequestDestination::kServiceWorker ||
       request.destination == network::mojom::RequestDestination::kScript);
  if (needs_to_skip_throttling) {
    // Throttling is needed when the skipped script is loaded again because it's
    // served from ServiceWorkerInstalledScriptLoader after the second time,
    // while at the first time the script comes from
    // ServiceWorkerUpdatedScriptLoader which uses ThrottlingURLLoader in the
    // browser process. See also comments at
    // EmbeddedWorkerStartParams::script_url_to_skip_throttling.
    // TODO(https://crbug.com/993641): need to simplify throttling for service
    // worker scripts.
    script_url_to_skip_throttling_ = KURL();
  } else if (throttle_provider_) {
    return throttle_provider_->CreateThrottles(std::nullopt, request);
  }
  return {};
}

mojom::ControllerServiceWorkerMode
WebServiceWorkerFetchContextImpl::GetControllerServiceWorkerMode() const {
  return mojom::ControllerServiceWorkerMode::kNoController;
}

net::SiteForCookies WebServiceWorkerFetchContextImpl::SiteForCookies() const {
  if (is_third_party_context_) {
    return net::SiteForCookies();
  }
  return net::SiteForCookies::FromUrl(GURL(worker_script_url_));
}

std::optional<WebSecurityOrigin>
WebServiceWorkerFetchContextImpl::TopFrameOrigin() const {
  return std::nullopt;
}

std::unique_ptr<WebSocketHandshakeThrottle>
WebServiceWorkerFetchContextImpl::CreateWebSocketHandshakeThrottle(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  if (!websocket_handshake_throttle_provider_)
    return nullptr;
  return websocket_handshake_throttle_provider_->CreateThrottle(
      std::nullopt, std::move(task_runner));
}

void WebServiceWorkerFetchContextImpl::UpdateSubresourceLoaderFactories(
    std::unique_ptr<PendingURLLoaderFactoryBundle>
        subresource_loader_factories) {
  url_loader_factory_ = std::make_unique<URLLoaderFactory>(
      network::SharedURLLoaderFactory::Create(
          std::move(subresource_loader_factories)),
      cors_exempt_header_list_, terminate_sync_load_event_);
}

void WebServiceWorkerFetchContextImpl::NotifyUpdate(
    const RendererPreferences& new_prefs) {
  DCHECK(accept_languages_watcher_);
  if (renderer_preferences_.accept_languages != new_prefs.accept_languages)
    accept_languages_watcher_->NotifyUpdate();
  renderer_preferences_ = new_prefs;
}

WebString WebServiceWorkerFetchContextImpl::GetAcceptLanguages() const {
  return WebString::FromUTF8(renderer_preferences_.accept_languages);
}

void WebServiceWorkerFetchContextImpl::SetIsOfflineMode(bool is_offline_mode) {
  is_offline_mode_ = is_offline_mode;
}

}  // namespace blink
