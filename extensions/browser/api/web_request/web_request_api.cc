// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_api.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/browser/api/web_request/extension_web_request_event_router.h"
#include "extensions/browser/api/web_request/web_request_api_constants.h"
#include "extensions/browser/api/web_request/web_request_api_helpers.h"
#include "extensions/browser/api/web_request/web_request_proxying_url_loader_factory.h"
#include "extensions/browser/api/web_request/web_request_proxying_websocket.h"
#include "extensions/browser/api/web_request/web_request_proxying_webtransport.h"
#include "extensions/browser/browser_frame_context_data.h"
#include "extensions/browser/browser_process_context_data.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_navigation_ui_data.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/warning_service.h"
#include "extensions/browser/warning_set.h"
#include "extensions/common/api/web_request.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/auth.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_util.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/web_transport.mojom.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_GUEST_VIEW)
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#endif

using content::BrowserThread;
using extension_web_request_api_helpers::ExtraInfoSpec;
using extensions::mojom::APIPermissionID;

namespace helpers = extension_web_request_api_helpers;
namespace keys = extension_web_request_api_constants;
using URLLoaderFactoryType =
    content::ContentBrowserClient::URLLoaderFactoryType;

namespace extensions {

namespace web_request = api::web_request;

namespace {

// Converts an HttpHeaders dictionary to a |name|, |value| pair. Returns
// true if successful.
bool FromHeaderDictionary(const base::Value::Dict& header_value,
                          std::string* name,
                          std::string* out_value) {
  const std::string* name_ptr = header_value.FindString(keys::kHeaderNameKey);
  if (!name) {
    return false;
  }
  *name = *name_ptr;

  const base::Value* value = header_value.Find(keys::kHeaderValueKey);
  const base::Value* binary_value =
      header_value.Find(keys::kHeaderBinaryValueKey);
  // We require either a "value" or a "binaryValue" entry, but not both.
  if ((value == nullptr && binary_value == nullptr) ||
      (value != nullptr && binary_value != nullptr)) {
    return false;
  }

  if (value) {
    if (!value->is_string()) {
      return false;
    }
    *out_value = value->GetString();
  } else if (!binary_value->is_list() ||
             !helpers::CharListToString(binary_value->GetList(), out_value)) {
    return false;
  }
  return true;
}

// Checks whether the extension has any permissions that would intercept or
// modify network requests.
bool HasAnyWebRequestPermissions(const Extension* extension) {
  static constexpr APIPermissionID kWebRequestPermissions[] = {
      APIPermissionID::kWebRequest,
      APIPermissionID::kWebRequestBlocking,
      APIPermissionID::kDeclarativeWebRequest,
      APIPermissionID::kDeclarativeNetRequest,
      APIPermissionID::kDeclarativeNetRequestWithHostAccess,
      APIPermissionID::kWebView,
  };

  const PermissionsData* permissions = extension->permissions_data();
  for (auto permission : kWebRequestPermissions) {
    if (permissions->HasAPIPermission(permission)) {
      return true;
    }
  }
  return false;
}

// Mirrors the histogram enum of the same name. DO NOT REORDER THESE VALUES OR
// CHANGE THEIR MEANING.
enum class WebRequestEventListenerFlag {
  kTotal,
  kNone,
  kRequestHeaders,
  kResponseHeaders,
  kBlocking,
  kAsyncBlocking,
  kRequestBody,
  kExtraHeaders,
  kMaxValue = kExtraHeaders,
};

}  // namespace

void WebRequestAPI::Proxy::HandleAuthRequest(
    const net::AuthChallengeInfo& auth_info,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    int32_t request_id,
    AuthRequestCallback callback) {
  // Default implementation cancels the request.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::nullopt,
                                false /* should_cancel */));
}

WebRequestAPI::ProxySet::ProxySet() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

WebRequestAPI::ProxySet::~ProxySet() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void WebRequestAPI::ProxySet::AddProxy(std::unique_ptr<Proxy> proxy) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  proxies_.insert(std::move(proxy));
}

void WebRequestAPI::ProxySet::RemoveProxy(Proxy* proxy) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto requests_it = proxy_to_request_id_map_.find(proxy);
  if (requests_it != proxy_to_request_id_map_.end()) {
    for (const auto& id : requests_it->second) {
      request_id_to_proxy_map_.erase(id);
    }
    proxy_to_request_id_map_.erase(requests_it);
  }

  auto proxy_it = proxies_.find(proxy);
  CHECK(proxy_it != proxies_.end(), base::NotFatalUntil::M130);
  proxies_.erase(proxy_it);
}

void WebRequestAPI::ProxySet::AssociateProxyWithRequestId(
    Proxy* proxy,
    const content::GlobalRequestID& id) {
  DCHECK(proxy);
  DCHECK(proxies_.count(proxy));
  DCHECK(id.request_id);
  auto result = request_id_to_proxy_map_.emplace(id, proxy);
  DCHECK(result.second) << "Unexpected request ID collision.";
  proxy_to_request_id_map_[proxy].insert(id);
}

void WebRequestAPI::ProxySet::DisassociateProxyWithRequestId(
    Proxy* proxy,
    const content::GlobalRequestID& id) {
  DCHECK(proxy);
  DCHECK(proxies_.count(proxy));
  DCHECK(id.request_id);
  size_t count = request_id_to_proxy_map_.erase(id);
  DCHECK_GT(count, 0u);
  count = proxy_to_request_id_map_[proxy].erase(id);
  DCHECK_GT(count, 0u);
}

WebRequestAPI::Proxy* WebRequestAPI::ProxySet::GetProxyFromRequestId(
    const content::GlobalRequestID& id) {
  auto it = request_id_to_proxy_map_.find(id);
  return it == request_id_to_proxy_map_.end() ? nullptr : it->second;
}

void WebRequestAPI::ProxySet::MaybeProxyAuthRequest(
    const net::AuthChallengeInfo& auth_info,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    const content::GlobalRequestID& request_id,
    AuthRequestCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Proxy* proxy = GetProxyFromRequestId(request_id);
  if (!proxy) {
    // Run the |callback| which will display a dialog for the user to enter
    // their auth credentials.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt,
                                  false /* should_cancel */));
    return;
  }

  proxy->HandleAuthRequest(auth_info, std::move(response_headers),
                           request_id.request_id, std::move(callback));
}

void WebRequestAPI::ProxySet::OnDNRExtensionUnloaded(
    const Extension* extension) {
  for (const auto& proxy : proxies_) {
    proxy->OnDNRExtensionUnloaded(extension);
  }
}

WebRequestAPI::RequestIDGenerator::RequestIDGenerator() = default;
WebRequestAPI::RequestIDGenerator::~RequestIDGenerator() = default;

int64_t WebRequestAPI::RequestIDGenerator::Generate(
    int32_t routing_id,
    int32_t network_service_request_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto it = saved_id_map_.find({routing_id, network_service_request_id});
  if (it != saved_id_map_.end()) {
    int64_t id = it->second;
    saved_id_map_.erase(it);
    return id;
  }
  return ++id_;
}

void WebRequestAPI::RequestIDGenerator::SaveID(
    int32_t routing_id,
    int32_t network_service_request_id,
    uint64_t request_id) {
  // If |network_service_request_id| is 0, we cannot reliably match the
  // generated ID to a future request, so ignore it.
  if (network_service_request_id != 0) {
    saved_id_map_.insert(
        {{routing_id, network_service_request_id}, request_id});
  }
}

WebRequestAPI::WebRequestAPI(content::BrowserContext* context)
    : browser_context_(context),
      proxies_(std::make_unique<ProxySet>()),
      may_have_proxies_(MayHaveProxies()) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  // TODO(crbug.com/40393861): Once ExtensionWebRequestEventRouter is a per-
  // BrowserContext instance, it can observe these events itself. That's a
  // bit tricky right now because the singleton instance would need to
  // observe the EventRouter for each BrowserContext that has webRequest
  // API event listeners.
  // Observe related events in the EventRouter for the WebRequestEventRouter.
  for (std::string event_name : WebRequestEventRouter::GetEventNames()) {
    event_router->RegisterObserver(this, event_name);
  }
  extensions::ExtensionRegistry::Get(browser_context_)->AddObserver(this);
}

WebRequestAPI::~WebRequestAPI() = default;

void WebRequestAPI::Shutdown() {
  proxies_.reset();
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
  extensions::ExtensionRegistry::Get(browser_context_)->RemoveObserver(this);
  // TODO(crbug.com/40264286): Remove this once WebRequestEventRouter
  // implements `KeyedService::Shutdown` correctly.
  WebRequestEventRouter::Get(browser_context_)
      ->OnBrowserContextShutdown(browser_context_);
}

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<WebRequestAPI>>::DestructorAtExit g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<WebRequestAPI>*
WebRequestAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

void WebRequestAPI::OnListenerRemoved(const EventListenerInfo& details) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(fsamuel): <webview> events will not be removed through this code path.
  // <webview> events will be removed in RemoveWebViewEventListeners. Ideally,
  // this code should be decoupled from extensions, we should use the host ID
  // instead, and not have two different code paths. This is a huge undertaking
  // unfortunately, so we'll resort to two code paths for now.

  // Note that details.event_name includes the sub-event details (e.g. "/123").
  const std::string& sub_event_name = details.event_name;

  // The way we handle the listener removal depends on whether this was a
  // lazy listener registration (indicated by a null browser context on
  // `details`).
  base::OnceClosure remove_listener;

  if (details.is_lazy) {
    // This is a removed lazy listener. This happens when an extension uses
    // removeListener() in its lazy context to forceably remove a listener
    // registration (as opposed to when the context is torn down, in which case
    // it's the active listener registration that's removed).
    // Due to https://crbug.com/1347597, we only have a single lazy listener
    // registration shared for both the on- and off-the-record contexts, so we
    // use the original context (associated with this KeyedService) to remove
    // the listener from both contexts.
    // Note that we unwrap the raw_ptr BrowserContext instance using
    // raw_ptr::get() so we truly have a raw pointer to bind into the callback.
    remove_listener = base::BindOnce(
        &WebRequestAPI::RemoveLazyListener, weak_factory_.GetWeakPtr(),
        browser_context_.get(), details.extension_id, sub_event_name);
  } else {
    // This was an active listener registration.
    auto update_type = WebRequestEventRouter::ListenerUpdateType::kRemove;
    if (details.service_worker_version_id !=
        blink::mojom::kInvalidServiceWorkerVersionId) {
      // This was a listener removed for a service worker, but it wasn't the
      // lazy listener registration. In this case, we only deactivate the
      // listener (rather than removing it).
      update_type = WebRequestEventRouter::ListenerUpdateType::kDeactivate;
    }

    // Note that we unwrap the raw_ptr BrowserContext instance using
    // raw_ptr::get() so we truly have a raw pointer to bind into the callback.
    remove_listener = base::BindOnce(
        &WebRequestAPI::UpdateActiveListener, weak_factory_.GetWeakPtr(),
        base::UnsafeDanglingUntriaged(details.browser_context.get()),
        update_type, details.extension_id, sub_event_name,
        details.worker_thread_id, details.service_worker_version_id);
  }

  // This PostTask is necessary even though we are already on the UI thread to
  // allow cases where blocking listeners remove themselves inside the handler.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(remove_listener));
}

bool WebRequestAPI::MaybeProxyURLLoaderFactory(
    content::BrowserContext* browser_context,
    content::RenderFrameHost* frame,
    int render_process_id,
    URLLoaderFactoryType type,
    std::optional<int64_t> navigation_id,
    ukm::SourceIdObj ukm_source_id,
    network::URLLoaderFactoryBuilder& factory_builder,
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
        header_client,
    scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner,
    const url::Origin& request_initiator) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!MayHaveProxies()) {
    bool use_proxy = false;

#if BUILDFLAG(ENABLE_GUEST_VIEW)
    // There are a few internal WebUIs that use WebView tag that are allowlisted
    // for webRequest.
    // TODO(crbug.com/40288053): Remove the scheme check once we're sure
    // that WebUIs with WebView run in real WebUI processes and check the
    // context type using |IsAvailableToWebViewEmbedderFrame()| below.
    if (WebViewGuest::IsGuest(frame)) {
      content::RenderFrameHost* embedder =
          frame->GetOutermostMainFrameOrEmbedder();
      const auto& embedder_url = embedder->GetLastCommittedURL();
      if (embedder_url.SchemeIs(content::kChromeUIScheme)) {
        auto* feature = FeatureProvider::GetAPIFeature("webRequestInternal");
        if (feature
                ->IsAvailableToContext(
                    nullptr, mojom::ContextType::kWebUi, embedder_url,
                    util::GetBrowserContextId(browser_context),
                    BrowserFrameContextData(frame))
                .is_available()) {
          use_proxy = true;
        }
      } else {
        use_proxy = IsAvailableToWebViewEmbedderFrame(frame);
      }
    }
#endif

    // Create a proxy URLLoader even when there is no CRX
    // installed with webRequest permissions. This allows the extension
    // requests to be intercepted for CRX telemetry service if enabled.
    // Only proxy if the new RHC interception logic is disabled.
    // TODO(crbug.com/40913716): Clean up collection logic here once new RHC
    // interception logic is fully launched.
    const std::string& request_scheme = request_initiator.scheme();
    if (extensions::kExtensionScheme == request_scheme &&
        ExtensionsBrowserClient::Get()->IsExtensionTelemetryServiceEnabled(
            browser_context) &&
        base::FeatureList::IsEnabled(
            safe_browsing::kExtensionTelemetryReportContactedHosts) &&
        !base::FeatureList::IsEnabled(
            safe_browsing::
                kExtensionTelemetryInterceptRemoteHostsContactedInRenderer)) {
      use_proxy = true;
    }
    if (!use_proxy) {
      return false;
    }
  }

  std::unique_ptr<ExtensionNavigationUIData> navigation_ui_data;
  const bool is_navigation = (type == URLLoaderFactoryType::kNavigation);
  if (is_navigation) {
    DCHECK(frame);
    DCHECK(navigation_id);
    int tab_id;
    int window_id;
    ExtensionsBrowserClient::Get()->GetTabAndWindowIdForWebContents(
        content::WebContents::FromRenderFrameHost(frame), &tab_id, &window_id);
    navigation_ui_data =
        std::make_unique<ExtensionNavigationUIData>(frame, tab_id, window_id);
  }

  mojo::PendingReceiver<network::mojom::TrustedURLLoaderHeaderClient>
      header_client_receiver;
  if (header_client) {
    header_client_receiver = header_client->InitWithNewPipeAndPassReceiver();
  }

  // NOTE: This request may be proxied on behalf of an incognito frame, but
  // |this| will always be bound to a regular profile (see
  // |BrowserContextKeyedAPI::kServiceRedirectedInIncognito|).
  DCHECK(browser_context == browser_context_ ||
         (browser_context->IsOffTheRecord() &&
          ExtensionsBrowserClient::Get()->GetOriginalContext(browser_context) ==
              browser_context_));
  WebRequestProxyingURLLoaderFactory::StartProxying(
      browser_context, is_navigation ? -1 : render_process_id,
      frame ? frame->GetRoutingID() : MSG_ROUTING_NONE,
      frame ? frame->GetRenderViewHost()->GetRoutingID() : MSG_ROUTING_NONE,
      &request_id_generator_, std::move(navigation_ui_data),
      std::move(navigation_id), ukm_source_id, factory_builder,
      std::move(header_client_receiver), proxies_.get(), type,
      std::move(navigation_response_task_runner));
  return true;
}

bool WebRequestAPI::MaybeProxyAuthRequest(
    content::BrowserContext* browser_context,
    const net::AuthChallengeInfo& auth_info,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    const content::GlobalRequestID& request_id,
    bool is_request_for_navigation,
    AuthRequestCallback callback,
    WebViewGuest* web_view_guest) {
  if (!MayHaveProxies()) {
    bool needed_for_webview = false;
#if BUILDFLAG(ENABLE_GUEST_VIEW)
    needed_for_webview =
        web_view_guest &&
        IsAvailableToWebViewEmbedderFrame(web_view_guest->GetGuestMainFrame());
#endif
    if (!needed_for_webview) {
      return false;
    }
  }

  content::GlobalRequestID proxied_request_id = request_id;
  // In MaybeProxyURLLoaderFactory, we use -1 as render_process_id for
  // navigation requests. Applying the same logic here so that we can correctly
  // identify the request.
  if (is_request_for_navigation) {
    proxied_request_id.child_id = -1;
  }

  // NOTE: This request may be proxied on behalf of an incognito frame, but
  // |this| will always be bound to a regular profile (see
  // |BrowserContextKeyedAPI::kServiceRedirectedInIncognito|).
  DCHECK(browser_context == browser_context_ ||
         (browser_context->IsOffTheRecord() &&
          ExtensionsBrowserClient::Get()->GetOriginalContext(browser_context) ==
              browser_context_));
  proxies_->MaybeProxyAuthRequest(auth_info, std::move(response_headers),
                                  proxied_request_id, std::move(callback));
  return true;
}

void WebRequestAPI::ProxyWebSocket(
    content::RenderFrameHost* frame,
    content::ContentBrowserClient::WebSocketFactory factory,
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const std::optional<std::string>& user_agent,
    mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
        handshake_client) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(MayHaveProxies() || MayHaveWebsocketProxiesForExtensionTelemetry() ||
         IsAvailableToWebViewEmbedderFrame(frame));

  content::BrowserContext* browser_context =
      frame->GetProcess()->GetBrowserContext();
  const bool has_extra_headers =
      WebRequestEventRouter::Get(browser_context)
          ->HasAnyExtraHeadersListener(browser_context);

  WebRequestProxyingWebSocket::StartProxying(
      std::move(factory), url, site_for_cookies, user_agent,
      std::move(handshake_client), has_extra_headers,
      frame->GetProcess()->GetID(), frame->GetRoutingID(),
      &request_id_generator_, frame->GetLastCommittedOrigin(),
      frame->GetProcess()->GetBrowserContext(), proxies_.get());
}

void WebRequestAPI::ProxyWebTransport(
    content::RenderProcessHost& render_process_host,
    int frame_routing_id,
    const GURL& url,
    const url::Origin& initiator_origin,
    mojo::PendingRemote<network::mojom::WebTransportHandshakeClient>
        handshake_client,
    content::ContentBrowserClient::WillCreateWebTransportCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!MayHaveProxies()) {
    auto* render_frame_host = content::RenderFrameHost::FromID(
        render_process_host.GetID(), frame_routing_id);
    if (!IsAvailableToWebViewEmbedderFrame(render_frame_host)) {
      std::move(callback).Run(std::move(handshake_client), std::nullopt);
      return;
    }
  }
  DCHECK(proxies_);
  StartWebRequestProxyingWebTransport(
      render_process_host, frame_routing_id, url, initiator_origin,
      std::move(handshake_client),
      request_id_generator_.Generate(MSG_ROUTING_NONE, 0), *proxies_.get(),
      std::move(callback));
}

void WebRequestAPI::ForceProxyForTesting() {
  ++web_request_extension_count_;
  UpdateMayHaveProxies();
}

bool WebRequestAPI::MayHaveProxies() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (base::FeatureList::IsEnabled(
          extensions_features::kForceWebRequestProxyForTest)) {
    return true;
  }

  return web_request_extension_count_ > 0;
}

bool WebRequestAPI::MayHaveWebsocketProxiesForExtensionTelemetry() const {
  // TODO(crbug.com/40913716): Clean up once new RHC interception logic is fully
  // launched.
  return ExtensionsBrowserClient::Get()->IsExtensionTelemetryServiceEnabled(
             browser_context_) &&
         base::FeatureList::IsEnabled(
             safe_browsing::kExtensionTelemetryReportContactedHosts) &&
         base::FeatureList::IsEnabled(
             safe_browsing::
                 kExtensionTelemetryReportHostsContactedViaWebSocket) &&
         !base::FeatureList::IsEnabled(
             safe_browsing::
                 kExtensionTelemetryInterceptRemoteHostsContactedInRenderer);
}

bool WebRequestAPI::IsAvailableToWebViewEmbedderFrame(
    content::RenderFrameHost* render_frame_host) const {
#if BUILDFLAG(ENABLE_GUEST_VIEW)
  if (!render_frame_host || !WebViewGuest::IsGuest(render_frame_host)) {
    return false;
  }

  content::BrowserContext* browser_context =
      render_frame_host->GetBrowserContext();
  content::RenderFrameHost* embedder_frame =
      render_frame_host->GetOutermostMainFrameOrEmbedder();

  if (!ProcessMap::Get(browser_context)
           ->CanProcessHostContextType(/*extension=*/nullptr,
                                       *embedder_frame->GetProcess(),
                                       mojom::ContextType::kWebPage)) {
    return false;
  }

  Feature::Availability availability =
      ExtensionAPI::GetSharedInstance()->IsAvailable(
          "webRequestInternal", /*extension=*/nullptr,
          mojom::ContextType::kWebPage, embedder_frame->GetLastCommittedURL(),
          CheckAliasStatus::ALLOWED, util::GetBrowserContextId(browser_context),
          BrowserFrameContextData(embedder_frame));
  return availability.is_available();
#else
  return false;
#endif
}

bool WebRequestAPI::HasExtraHeadersListenerForTesting() {
  return WebRequestEventRouter::Get(browser_context_)
      ->HasAnyExtraHeadersListener(browser_context_);
}

void WebRequestAPI::UpdateMayHaveProxies() {
  bool may_have_proxies = MayHaveProxies();
  if (!may_have_proxies_ && may_have_proxies) {
    browser_context_->GetDefaultStoragePartition()->ResetURLLoaderFactories();
  }
  may_have_proxies_ = may_have_proxies;
}

void WebRequestAPI::OnExtensionLoaded(content::BrowserContext* browser_context,
                                      const Extension* extension) {
  if (HasAnyWebRequestPermissions(extension)) {
    ++web_request_extension_count_;
    UpdateMayHaveProxies();
  }
}

void WebRequestAPI::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  if (HasAnyWebRequestPermissions(extension)) {
    --web_request_extension_count_;
    UpdateMayHaveProxies();
  }

  if (declarative_net_request::HasAnyDNRPermission(*extension)) {
    proxies_->OnDNRExtensionUnloaded(extension);
  }
}

void WebRequestAPI::UpdateActiveListener(
    void* browser_context_id,
    WebRequestEventRouter::ListenerUpdateType update_type,
    const ExtensionId& extension_id,
    const std::string& sub_event_name,
    int worker_thread_id,
    int64_t service_worker_version_id) {
  if (!ExtensionsBrowserClient::Get()->IsValidContext(browser_context_id)) {
    return;
  }

  content::BrowserContext* browser_context =
      reinterpret_cast<content::BrowserContext*>(browser_context_id);
  WebRequestEventRouter::Get(browser_context)
      ->UpdateActiveListener(browser_context, update_type, extension_id,
                             sub_event_name, worker_thread_id,
                             service_worker_version_id);
}

void WebRequestAPI::RemoveLazyListener(content::BrowserContext* browser_context,
                                       const ExtensionId& extension_id,
                                       const std::string& sub_event_name) {
  if (!ExtensionsBrowserClient::Get()->IsValidContext(browser_context)) {
    return;
  }
  WebRequestEventRouter::Get(browser_context)
      ->RemoveLazyListener(browser_context, extension_id, sub_event_name);
}

// Special QuotaLimitHeuristic for WebRequestHandlerBehaviorChangedFunction.
//
// Each call of webRequest.handlerBehaviorChanged() clears the in-memory cache
// of WebKit at the time of the next page load (top level navigation event).
// This quota heuristic is intended to limit the number of times the cache is
// cleared by an extension.
//
// As we want to account for the number of times the cache is really cleared
// (opposed to the number of times webRequest.handlerBehaviorChanged() is
// called), we cannot decide whether a call of
// webRequest.handlerBehaviorChanged() should trigger a quota violation at the
// time it is called. Instead we only decrement the bucket counter at the time
// when the cache is cleared (when page loads happen).
class ClearCacheQuotaHeuristic : public QuotaLimitHeuristic {
 public:
  ClearCacheQuotaHeuristic(const Config& config,
                           std::unique_ptr<BucketMapper> map)
      : QuotaLimitHeuristic(
            config,
            std::move(map),
            "MAX_HANDLER_BEHAVIOR_CHANGED_CALLS_PER_10_MINUTES"),
        callback_registered_(false) {}

  ClearCacheQuotaHeuristic(const ClearCacheQuotaHeuristic&) = delete;
  ClearCacheQuotaHeuristic& operator=(const ClearCacheQuotaHeuristic&) = delete;

  ~ClearCacheQuotaHeuristic() override {}
  bool Apply(Bucket* bucket, const base::TimeTicks& event_time) override;

 private:
  // Callback that is triggered by the WebRequestEventRouter on a page load.
  //
  // We don't need to take care of the life time of |bucket|: It is owned by the
  // BucketMapper of our base class in |QuotaLimitHeuristic::bucket_mapper_|. As
  // long as |this| exists, the respective BucketMapper and its bucket will
  // exist as well.
  void OnPageLoad(Bucket* bucket);

  // Flag to prevent that we register more than one call back in-between
  // clearing the cache.
  bool callback_registered_;

  base::WeakPtrFactory<ClearCacheQuotaHeuristic> weak_ptr_factory_{this};
};

bool ClearCacheQuotaHeuristic::Apply(Bucket* bucket,
                                     const base::TimeTicks& event_time) {
  if (event_time > bucket->expiration()) {
    bucket->Reset(config(), event_time);
  }

  // Call bucket->DeductToken() on a new page load, this is when
  // webRequest.handlerBehaviorChanged() clears the cache.
  if (!callback_registered_) {
    WebRequestEventRouter::AddCallbackForPageLoad(
        base::BindOnce(&ClearCacheQuotaHeuristic::OnPageLoad,
                       weak_ptr_factory_.GetWeakPtr(), bucket));
    callback_registered_ = true;
  }

  // We only check whether tokens are left here. Deducting a token happens in
  // OnPageLoad().
  return bucket->has_tokens();
}

void ClearCacheQuotaHeuristic::OnPageLoad(Bucket* bucket) {
  callback_registered_ = false;
  bucket->DeductToken();
}

ExtensionFunction::ResponseAction
WebRequestInternalAddEventListenerFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() == 6);

  // Argument 0 is the callback, which we don't use here.
  WebRequestEventRouter::RequestFilter filter;
  EXTENSION_FUNCTION_VALIDATE(args()[1].is_dict());
  // Failure + an empty error string means a fatal error.
  std::string error;
  EXTENSION_FUNCTION_VALIDATE(
      filter.InitFromValue(args()[1].GetDict(), &error) || !error.empty());
  if (!error.empty()) {
    return RespondNow(Error(std::move(error)));
  }

  int extra_info_spec = 0;
  if (HasOptionalArgument(2)) {
    EXTENSION_FUNCTION_VALIDATE(ExtraInfoSpec::InitFromValue(
        browser_context(), args()[2], &extra_info_spec));
  }

  const auto& event_name_value = args()[3];
  const auto& sub_event_name_value = args()[4];
  const auto& web_view_instance_id_value = args()[5];
  EXTENSION_FUNCTION_VALIDATE(event_name_value.is_string());
  EXTENSION_FUNCTION_VALIDATE(sub_event_name_value.is_string());
  EXTENSION_FUNCTION_VALIDATE(web_view_instance_id_value.is_int());
  std::string event_name = event_name_value.GetString();
  std::string sub_event_name = sub_event_name_value.GetString();
  int web_view_instance_id = web_view_instance_id_value.GetInt();

  int render_process_id = source_process_id();

  const Extension* extension = ExtensionRegistry::Get(browser_context())
                                   ->enabled_extensions()
                                   .GetByID(extension_id_safe());
  std::string extension_name =
      extension ? extension->name() : extension_id_safe();

  if (web_view_instance_id) {
    // If a web view ID has been supplied and the call is from an extension
    // (i.e. not from WebUI), we require the extension to have the webview
    // permission.
    if (extension && !extension->permissions_data()->HasAPIPermission(
                         mojom::APIPermissionID::kWebView)) {
      return RespondNow(Error("Missing webview permission."));
    }
  } else {
    auto has_blocking_permission = [&extension, &event_name]() {
      if (extension->permissions_data()->HasAPIPermission(
              APIPermissionID::kWebRequestBlocking)) {
        return true;
      }

      return event_name == keys::kOnAuthRequiredEvent &&
             extension->permissions_data()->HasAPIPermission(
                 APIPermissionID::kWebRequestAuthProvider);
    };

    // We check automatically whether the extension has the 'webRequest'
    // permission. For blocking calls we require the additional permission
    // 'webRequestBlocking' or 'webRequestAuthProvider'.
    bool is_blocking = extra_info_spec & (ExtraInfoSpec::BLOCKING |
                                          ExtraInfoSpec::ASYNC_BLOCKING);
    if (is_blocking && !has_blocking_permission()) {
      return RespondNow(Error(keys::kBlockingPermissionRequired));
    }

    // We allow to subscribe to patterns that are broader than the host
    // permissions. E.g., we could subscribe to http://www.example.com/*
    // while having host permissions for http://www.example.com/foo/* and
    // http://www.example.com/bar/*.
    // For this reason we do only a coarse check here to warn the extension
    // developer if they do something obviously wrong.
    if (extension->permissions_data()
            ->GetEffectiveHostPermissions()
            .is_empty() &&
        extension->permissions_data()
            ->withheld_permissions()
            .explicit_hosts()
            .is_empty()) {
      return RespondNow(Error(keys::kHostPermissionsRequired));
    }
  }

  bool success =
      WebRequestEventRouter::Get(browser_context())
          ->AddEventListener(browser_context(), extension_id_safe(),
                             extension_name, event_name, sub_event_name,
                             std::move(filter), extra_info_spec,
                             render_process_id, web_view_instance_id,
                             worker_thread_id(), service_worker_version_id());
  EXTENSION_FUNCTION_VALIDATE(success);

  helpers::ClearCacheOnNavigation();

  return RespondNow(NoArguments());
}

void WebRequestInternalEventHandledFunction::OnError(
    const std::string& event_name,
    const std::string& sub_event_name,
    uint64_t request_id,
    int render_process_id,
    int web_view_instance_id,
    std::unique_ptr<WebRequestEventRouter::EventResponse> response) {
  WebRequestEventRouter::Get(browser_context())
      ->OnEventHandled(browser_context(), extension_id_safe(), event_name,
                       sub_event_name, request_id, render_process_id,
                       web_view_instance_id, worker_thread_id(),
                       service_worker_version_id(), std::move(response));
}

ExtensionFunction::ResponseAction
WebRequestInternalEventHandledFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 5);
  const auto& event_name_value = args()[0];
  const auto& sub_event_name_value = args()[1];
  const auto& request_id_str_value = args()[2];
  const auto& web_view_instance_id_value = args()[3];
  EXTENSION_FUNCTION_VALIDATE(event_name_value.is_string());
  EXTENSION_FUNCTION_VALIDATE(sub_event_name_value.is_string());
  EXTENSION_FUNCTION_VALIDATE(request_id_str_value.is_string());
  EXTENSION_FUNCTION_VALIDATE(web_view_instance_id_value.is_int());
  std::string event_name = event_name_value.GetString();
  std::string sub_event_name = sub_event_name_value.GetString();
  std::string request_id_str = request_id_str_value.GetString();
  int web_view_instance_id = web_view_instance_id_value.GetInt();

  uint64_t request_id;
  EXTENSION_FUNCTION_VALIDATE(
      base::StringToUint64(request_id_str, &request_id));

  int render_process_id = source_process_id();

  std::unique_ptr<WebRequestEventRouter::EventResponse> response;
  if (HasOptionalArgument(4)) {
    EXTENSION_FUNCTION_VALIDATE(args()[4].is_dict());
    const base::Value::Dict& dict_value = args()[4].GetDict();

    if (!dict_value.empty()) {
      base::Time install_time = ExtensionPrefs::Get(browser_context())
                                    ->GetLastUpdateTime(extension_id_safe());
      response = std::make_unique<WebRequestEventRouter::EventResponse>(
          extension_id_safe(), install_time);
    }

    const base::Value* redirect_url_value = dict_value.Find("redirectUrl");
    const base::Value* auth_credentials_value =
        dict_value.Find(keys::kAuthCredentialsKey);
    const base::Value* request_headers_value =
        dict_value.Find("requestHeaders");
    const base::Value* response_headers_value =
        dict_value.Find("responseHeaders");

    const base::Value* cancel_value = dict_value.Find("cancel");
    if (cancel_value) {
      // Don't allow cancel mixed with other keys.
      if (dict_value.size() != 1) {
        OnError(event_name, sub_event_name, request_id, render_process_id,
                web_view_instance_id, std::move(response));
        return RespondNow(Error(keys::kInvalidBlockingResponse));
      }

      EXTENSION_FUNCTION_VALIDATE(cancel_value->is_bool());
      response->cancel = cancel_value->GetBool();
    }

    if (redirect_url_value) {
      EXTENSION_FUNCTION_VALIDATE(redirect_url_value->is_string());
      std::string new_url_str = redirect_url_value->GetString();
      response->new_url = GURL(new_url_str);
      if (!response->new_url.is_valid()) {
        OnError(event_name, sub_event_name, request_id, render_process_id,
                web_view_instance_id, std::move(response));
        return RespondNow(Error(keys::kInvalidRedirectUrl, new_url_str));
      }
    }

    const bool has_request_headers = request_headers_value != nullptr;
    const bool has_response_headers = response_headers_value != nullptr;
    if (has_request_headers || has_response_headers) {
      if (has_request_headers && has_response_headers) {
        // Allow only one of the keys, not both.
        OnError(event_name, sub_event_name, request_id, render_process_id,
                web_view_instance_id, std::move(response));
        return RespondNow(Error(keys::kInvalidHeaderKeyCombination));
      }

      const base::Value::List* headers_value = nullptr;
      std::unique_ptr<net::HttpRequestHeaders> request_headers;
      std::unique_ptr<helpers::ResponseHeaders> response_headers;
      if (has_request_headers) {
        request_headers = std::make_unique<net::HttpRequestHeaders>();
        headers_value = dict_value.FindList(keys::kRequestHeadersKey);
      } else {
        response_headers = std::make_unique<helpers::ResponseHeaders>();
        headers_value = dict_value.FindList(keys::kResponseHeadersKey);
      }
      EXTENSION_FUNCTION_VALIDATE(headers_value);

      for (const base::Value& elem : *headers_value) {
        EXTENSION_FUNCTION_VALIDATE(elem.is_dict());
        const base::Value::Dict& header_value = elem.GetDict();
        std::string name;
        std::string value;
        if (!FromHeaderDictionary(header_value, &name, &value)) {
          std::string serialized_header;
          base::JSONWriter::Write(header_value, &serialized_header);
          OnError(event_name, sub_event_name, request_id, render_process_id,
                  web_view_instance_id, std::move(response));
          return RespondNow(Error(keys::kInvalidHeader, serialized_header));
        }
        if (!net::HttpUtil::IsValidHeaderName(name)) {
          OnError(event_name, sub_event_name, request_id, render_process_id,
                  web_view_instance_id, std::move(response));
          return RespondNow(Error(keys::kInvalidHeaderName));
        }
        if (!net::HttpUtil::IsValidHeaderValue(value)) {
          OnError(event_name, sub_event_name, request_id, render_process_id,
                  web_view_instance_id, std::move(response));
          return RespondNow(Error(keys::kInvalidHeaderValue, name));
        }
        if (has_request_headers) {
          request_headers->SetHeader(name, value);
        } else {
          response_headers->push_back(helpers::ResponseHeader(name, value));
        }
      }
      if (has_request_headers) {
        response->request_headers = std::move(request_headers);
      } else {
        response->response_headers = std::move(response_headers);
      }
    }

    if (auth_credentials_value) {
      const base::Value::Dict* credentials_value =
          auth_credentials_value->GetIfDict();
      EXTENSION_FUNCTION_VALIDATE(credentials_value);
      const std::string* username =
          credentials_value->FindString(keys::kUsernameKey);
      const std::string* password =
          credentials_value->FindString(keys::kPasswordKey);
      EXTENSION_FUNCTION_VALIDATE(username);
      EXTENSION_FUNCTION_VALIDATE(password);
      response->auth_credentials = net::AuthCredentials(
          base::UTF8ToUTF16(*username), base::UTF8ToUTF16(*password));
    }
  }

  WebRequestEventRouter::Get(browser_context())
      ->OnEventHandled(browser_context(), extension_id_safe(), event_name,
                       sub_event_name, request_id, render_process_id,
                       web_view_instance_id, worker_thread_id(),
                       service_worker_version_id(), std::move(response));

  return RespondNow(NoArguments());
}

void WebRequestHandlerBehaviorChangedFunction::GetQuotaLimitHeuristics(
    QuotaLimitHeuristics* heuristics) const {
  QuotaLimitHeuristic::Config config = {
      // See web_request.json for current value.
      web_request::MAX_HANDLER_BEHAVIOR_CHANGED_CALLS_PER_10_MINUTES,
      base::Minutes(10)};
  heuristics->push_back(std::make_unique<ClearCacheQuotaHeuristic>(
      config, std::make_unique<QuotaLimitHeuristic::SingletonBucketMapper>()));
}

void WebRequestHandlerBehaviorChangedFunction::OnQuotaExceeded(
    std::string violation_error) {
  // Post warning message.
  WarningSet warnings;
  warnings.insert(
      Warning::CreateRepeatedCacheFlushesWarning(extension_id_safe()));
  WarningService::NotifyWarningsOnUI(browser_context(), warnings);

  // Continue gracefully.
  RunWithValidation().Execute();
}

ExtensionFunction::ResponseAction
WebRequestHandlerBehaviorChangedFunction::Run() {
  helpers::ClearCacheOnNavigation();
  return RespondNow(NoArguments());
}

}  // namespace extensions
