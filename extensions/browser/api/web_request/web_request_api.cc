// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_api.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/resource_type.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/api/activity_log/web_request_constants.h"
#include "extensions/browser/api/declarative/rules_registry_service.h"
#include "extensions/browser/api/declarative_net_request/ruleset_manager.h"
#include "extensions/browser/api/declarative_webrequest/request_stage.h"
#include "extensions/browser/api/declarative_webrequest/webrequest_constants.h"
#include "extensions/browser/api/declarative_webrequest/webrequest_rules_registry.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/web_request/web_request_api_constants.h"
#include "extensions/browser/api/web_request/web_request_api_helpers.h"
#include "extensions/browser/api/web_request/web_request_event_details.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/browser/api/web_request/web_request_proxying_url_loader_factory.h"
#include "extensions/browser/api/web_request/web_request_proxying_websocket.h"
#include "extensions/browser/api/web_request/web_request_resource_type.h"
#include "extensions/browser/api/web_request/web_request_time_tracker.h"
#include "extensions/browser/api_activity_monitor.h"
#include "extensions/browser/device_local_account_util.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_navigation_ui_data.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/guest_view/guest_view_events.h"
#include "extensions/browser/guest_view/web_view/web_view_constants.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/info_map.h"
#include "extensions/browser/io_thread_extension_message_filter.h"
#include "extensions/browser/runtime_data.h"
#include "extensions/browser/warning_service.h"
#include "extensions/browser/warning_set.h"
#include "extensions/common/api/web_request.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/event_filtering_info.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "net/base/auth.h"
#include "net/base/net_errors.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/features.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chromeos/login/login_state.h"
#endif  // defined(OS_CHROMEOS)

using content::BrowserThread;
using content::ResourceRequestInfo;
using extension_web_request_api_helpers::ExtraInfoSpec;

namespace activity_log = activity_log_web_request_constants;
namespace helpers = extension_web_request_api_helpers;
namespace keys = extension_web_request_api_constants;

namespace extensions {

namespace declarative_keys = declarative_webrequest_constants;
namespace web_request = api::web_request;

namespace {

// Describes the action taken by the Web Request API for a given stage of a web
// request.
// These values are written to logs.  New enum values can be added, but existing
// enum values must never be renumbered or deleted and reused.
enum RequestAction {
  CANCEL = 0,
  REDIRECT = 1,
  MODIFY_REQUEST_HEADERS = 2,
  MODIFY_RESPONSE_HEADERS = 3,
  SET_AUTH_CREDENTIALS = 4,
  MAX
};

// Corresponds to the "WebRequestEventResponse" histogram enumeration type in
// src/tools/metrics/histograms/enums.xml.
//
// DO NOT REORDER OR CHANGE THE MEANING OF THESE VALUES.
enum class WebRequestEventResponse {
  kIgnored,
  kObserved,
  kMaxValue = kObserved,
};

const char kWebRequestEventPrefix[] = "webRequest.";

// List of all the webRequest events. Note: this doesn't include
// "onActionIgnored" which is not related to a request's lifecycle and is
// handled as a normal event (as opposed to a WebRequestEvent at the bindings
// layer).
const char* const kWebRequestEvents[] = {
    keys::kOnBeforeRedirectEvent,
    web_request::OnBeforeRequest::kEventName,
    keys::kOnBeforeSendHeadersEvent,
    keys::kOnCompletedEvent,
    web_request::OnErrorOccurred::kEventName,
    keys::kOnSendHeadersEvent,
    keys::kOnAuthRequiredEvent,
    keys::kOnResponseStartedEvent,
    keys::kOnHeadersReceivedEvent,
};

// User data key for WebRequestAPI::ProxySet.
const void* const kWebRequestProxySetUserDataKey =
    &kWebRequestProxySetUserDataKey;

const char* GetRequestStageAsString(
    ExtensionWebRequestEventRouter::EventTypes type) {
  switch (type) {
    case ExtensionWebRequestEventRouter::kInvalidEvent:
      return "Invalid";
    case ExtensionWebRequestEventRouter::kOnBeforeRequest:
      return keys::kOnBeforeRequest;
    case ExtensionWebRequestEventRouter::kOnBeforeSendHeaders:
      return keys::kOnBeforeSendHeaders;
    case ExtensionWebRequestEventRouter::kOnSendHeaders:
      return keys::kOnSendHeaders;
    case ExtensionWebRequestEventRouter::kOnHeadersReceived:
      return keys::kOnHeadersReceived;
    case ExtensionWebRequestEventRouter::kOnBeforeRedirect:
      return keys::kOnBeforeRedirect;
    case ExtensionWebRequestEventRouter::kOnAuthRequired:
      return keys::kOnAuthRequired;
    case ExtensionWebRequestEventRouter::kOnResponseStarted:
      return keys::kOnResponseStarted;
    case ExtensionWebRequestEventRouter::kOnErrorOccurred:
      return keys::kOnErrorOccurred;
    case ExtensionWebRequestEventRouter::kOnCompleted:
      return keys::kOnCompleted;
  }
  NOTREACHED();
  return "Not reached";
}

void LogRequestAction(RequestAction action) {
  DCHECK_NE(RequestAction::MAX, action);
  UMA_HISTOGRAM_ENUMERATION("Extensions.WebRequestAction", action,
                            RequestAction::MAX);
}

bool IsWebRequestEvent(const std::string& event_name) {
  std::string web_request_event_name(event_name);
  if (base::StartsWith(web_request_event_name,
                       webview::kWebViewEventPrefix,
                       base::CompareCase::SENSITIVE)) {
    web_request_event_name.replace(
        0, strlen(webview::kWebViewEventPrefix), kWebRequestEventPrefix);
  }
  auto* const* web_request_events_end =
      kWebRequestEvents + arraysize(kWebRequestEvents);
  return std::find(kWebRequestEvents, web_request_events_end,
                   web_request_event_name) != web_request_events_end;
}

// Returns whether |request| has been triggered by an extension in
// |extension_info_map|.
bool IsRequestFromExtension(const WebRequestInfo& request,
                            const InfoMap* extension_info_map) {
  if (request.render_process_id == -1)
    return false;

  // |extension_info_map| is NULL for system-level requests.
  if (!extension_info_map)
    return false;

  const std::set<std::string> extension_ids =
      extension_info_map->process_map().GetExtensionsInProcess(
          request.render_process_id);
  if (extension_ids.empty())
    return false;

  // Treat hosted apps as normal web pages (crbug.com/526413).
  for (const std::string& extension_id : extension_ids) {
    const Extension* extension =
        extension_info_map->extensions().GetByID(extension_id);
    if (extension && !extension->is_hosted_app())
      return true;
  }
  return false;
}

// Converts a HttpHeaders dictionary to a |name|, |value| pair. Returns
// true if successful.
bool FromHeaderDictionary(const base::DictionaryValue* header_value,
                          std::string* name,
                          std::string* value) {
  if (!header_value->GetString(keys::kHeaderNameKey, name))
    return false;

  // We require either a "value" or a "binaryValue" entry.
  if (!(header_value->HasKey(keys::kHeaderValueKey) ^
        header_value->HasKey(keys::kHeaderBinaryValueKey))) {
    return false;
  }

  if (header_value->HasKey(keys::kHeaderValueKey)) {
    if (!header_value->GetString(keys::kHeaderValueKey, value)) {
      return false;
    }
  } else if (header_value->HasKey(keys::kHeaderBinaryValueKey)) {
    const base::ListValue* list = NULL;
    if (!header_value->HasKey(keys::kHeaderBinaryValueKey)) {
      *value = "";
    } else if (!header_value->GetList(keys::kHeaderBinaryValueKey, &list) ||
               !helpers::CharListToString(list, value)) {
      return false;
    }
  }
  return true;
}

// Sends an event to subscribers of chrome.declarativeWebRequest.onMessage or
// to subscribers of webview.onMessage if the action is being operated upon
// a <webview> guest renderer.
// |extension_id| identifies the extension that sends and receives the event.
// |is_web_view_guest| indicates whether the action is for a <webview>.
// |web_view_instance_id| is a valid if |is_web_view_guest| is true.
// |event_details| is passed to the event listener.
void SendOnMessageEventOnUI(
    void* browser_context_id,
    const std::string& extension_id,
    bool is_web_view_guest,
    int web_view_instance_id,
    std::unique_ptr<WebRequestEventDetails> event_details) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content::BrowserContext* browser_context =
      reinterpret_cast<content::BrowserContext*>(browser_context_id);
  if (!ExtensionsBrowserClient::Get()->IsValidContext(browser_context))
    return;

  std::unique_ptr<base::ListValue> event_args(new base::ListValue);
  event_details->DetermineFrameDataOnUI();
  event_args->Append(event_details->GetAndClearDict());

  EventRouter* event_router = EventRouter::Get(browser_context);

  EventFilteringInfo event_filtering_info;

  events::HistogramValue histogram_value = events::UNKNOWN;
  std::string event_name;
  // The instance ID uniquely identifies a <webview> instance within an embedder
  // process. We use a filter here so that only event listeners for a particular
  // <webview> will fire.
  if (is_web_view_guest) {
    event_filtering_info.instance_id = web_view_instance_id;
    histogram_value = events::WEB_VIEW_INTERNAL_ON_MESSAGE;
    event_name = webview::kEventMessage;
  } else {
    histogram_value = events::DECLARATIVE_WEB_REQUEST_ON_MESSAGE;
    event_name = declarative_keys::kOnMessage;
  }

  std::unique_ptr<Event> event(new Event(
      histogram_value, event_name, std::move(event_args), browser_context,
      GURL(), EventRouter::USER_GESTURE_UNKNOWN, event_filtering_info));
  event_router->DispatchEventToExtension(extension_id, std::move(event));
}

// Helper to dispatch the "onActionIgnored" event.
void NotifyIgnoredActionsOnUI(
    void* browser_context_id,
    uint64_t request_id,
    extension_web_request_api_helpers::IgnoredActions ignored_actions) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content::BrowserContext* browser_context =
      reinterpret_cast<content::BrowserContext*>(browser_context_id);
  if (!ExtensionsBrowserClient::Get()->IsValidContext(browser_context))
    return;

  EventRouter* event_router = EventRouter::Get(browser_context);
  web_request::OnActionIgnored::Details details;
  details.request_id = base::NumberToString(request_id);
  details.action = web_request::IGNORED_ACTION_TYPE_NONE;
  for (const auto& ignored_action : ignored_actions) {
    DCHECK_NE(web_request::IGNORED_ACTION_TYPE_NONE,
              ignored_action.action_type);

    details.action = ignored_action.action_type;
    auto event = std::make_unique<Event>(
        events::WEB_REQUEST_ON_ACTION_IGNORED,
        web_request::OnActionIgnored::kEventName,
        web_request::OnActionIgnored::Create(details), browser_context);
    event_router->DispatchEventToExtension(ignored_action.extension_id,
                                           std::move(event));
  }
}

events::HistogramValue GetEventHistogramValue(const std::string& event_name) {
  // Event names will either be webRequest events, or guest view (probably web
  // view) events that map to webRequest events. Check webRequest first.
  static struct ValueAndName {
    events::HistogramValue histogram_value;
    const char* const event_name;
  } values_and_names[] = {
      {events::WEB_REQUEST_ON_BEFORE_REDIRECT, keys::kOnBeforeRedirectEvent},
      {events::WEB_REQUEST_ON_BEFORE_REQUEST,
       web_request::OnBeforeRequest::kEventName},
      {events::WEB_REQUEST_ON_BEFORE_SEND_HEADERS,
       keys::kOnBeforeSendHeadersEvent},
      {events::WEB_REQUEST_ON_COMPLETED, keys::kOnCompletedEvent},
      {events::WEB_REQUEST_ON_ERROR_OCCURRED,
       web_request::OnErrorOccurred::kEventName},
      {events::WEB_REQUEST_ON_SEND_HEADERS, keys::kOnSendHeadersEvent},
      {events::WEB_REQUEST_ON_AUTH_REQUIRED, keys::kOnAuthRequiredEvent},
      {events::WEB_REQUEST_ON_RESPONSE_STARTED, keys::kOnResponseStartedEvent},
      {events::WEB_REQUEST_ON_HEADERS_RECEIVED, keys::kOnHeadersReceivedEvent}};
  static_assert(arraysize(kWebRequestEvents) == arraysize(values_and_names),
                "kWebRequestEvents and values_and_names must be the same");
  for (const ValueAndName& value_and_name : values_and_names) {
    if (value_and_name.event_name == event_name)
      return value_and_name.histogram_value;
  }

  // If there is no webRequest event, it might be a guest view webRequest event.
  events::HistogramValue guest_view_histogram_value =
      guest_view_events::GetEventHistogramValue(event_name);
  if (guest_view_histogram_value != events::UNKNOWN)
    return guest_view_histogram_value;

  // There is no histogram value for this event name. It should be added to
  // either the mapping here, or in guest_view_events.
  NOTREACHED() << "Event " << event_name << " must have a histogram value";
  return events::UNKNOWN;
}

// We hide events from the system context as well as sensitive requests.
bool ShouldHideEvent(void* browser_context,
                     const InfoMap* extension_info_map,
                     const WebRequestInfo& request) {
  return (!browser_context ||
          WebRequestPermissions::HideRequest(extension_info_map, request));
}

// Returns true if we're in a Public Session and restrictions are enabled.
bool ArePublicSessionRestrictionsEnabled() {
#if defined(OS_CHROMEOS)
  if (chromeos::LoginState::IsInitialized()) {
    return chromeos::LoginState::Get()->ArePublicSessionRestrictionsEnabled();
  }
#endif
  return false;
}

// Returns event details for a given request.
std::unique_ptr<WebRequestEventDetails> CreateEventDetails(
    const WebRequestInfo& request,
    int extra_info_spec) {
  return std::make_unique<WebRequestEventDetails>(request, extra_info_spec);
}

void MaybeProxyAuthRequestOnIO(
    content::ResourceContext* resource_context,
    net::AuthChallengeInfo* auth_info,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    const content::GlobalRequestID& request_id,
    WebRequestAPI::AuthRequestCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  auto* proxies =
      WebRequestAPI::ProxySet::GetFromResourceContext(resource_context);
  proxies->MaybeProxyAuthRequest(auth_info, std::move(response_headers),
                                 request_id, std::move(callback));
}

// Checks whether the extension has any permissions that would use the web
// request API.
bool HasAnyWebRequestPermissions(const Extension* extension) {
  static const APIPermission::ID kWebRequestPermissions[] = {
      APIPermission::ID::kWebRequest,
      APIPermission::ID::kWebRequestBlocking,
      APIPermission::ID::kDeclarativeWebRequest,
      APIPermission::ID::kDeclarativeNetRequest,
      APIPermission::ID::kWebView,
  };

  const PermissionsData* permissions = extension->permissions_data();
  for (auto permission : kWebRequestPermissions) {
    if (permissions->HasAPIPermission(permission))
      return true;
  }
  return false;
}

}  // namespace

void WebRequestAPI::Proxy::HandleAuthRequest(
    net::AuthChallengeInfo* auth_info,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    int32_t request_id,
    AuthRequestCallback callback) {
  // Default implementation cancels the request.
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(std::move(callback), base::nullopt,
                                          false /* should_cancel */));
}

WebRequestAPI::ProxySet::ProxySet() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

WebRequestAPI::ProxySet::~ProxySet() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

WebRequestAPI::ProxySet* WebRequestAPI::ProxySet::GetFromResourceContext(
    content::ResourceContext* resource_context) {
  if (!resource_context->GetUserData(kWebRequestProxySetUserDataKey)) {
    resource_context->SetUserData(kWebRequestProxySetUserDataKey,
                                  std::make_unique<WebRequestAPI::ProxySet>());
  }
  return static_cast<WebRequestAPI::ProxySet*>(
      resource_context->GetUserData(kWebRequestProxySetUserDataKey));
}

void WebRequestAPI::ProxySet::AddProxy(std::unique_ptr<Proxy> proxy) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  proxies_.insert(std::move(proxy));
}

void WebRequestAPI::ProxySet::RemoveProxy(Proxy* proxy) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto requests_it = proxy_to_request_id_map_.find(proxy);
  if (requests_it != proxy_to_request_id_map_.end()) {
    for (const auto& id : requests_it->second)
      request_id_to_proxy_map_.erase(id);
    proxy_to_request_id_map_.erase(requests_it);
  }

  auto proxy_it = proxies_.find(proxy);
  DCHECK(proxy_it != proxies_.end());
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
  if (it == request_id_to_proxy_map_.end())
    return nullptr;
  return it->second;
}

void WebRequestAPI::ProxySet::MaybeProxyAuthRequest(
    net::AuthChallengeInfo* auth_info,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    const content::GlobalRequestID& request_id,
    AuthRequestCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  Proxy* proxy = GetProxyFromRequestId(request_id);
  if (!proxy) {
    // The request=>proxy map is maintained on the IO thread so it is not an
    // error to get here and have no proxy. In this situation run the |callback|
    // which will display a dialog for the user to enter their auth credentials.
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                             base::BindOnce(std::move(callback), base::nullopt,
                                            false /* should_cancel */));
    return;
  }

  proxy->HandleAuthRequest(auth_info, std::move(response_headers),
                           request_id.request_id, std::move(callback));
}

WebRequestAPI::WebRequestAPI(content::BrowserContext* context)
    : browser_context_(context),
      info_map_(ExtensionSystem::Get(browser_context_)->info_map()),
      request_id_generator_(base::MakeRefCounted<RequestIDGenerator>()),
      may_have_proxies_(MayHaveProxies()) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  for (size_t i = 0; i < arraysize(kWebRequestEvents); ++i) {
    // Observe the webRequest event.
    std::string event_name = kWebRequestEvents[i];
    event_router->RegisterObserver(this, event_name);

    // Also observe the corresponding webview event.
    event_name.replace(
        0, sizeof(kWebRequestEventPrefix) - 1, webview::kWebViewEventPrefix);
    event_router->RegisterObserver(this, event_name);
  }
  extensions::ExtensionRegistry::Get(browser_context_)->AddObserver(this);
}

WebRequestAPI::~WebRequestAPI() {
}

void WebRequestAPI::Shutdown() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
  extensions::ExtensionRegistry::Get(browser_context_)->RemoveObserver(this);
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
  // Note that details.event_name includes the sub-event details (e.g. "/123").
  // TODO(fsamuel): <webview> events will not be removed through this code path.
  // <webview> events will be removed in RemoveWebViewEventListeners. Ideally,
  // this code should be decoupled from extensions, we should use the host ID
  // instead, and not have two different code paths. This is a huge undertaking
  // unfortunately, so we'll resort to two code paths for now.
  //
  // Note that details.event_name is actually the sub_event_name!
  ExtensionWebRequestEventRouter::EventListener::ID id(
      details.browser_context, details.extension_id, details.event_name, 0, 0);

  // This Unretained is safe because the ExtensionWebRequestEventRouter
  // singleton is leaked.
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::Bind(
          &ExtensionWebRequestEventRouter::RemoveEventListener,
          base::Unretained(ExtensionWebRequestEventRouter::GetInstance()), id,
          false /* not strict */));
}

bool WebRequestAPI::MaybeProxyURLLoaderFactory(
    content::RenderFrameHost* frame,
    bool is_navigation,
    network::mojom::URLLoaderFactoryRequest* factory_request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!MayHaveProxies()) {
    bool skip_proxy = true;
    // There are a few internal WebUIs that use WebView tag that are whitelisted
    // for webRequest.
    auto* web_contents = content::WebContents::FromRenderFrameHost(frame);
    if (web_contents && WebViewGuest::IsGuest(web_contents)) {
      auto* guest_web_contents =
          WebViewGuest::GetTopLevelWebContents(web_contents);
      auto& guest_url = guest_web_contents->GetURL();
      if (guest_url.SchemeIs(content::kChromeUIScheme)) {
        auto* feature = FeatureProvider::GetAPIFeature("webRequestInternal");
        if (feature
                ->IsAvailableToContext(nullptr, Feature::WEBUI_CONTEXT,
                                       guest_url)
                .is_available()) {
          skip_proxy = false;
        }
      }
    }

    if (skip_proxy)
      return false;
  }

  auto proxied_request = std::move(*factory_request);
  network::mojom::URLLoaderFactoryPtrInfo target_factory_info;
  *factory_request = mojo::MakeRequest(&target_factory_info);

  std::unique_ptr<ExtensionNavigationUIData> navigation_ui_data;
  if (is_navigation) {
    int tab_id;
    int window_id;
    ExtensionsBrowserClient::Get()->GetTabAndWindowIdForWebContents(
        content::WebContents::FromRenderFrameHost(frame), &tab_id, &window_id);
    navigation_ui_data =
        std::make_unique<ExtensionNavigationUIData>(frame, tab_id, window_id);
  }

  // NOTE: This request may be proxied on behalf of an incognito frame, but
  // |this| will always be bound to a regular profile (see
  // |BrowserContextKeyedAPI::kServiceRedirectedInIncognito|). As such, we use
  // the frame's BrowserContext.
  auto* browser_context = frame->GetProcess()->GetBrowserContext();
  DCHECK(browser_context == browser_context_ ||
         (browser_context->IsOffTheRecord() &&
          ExtensionsBrowserClient::Get()->GetOriginalContext(browser_context) ==
              browser_context_));
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&WebRequestProxyingURLLoaderFactory::StartProxying,
                     browser_context, browser_context->GetResourceContext(),
                     // Match the behavior of the WebRequestInfo constructor
                     // which takes a net::URLRequest*.
                     is_navigation ? -1 : frame->GetProcess()->GetID(),
                     request_id_generator_, std::move(navigation_ui_data),
                     base::Unretained(info_map_), std::move(proxied_request),
                     std::move(target_factory_info)));
  return true;
}

bool WebRequestAPI::MaybeProxyAuthRequest(
    net::AuthChallengeInfo* auth_info,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    const content::GlobalRequestID& request_id,
    bool is_main_frame,
    AuthRequestCallback callback) {
  if (!MayHaveProxies())
    return false;

  content::GlobalRequestID proxied_request_id = request_id;
  if (is_main_frame)
    proxied_request_id.child_id = -1;
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&MaybeProxyAuthRequestOnIO,
                     browser_context_->GetResourceContext(),
                     base::RetainedRef(auth_info), std::move(response_headers),
                     proxied_request_id, std::move(callback)));
  return true;
}

void WebRequestAPI::MaybeProxyWebSocket(
    content::RenderFrameHost* frame,
    network::mojom::WebSocketRequest* request,
    network::mojom::AuthenticationHandlerPtr* auth_handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!MayHaveProxies())
    return;

  network::mojom::WebSocketPtrInfo proxied_socket_ptr_info;
  auto proxied_request = std::move(*request);
  *request = mojo::MakeRequest(&proxied_socket_ptr_info);
  auto authentication_request = mojo::MakeRequest(auth_handler);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &WebRequestProxyingWebSocket::StartProxying,
          frame->GetProcess()->GetID(), frame->GetRoutingID(),
          request_id_generator_, frame->GetLastCommittedOrigin(),
          frame->GetProcess()->GetBrowserContext(),
          frame->GetProcess()->GetBrowserContext()->GetResourceContext(),
          base::Unretained(info_map_), std::move(proxied_socket_ptr_info),
          std::move(proxied_request), std::move(authentication_request)));
}

void WebRequestAPI::ForceProxyForTesting() {
  ++web_request_extension_count_;
  UpdateMayHaveProxies();
}

bool WebRequestAPI::MayHaveProxies() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService))
    return false;

  return web_request_extension_count_ > 0;
}

void WebRequestAPI::UpdateMayHaveProxies() {
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService))
    return;

  bool may_have_proxies = MayHaveProxies();
  if (!may_have_proxies_ && may_have_proxies) {
    content::BrowserContext::GetDefaultStoragePartition(browser_context_)
        ->ResetURLLoaderFactories();
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
}

// Represents a single unique listener to an event, along with whatever filter
// parameters and extra_info_spec were specified at the time the listener was
// added.
// NOTE(benjhayden) New APIs should not use this sub_event_name trick! It does
// not play well with event pages. See downloads.onDeterminingFilename and
// ExtensionDownloadsEventRouter for an alternative approach.
ExtensionWebRequestEventRouter::EventListener::EventListener(ID id) : id(id) {}
ExtensionWebRequestEventRouter::EventListener::~EventListener() {}

// Contains info about requests that are blocked waiting for a response from
// an extension.
struct ExtensionWebRequestEventRouter::BlockedRequest {
  BlockedRequest() = default;

  // Information about the request that is being blocked. Not owned.
  const WebRequestInfo* request = nullptr;

  // Whether the request originates from an incognito tab.
  bool is_incognito = false;

  // The event that we're currently blocked on.
  EventTypes event = kInvalidEvent;

  // The number of event handlers that we are awaiting a response from.
  int num_handlers_blocking = 0;

  // The callback to call when we get a response from all event handlers.
  net::CompletionOnceCallback callback;

  // If non-empty, this contains the new URL that the request will redirect to.
  // Only valid for OnBeforeRequest and OnHeadersReceived.
  GURL* new_url = nullptr;

  // The request headers that will be issued along with this request. Only valid
  // for OnBeforeSendHeaders.
  net::HttpRequestHeaders* request_headers = nullptr;

  // The response headers that were received from the server. Only valid for
  // OnHeadersReceived.
  scoped_refptr<const net::HttpResponseHeaders> original_response_headers;

  // Location where to override response headers. Only valid for
  // OnHeadersReceived.
  scoped_refptr<net::HttpResponseHeaders>* override_response_headers = nullptr;

  // If non-empty, this contains the auth credentials that may be filled in.
  // Only valid for OnAuthRequired.
  net::AuthCredentials* auth_credentials = nullptr;

  // The callback to invoke for auth. If |auth_callback.is_null()| is false,
  // |callback| must be NULL.
  // Only valid for OnAuthRequired.
  net::NetworkDelegate::AuthCallback auth_callback;

  // Time the request was paused. Used for logging purposes.
  base::Time blocking_time;

  // Changes requested by extensions.
  helpers::EventResponseDeltas response_deltas;

  // Provider of meta data about extensions, only used and non-NULL for events
  // that are delayed until the rules registry is ready.
  const InfoMap* extension_info_map = nullptr;
};

bool ExtensionWebRequestEventRouter::RequestFilter::InitFromValue(
    const base::DictionaryValue& value, std::string* error) {
  if (!value.HasKey("urls"))
    return false;

  for (base::DictionaryValue::Iterator it(value); !it.IsAtEnd(); it.Advance()) {
    if (it.key() == "urls") {
      const base::ListValue* urls_value = NULL;
      if (!it.value().GetAsList(&urls_value))
        return false;
      for (size_t i = 0; i < urls_value->GetSize(); ++i) {
        std::string url;
        URLPattern pattern(URLPattern::SCHEME_HTTP | URLPattern::SCHEME_HTTPS |
                           URLPattern::SCHEME_FTP | URLPattern::SCHEME_FILE |
                           URLPattern::SCHEME_EXTENSION |
                           URLPattern::SCHEME_WS | URLPattern::SCHEME_WSS);
        if (!urls_value->GetString(i, &url) ||
            pattern.Parse(url) != URLPattern::ParseResult::kSuccess) {
          *error = ErrorUtils::FormatErrorMessage(
              keys::kInvalidRequestFilterUrl, url);
          return false;
        }
        urls.AddPattern(pattern);
      }
    } else if (it.key() == "types") {
      const base::ListValue* types_value = NULL;
      if (!it.value().GetAsList(&types_value))
        return false;
      for (size_t i = 0; i < types_value->GetSize(); ++i) {
        std::string type_str;
        types.push_back(WebRequestResourceType::OTHER);
        if (!types_value->GetString(i, &type_str) ||
            !ParseWebRequestResourceType(type_str, &types.back())) {
          return false;
        }
      }
    } else if (it.key() == "tabId") {
      if (!it.value().GetAsInteger(&tab_id))
        return false;
    } else if (it.key() == "windowId") {
      if (!it.value().GetAsInteger(&window_id))
        return false;
    } else {
      return false;
    }
  }
  return true;
}

ExtensionWebRequestEventRouter::EventResponse::EventResponse(
    const std::string& extension_id, const base::Time& extension_install_time)
    : extension_id(extension_id),
      extension_install_time(extension_install_time),
      cancel(false) {
}

ExtensionWebRequestEventRouter::EventResponse::~EventResponse() {
}

ExtensionWebRequestEventRouter::RequestFilter::RequestFilter()
    : tab_id(-1), window_id(-1) {
}

ExtensionWebRequestEventRouter::RequestFilter::RequestFilter(
    const RequestFilter& other) = default;

ExtensionWebRequestEventRouter::RequestFilter::~RequestFilter() {
}

//
// ExtensionWebRequestEventRouter
//

// static
ExtensionWebRequestEventRouter* ExtensionWebRequestEventRouter::GetInstance() {
  static base::NoDestructor<ExtensionWebRequestEventRouter> instance;
  return instance.get();
}

ExtensionWebRequestEventRouter::ExtensionWebRequestEventRouter()
    : request_time_tracker_(new ExtensionWebRequestTimeTracker) {
}

void ExtensionWebRequestEventRouter::RegisterRulesRegistry(
    void* browser_context,
    int rules_registry_id,
    scoped_refptr<WebRequestRulesRegistry> rules_registry) {
  RulesRegistryKey key(browser_context, rules_registry_id);
  if (rules_registry.get())
    rules_registries_[key] = rules_registry;
  else
    rules_registries_.erase(key);
}

int ExtensionWebRequestEventRouter::OnBeforeRequest(
    void* browser_context,
    const InfoMap* extension_info_map,
    WebRequestInfo* request,
    net::CompletionOnceCallback callback,
    GURL* new_url,
    bool* should_collapse_initiator) {
  DCHECK(should_collapse_initiator);

  if (ShouldHideEvent(browser_context, extension_info_map, *request))
    return net::OK;

  if (IsPageLoad(*request))
    NotifyPageLoad();

  request_time_tracker_->LogRequestStartTime(request->id, base::Time::Now());

  const bool is_incognito_context = IsIncognitoBrowserContext(browser_context);

  // Handle Declarative Net Request API rules. This gets preference over the Web
  // Request and Declarative Web Request APIs. Only checking the rules in the
  // OnBeforeRequest stage works, since the rules currently only depend on the
  // request url, initiator and resource type, which should stay the same during
  // the diffierent network request stages. A redirect should cause another
  // OnBeforeRequest call.
  // |extension_info_map| is null for system level requests.
  if (extension_info_map) {
    using Action = declarative_net_request::RulesetManager::Action;

    Action action = extension_info_map->GetRulesetManager()->EvaluateRequest(
        *request, is_incognito_context, new_url);
    switch (action) {
      case Action::NONE:
        break;
      case Action::BLOCK:
        return net::ERR_BLOCKED_BY_CLIENT;
      case Action::COLLAPSE:
        *should_collapse_initiator = true;
        return net::ERR_BLOCKED_BY_CLIENT;
      case Action::REDIRECT:
        return net::OK;
    }
  }

  // Whether to initialized |blocked_requests_|.
  bool initialize_blocked_requests = false;

  initialize_blocked_requests |=
      ProcessDeclarativeRules(browser_context, extension_info_map,
                              web_request::OnBeforeRequest::kEventName, request,
                              ON_BEFORE_REQUEST, nullptr);

  int extra_info_spec = 0;
  RawListeners listeners = GetMatchingListeners(
      browser_context, extension_info_map,
      web_request::OnBeforeRequest::kEventName, request, &extra_info_spec);
  if (!listeners.empty() && !GetAndSetSignaled(request->id, kOnBeforeRequest)) {
    std::unique_ptr<WebRequestEventDetails> event_details(
        CreateEventDetails(*request, extra_info_spec));
    event_details->SetRequestBody(request);

    initialize_blocked_requests |=
        DispatchEvent(browser_context, extension_info_map, request, listeners,
                      std::move(event_details));
  }

  if (!initialize_blocked_requests)
    return net::OK;  // Nobody saw a reason for modifying the request.

  BlockedRequest& blocked_request = blocked_requests_[request->id];
  blocked_request.event = kOnBeforeRequest;
  blocked_request.is_incognito |= is_incognito_context;
  blocked_request.request = request;
  blocked_request.callback = std::move(callback);
  blocked_request.new_url = new_url;

  if (blocked_request.num_handlers_blocking == 0) {
    // If there are no blocking handlers, only the declarative rules tried
    // to modify the request and we can respond synchronously.
    return ExecuteDeltas(browser_context, request, false /* call_callback*/);
  }
  return net::ERR_IO_PENDING;
}

int ExtensionWebRequestEventRouter::OnBeforeSendHeaders(
    void* browser_context,
    const InfoMap* extension_info_map,
    const WebRequestInfo* request,
    net::CompletionOnceCallback callback,
    net::HttpRequestHeaders* headers) {
  if (ShouldHideEvent(browser_context, extension_info_map, *request))
    return net::OK;

  bool initialize_blocked_requests = false;

  initialize_blocked_requests |= ProcessDeclarativeRules(
      browser_context, extension_info_map, keys::kOnBeforeSendHeadersEvent,
      request, ON_BEFORE_SEND_HEADERS, NULL);

  int extra_info_spec = 0;
  RawListeners listeners = GetMatchingListeners(
      browser_context, extension_info_map, keys::kOnBeforeSendHeadersEvent,
      request, &extra_info_spec);
  if (!listeners.empty() &&
      !GetAndSetSignaled(request->id, kOnBeforeSendHeaders)) {
    std::unique_ptr<WebRequestEventDetails> event_details(
        CreateEventDetails(*request, extra_info_spec));
    event_details->SetRequestHeaders(*headers);

    initialize_blocked_requests |=
        DispatchEvent(browser_context, extension_info_map, request, listeners,
                      std::move(event_details));
  }

  UMA_HISTOGRAM_ENUMERATION(
      "Extensions.WebRequest.OnBeforeSendHeadersEventResponse",
      initialize_blocked_requests ? WebRequestEventResponse::kObserved
                                  : WebRequestEventResponse::kIgnored);

  if (!initialize_blocked_requests)
    return net::OK;  // Nobody saw a reason for modifying the request.

  BlockedRequest& blocked_request = blocked_requests_[request->id];
  blocked_request.event = kOnBeforeSendHeaders;
  blocked_request.is_incognito |= IsIncognitoBrowserContext(browser_context);
  blocked_request.request = request;
  blocked_request.callback = std::move(callback);
  blocked_request.request_headers = headers;

  if (blocked_request.num_handlers_blocking == 0) {
    // If there are no blocking handlers, only the declarative rules tried
    // to modify the request and we can respond synchronously.
    return ExecuteDeltas(browser_context, request, false /* call_callback*/);
  }
  return net::ERR_IO_PENDING;
}

void ExtensionWebRequestEventRouter::OnSendHeaders(
    void* browser_context,
    const InfoMap* extension_info_map,
    const WebRequestInfo* request,
    const net::HttpRequestHeaders& headers) {
  if (ShouldHideEvent(browser_context, extension_info_map, *request))
    return;

  if (GetAndSetSignaled(request->id, kOnSendHeaders))
    return;

  ClearSignaled(request->id, kOnBeforeRedirect);

  int extra_info_spec = 0;
  RawListeners listeners = GetMatchingListeners(
      browser_context, extension_info_map, keys::kOnSendHeadersEvent, request,
      &extra_info_spec);
  if (listeners.empty())
    return;

  std::unique_ptr<WebRequestEventDetails> event_details(
      CreateEventDetails(*request, extra_info_spec));
  event_details->SetRequestHeaders(headers);

  DispatchEvent(browser_context, extension_info_map, request, listeners,
                std::move(event_details));
}

int ExtensionWebRequestEventRouter::OnHeadersReceived(
    void* browser_context,
    const InfoMap* extension_info_map,
    const WebRequestInfo* request,
    net::CompletionOnceCallback callback,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    GURL* allowed_unsafe_redirect_url) {
  if (ShouldHideEvent(browser_context, extension_info_map, *request))
    return net::OK;

  bool initialize_blocked_requests = false;

  initialize_blocked_requests |= ProcessDeclarativeRules(
      browser_context, extension_info_map, keys::kOnHeadersReceivedEvent,
      request, ON_HEADERS_RECEIVED, original_response_headers);

  int extra_info_spec = 0;
  RawListeners listeners = GetMatchingListeners(
      browser_context, extension_info_map, keys::kOnHeadersReceivedEvent,
      request, &extra_info_spec);

  if (!listeners.empty() &&
      !GetAndSetSignaled(request->id, kOnHeadersReceived)) {
    std::unique_ptr<WebRequestEventDetails> event_details(
        CreateEventDetails(*request, extra_info_spec));
    event_details->SetResponseHeaders(*request, original_response_headers);

    initialize_blocked_requests |=
        DispatchEvent(browser_context, extension_info_map, request, listeners,
                      std::move(event_details));
  }

  UMA_HISTOGRAM_ENUMERATION(
      "Extensions.WebRequest.OnHeadersReceivedEventResponse",
      initialize_blocked_requests ? WebRequestEventResponse::kObserved
                                  : WebRequestEventResponse::kIgnored);

  if (!initialize_blocked_requests)
    return net::OK;  // Nobody saw a reason for modifying the request.

  BlockedRequest& blocked_request = blocked_requests_[request->id];
  blocked_request.event = kOnHeadersReceived;
  blocked_request.is_incognito |= IsIncognitoBrowserContext(browser_context);
  blocked_request.request = request;
  blocked_request.callback = std::move(callback);
  blocked_request.override_response_headers = override_response_headers;
  blocked_request.original_response_headers = original_response_headers;
  blocked_request.new_url = allowed_unsafe_redirect_url;

  if (blocked_request.num_handlers_blocking == 0) {
    // If there are no blocking handlers, only the declarative rules tried
    // to modify the request and we can respond synchronously.
    return ExecuteDeltas(browser_context, request, false /* call_callback*/);
  }
  return net::ERR_IO_PENDING;
}

net::NetworkDelegate::AuthRequiredResponse
ExtensionWebRequestEventRouter::OnAuthRequired(
    void* browser_context,
    const InfoMap* extension_info_map,
    const WebRequestInfo* request,
    const net::AuthChallengeInfo& auth_info,
    net::NetworkDelegate::AuthCallback callback,
    net::AuthCredentials* credentials) {
  // No browser_context means that this is for authentication challenges in the
  // system context. Skip in that case. Also skip sensitive requests.
  if (!browser_context ||
      WebRequestPermissions::HideRequest(extension_info_map, *request)) {
    return net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION;
  }

  int extra_info_spec = 0;
  RawListeners listeners = GetMatchingListeners(
      browser_context, extension_info_map, keys::kOnAuthRequiredEvent, request,
      &extra_info_spec);
  if (listeners.empty())
    return net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION;

  std::unique_ptr<WebRequestEventDetails> event_details(
      CreateEventDetails(*request, extra_info_spec));
  event_details->SetResponseHeaders(*request, request->response_headers.get());
  event_details->SetAuthInfo(auth_info);

  if (DispatchEvent(browser_context, extension_info_map, request, listeners,
                    std::move(event_details))) {
    BlockedRequest& blocked_request = blocked_requests_[request->id];
    blocked_request.event = kOnAuthRequired;
    blocked_request.is_incognito |= IsIncognitoBrowserContext(browser_context);
    blocked_request.request = request;
    blocked_request.auth_callback = std::move(callback);
    blocked_request.auth_credentials = credentials;
    return net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_IO_PENDING;
  }
  return net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION;
}

void ExtensionWebRequestEventRouter::OnBeforeRedirect(
    void* browser_context,
    const InfoMap* extension_info_map,
    const WebRequestInfo* request,
    const GURL& new_location) {
  if (ShouldHideEvent(browser_context, extension_info_map, *request))
    return;

  if (GetAndSetSignaled(request->id, kOnBeforeRedirect))
    return;

  ClearSignaled(request->id, kOnBeforeRequest);
  ClearSignaled(request->id, kOnBeforeSendHeaders);
  ClearSignaled(request->id, kOnSendHeaders);
  ClearSignaled(request->id, kOnHeadersReceived);

  int extra_info_spec = 0;
  RawListeners listeners = GetMatchingListeners(
      browser_context, extension_info_map, keys::kOnBeforeRedirectEvent,
      request, &extra_info_spec);
  if (listeners.empty())
    return;

  std::unique_ptr<WebRequestEventDetails> event_details(
      CreateEventDetails(*request, extra_info_spec));
  event_details->SetResponseHeaders(*request, request->response_headers.get());
  event_details->SetResponseSource(*request);
  event_details->SetString(keys::kRedirectUrlKey, new_location.spec());

  DispatchEvent(browser_context, extension_info_map, request, listeners,
                std::move(event_details));
}

void ExtensionWebRequestEventRouter::OnResponseStarted(
    void* browser_context,
    const InfoMap* extension_info_map,
    const WebRequestInfo* request,
    int net_error) {
  DCHECK_NE(net::ERR_IO_PENDING, net_error);

  if (ShouldHideEvent(browser_context, extension_info_map, *request))
    return;

  // OnResponseStarted is even triggered, when the request was cancelled.
  if (net_error != net::OK)
    return;

  int extra_info_spec = 0;
  RawListeners listeners = GetMatchingListeners(
      browser_context, extension_info_map, keys::kOnResponseStartedEvent,
      request, &extra_info_spec);
  if (listeners.empty())
    return;

  std::unique_ptr<WebRequestEventDetails> event_details(
      CreateEventDetails(*request, extra_info_spec));
  event_details->SetResponseHeaders(*request, request->response_headers.get());
  event_details->SetResponseSource(*request);

  DispatchEvent(browser_context, extension_info_map, request, listeners,
                std::move(event_details));
}

void ExtensionWebRequestEventRouter::OnCompleted(
    void* browser_context,
    const InfoMap* extension_info_map,
    const WebRequestInfo* request,
    int net_error) {
  // We hide events from the system context as well as sensitive requests.
  // However, if the request first became sensitive after redirecting we have
  // already signaled it and thus we have to signal the end of it. This is
  // risk-free because the handler cannot modify the request now.
  if (!browser_context ||
      (WebRequestPermissions::HideRequest(extension_info_map, *request) &&
       !WasSignaled(*request))) {
    return;
  }

  request_time_tracker_->LogRequestEndTime(request->id, base::Time::Now());

  // See comment in OnErrorOccurred regarding net::ERR_WS_UPGRADE.
  DCHECK(net_error == net::OK || net_error == net::ERR_WS_UPGRADE);

  DCHECK(!GetAndSetSignaled(request->id, kOnCompleted));

  ClearPendingCallbacks(*request);

  int extra_info_spec = 0;
  RawListeners listeners =
      GetMatchingListeners(browser_context, extension_info_map,
                           keys::kOnCompletedEvent, request, &extra_info_spec);
  if (listeners.empty())
    return;

  std::unique_ptr<WebRequestEventDetails> event_details(
      CreateEventDetails(*request, extra_info_spec));
  event_details->SetResponseHeaders(*request, request->response_headers.get());
  event_details->SetResponseSource(*request);

  DispatchEvent(browser_context, extension_info_map, request, listeners,
                std::move(event_details));
}

void ExtensionWebRequestEventRouter::OnErrorOccurred(
    void* browser_context,
    const InfoMap* extension_info_map,
    const WebRequestInfo* request,
    bool started,
    int net_error) {
  // When WebSocket handshake request finishes, the request is cancelled with an
  // ERR_WS_UPGRADE code (see WebSocketStreamRequestImpl::PerformUpgrade).
  // WebRequest API reports this as a completed request.
  if (net_error == net::ERR_WS_UPGRADE) {
    OnCompleted(browser_context, extension_info_map, request, net_error);
    return;
  }

  ExtensionsBrowserClient* client = ExtensionsBrowserClient::Get();
  if (!client) {
    // |client| could be NULL during shutdown.
    return;
  }
  // We hide events from the system context as well as sensitive requests.
  // However, if the request first became sensitive after redirecting we have
  // already signaled it and thus we have to signal the end of it. This is
  // risk-free because the handler cannot modify the request now.
  if (!browser_context ||
      (WebRequestPermissions::HideRequest(extension_info_map, *request) &&
       !WasSignaled(*request))) {
    return;
  }

  request_time_tracker_->LogRequestEndTime(request->id, base::Time::Now());

  DCHECK_NE(net::OK, net_error);
  DCHECK_NE(net::ERR_IO_PENDING, net_error);

  DCHECK(!GetAndSetSignaled(request->id, kOnErrorOccurred));

  ClearPendingCallbacks(*request);

  int extra_info_spec = 0;
  RawListeners listeners = GetMatchingListeners(
      browser_context, extension_info_map,
      web_request::OnErrorOccurred::kEventName, request, &extra_info_spec);
  if (listeners.empty())
    return;

  std::unique_ptr<WebRequestEventDetails> event_details(
      CreateEventDetails(*request, extra_info_spec));
  if (started)
    event_details->SetResponseSource(*request);
  else
    event_details->SetBoolean(keys::kFromCache, request->response_from_cache);
  event_details->SetString(keys::kErrorKey, net::ErrorToString(net_error));

  DispatchEvent(browser_context, extension_info_map, request, listeners,
                std::move(event_details));
}

void ExtensionWebRequestEventRouter::OnRequestWillBeDestroyed(
    void* browser_context,
    const WebRequestInfo* request) {
  ClearPendingCallbacks(*request);
  signaled_requests_.erase(request->id);
  request_time_tracker_->LogRequestEndTime(request->id, base::Time::Now());
}

void ExtensionWebRequestEventRouter::ClearPendingCallbacks(
    const WebRequestInfo& request) {
  blocked_requests_.erase(request.id);
}

bool ExtensionWebRequestEventRouter::DispatchEvent(
    void* browser_context,
    const InfoMap* extension_info_map,
    const WebRequestInfo* request,
    const RawListeners& listeners,
    std::unique_ptr<WebRequestEventDetails> event_details) {
  // TODO(mpcomplete): Consider consolidating common (extension_id,json_args)
  // pairs into a single message sent to a list of sub_event_names.
  int num_handlers_blocking = 0;

  std::unique_ptr<ListenerIDs> listeners_to_dispatch(new ListenerIDs);
  listeners_to_dispatch->reserve(listeners.size());
  for (EventListener* listener : listeners) {
    listeners_to_dispatch->push_back(listener->id);
    if (listener->extra_info_spec &
        (ExtraInfoSpec::BLOCKING | ExtraInfoSpec::ASYNC_BLOCKING)) {
      listener->blocked_requests.insert(request->id);
      // If this is the first delegate blocking the request, go ahead and log
      // it.
      if (num_handlers_blocking == 0) {
        std::string delegate_info = l10n_util::GetStringFUTF8(
            IDS_LOAD_STATE_PARAMETER_EXTENSION,
            base::UTF8ToUTF16(listener->extension_name));
        request->logger->LogBlockedBy(delegate_info);
      }
      ++num_handlers_blocking;
    }
  }

  if (request->frame_data) {
    // We may already have FrameData if this is a browser-side navigation, or
    // if the relevant FrameData was already cached on the IO thread at the time
    // of request creation. In that case we can dispatch immediately.
    event_details->SetFrameData(request->frame_data.value());
    DispatchEventToListeners(browser_context, extension_info_map,
                             std::move(listeners_to_dispatch),
                             std::move(event_details));
  } else {
    // We don't have FrameData available yet, so fetch it asynchronously. This
    // event will be dispatched once that operation completes. Unretained is
    // safe here because the ExtensionWebRequestEventRouter singleton is leaked.
    event_details.release()->DetermineFrameDataOnIO(
        base::Bind(&ExtensionWebRequestEventRouter::DispatchEventToListeners,
                   base::Unretained(this), browser_context,
                   base::RetainedRef(extension_info_map),
                   base::Passed(&listeners_to_dispatch)));
  }

  if (num_handlers_blocking > 0) {
    BlockedRequest& blocked_request = blocked_requests_[request->id];
    blocked_request.request = request;
    blocked_request.is_incognito |= IsIncognitoBrowserContext(browser_context);
    blocked_request.num_handlers_blocking += num_handlers_blocking;
    blocked_request.blocking_time = base::Time::Now();
    return true;
  }

  return false;
}

void ExtensionWebRequestEventRouter::DispatchEventToListeners(
    void* browser_context,
    const InfoMap* extension_info_map,
    std::unique_ptr<ListenerIDs> listener_ids,
    std::unique_ptr<WebRequestEventDetails> event_details) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!listener_ids->empty());
  DCHECK(event_details.get());

  std::string event_name =
      EventRouter::GetBaseEventName((*listener_ids)[0].sub_event_name);
  DCHECK(IsWebRequestEvent(event_name));

  Listeners& event_listeners = listeners_[browser_context][event_name];
  void* cross_browser_context = GetCrossBrowserContext(browser_context);
  Listeners* cross_event_listeners =
      cross_browser_context ? &listeners_[cross_browser_context][event_name]
                            : nullptr;

  std::unique_ptr<WebRequestEventDetails> event_details_filtered_copy;

  for (const EventListener::ID& id : *listener_ids) {
    // It's possible that the listener is no longer present. Check to make sure
    // it's still there.
    const EventListener* listener =
        FindEventListenerInContainer(id, event_listeners);
    if (!listener && cross_event_listeners) {
      listener = FindEventListenerInContainer(id, *cross_event_listeners);
    }
    if (!listener)
      continue;

    if (!listener->ipc_sender.get())
      continue;

    // Filter out the optional keys that this listener didn't request.
    std::unique_ptr<base::ListValue> args_filtered(new base::ListValue);
    void* cross_browser_context = GetCrossBrowserContext(browser_context);

    // In Public Sessions we want to restrict access to security or privacy
    // sensitive data. Data is filtered for *all* listeners, not only extensions
    // which are force-installed by policy. Whitelisted extensions are exempt
    // from this filtering.
    WebRequestEventDetails* custom_event_details = event_details.get();
    if (ArePublicSessionRestrictionsEnabled() &&
        !extensions::IsWhitelistedForPublicSession(listener->id.extension_id)) {
      if (!event_details_filtered_copy) {
        event_details_filtered_copy =
            event_details->CreatePublicSessionCopy();
      }
      custom_event_details = event_details_filtered_copy.get();
    }
    args_filtered->Append(custom_event_details->GetFilteredDict(
        listener->extra_info_spec, extension_info_map,
        listener->id.extension_id, (cross_browser_context != 0)));

    EventRouter::DispatchEventToSender(
        listener->ipc_sender.get(), browser_context, listener->id.extension_id,
        listener->histogram_value, listener->id.sub_event_name,
        std::move(args_filtered), EventRouter::USER_GESTURE_UNKNOWN,
        EventFilteringInfo());
  }
}

void ExtensionWebRequestEventRouter::OnEventHandled(
    void* browser_context,
    const std::string& extension_id,
    const std::string& event_name,
    const std::string& sub_event_name,
    uint64_t request_id,
    EventResponse* response) {
  // TODO(robwu): This ignores WebViews.
  Listeners& listeners = listeners_[browser_context][event_name];
  EventListener::ID id(browser_context, extension_id, sub_event_name, 0, 0);
  for (auto it = listeners.begin(); it != listeners.end(); ++it) {
    if ((*it)->id.LooselyMatches(id)) {
      (*it)->blocked_requests.erase(request_id);
    }
  }

  DecrementBlockCount(
      browser_context, extension_id, event_name, request_id, response);
}

bool ExtensionWebRequestEventRouter::AddEventListener(
    void* browser_context,
    const std::string& extension_id,
    const std::string& extension_name,
    events::HistogramValue histogram_value,
    const std::string& event_name,
    const std::string& sub_event_name,
    const RequestFilter& filter,
    int extra_info_spec,
    int embedder_process_id,
    int web_view_instance_id,
    base::WeakPtr<IPC::Sender> ipc_sender) {
  if (!IsWebRequestEvent(event_name))
    return false;

  if (event_name != EventRouter::GetBaseEventName(sub_event_name))
    return false;

  EventListener::ID id(browser_context, extension_id, sub_event_name,
                       embedder_process_id, web_view_instance_id);
  if (FindEventListener(id) != nullptr) {
    // This is likely an abuse of the API by a malicious extension.
    return false;
  }

  std::unique_ptr<EventListener> listener(new EventListener(id));
  listener->extension_name = extension_name;
  listener->histogram_value = histogram_value;
  listener->filter = filter;
  listener->extra_info_spec = extra_info_spec;
  listener->ipc_sender = ipc_sender;
  if (web_view_instance_id) {
    base::RecordAction(
        base::UserMetricsAction("WebView.WebRequest.AddListener"));
  }

  listeners_[browser_context][event_name].push_back(std::move(listener));
  return true;
}

size_t ExtensionWebRequestEventRouter::GetListenerCountForTesting(
    void* browser_context,
    const std::string& event_name) {
  return listeners_[browser_context][event_name].size();
}

ExtensionWebRequestEventRouter::EventListener*
ExtensionWebRequestEventRouter::FindEventListener(const EventListener::ID& id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::string event_name = EventRouter::GetBaseEventName(id.sub_event_name);
  Listeners& listeners = listeners_[id.browser_context][event_name];
  return FindEventListenerInContainer(id, listeners);
}

ExtensionWebRequestEventRouter::EventListener*
ExtensionWebRequestEventRouter::FindEventListenerInContainer(
    const EventListener::ID& id,
    Listeners& listeners) {
  for (auto it = listeners.begin(); it != listeners.end(); ++it) {
    if ((*it)->id == id) {
      return it->get();
    }
  }
  return nullptr;
}

void ExtensionWebRequestEventRouter::RemoveEventListener(
    const EventListener::ID& id,
    bool strict) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::string event_name = EventRouter::GetBaseEventName(id.sub_event_name);
  Listeners& listeners = listeners_[id.browser_context][event_name];
  for (auto it = listeners.begin(); it != listeners.end(); ++it) {
    std::unique_ptr<EventListener>& listener = *it;

    // There are two places that call this method: RemoveWebViewEventListeners
    // and OnListenerRemoved. The latter can't use operator== because it doesn't
    // have the embedder_process_id. This shouldn't be a problem, because
    // OnListenerRemoved is only called for web_view_instance_id == 0.
    bool matches =
        strict ? listener->id == id : listener->id.LooselyMatches(id);
    if (matches) {
      // Unblock any request that this event listener may have been blocking.
      for (uint64_t blocked_request_id : listener->blocked_requests)
        DecrementBlockCount(listener->id.browser_context,
                            listener->id.extension_id, event_name,
                            blocked_request_id, nullptr);

      listeners.erase(it);
      helpers::ClearCacheOnNavigation();
      return;
    }
  }
}

void ExtensionWebRequestEventRouter::RemoveWebViewEventListeners(
    void* browser_context,
    int embedder_process_id,
    int web_view_instance_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Iterate over all listeners of all WebRequest events to delete
  // any listeners that belong to the provided <webview>.
  ListenerMapForBrowserContext& map_for_browser_context =
      listeners_[browser_context];
  for (const auto& event_iter : map_for_browser_context) {
    // Construct a listeners_to_delete vector so that we don't modify the set of
    // listeners as we iterate through it.
    std::vector<EventListener::ID> listeners_to_delete;
    const Listeners& listeners = event_iter.second;
    for (const auto& listener : listeners) {
      if (listener->id.embedder_process_id == embedder_process_id &&
          listener->id.web_view_instance_id == web_view_instance_id) {
        listeners_to_delete.push_back(listener->id);
      }
    }
    // Remove the listeners selected for deletion.
    for (const auto& listener_id : listeners_to_delete)
      RemoveEventListener(listener_id, true /* strict */);
  }
}

void ExtensionWebRequestEventRouter::OnOTRBrowserContextCreated(
    void* original_browser_context, void* otr_browser_context) {
  cross_browser_context_map_[original_browser_context] =
      std::make_pair(false, otr_browser_context);
  cross_browser_context_map_[otr_browser_context] =
      std::make_pair(true, original_browser_context);
}

void ExtensionWebRequestEventRouter::OnOTRBrowserContextDestroyed(
    void* original_browser_context, void* otr_browser_context) {
  cross_browser_context_map_.erase(otr_browser_context);
  cross_browser_context_map_.erase(original_browser_context);
}

void ExtensionWebRequestEventRouter::AddCallbackForPageLoad(
    const base::Closure& callback) {
  callbacks_for_page_load_.push_back(callback);
}

bool ExtensionWebRequestEventRouter::IsPageLoad(
    const WebRequestInfo& request) const {
  return request.type == content::RESOURCE_TYPE_MAIN_FRAME;
}

void ExtensionWebRequestEventRouter::NotifyPageLoad() {
  for (const auto& callback : callbacks_for_page_load_)
    callback.Run();
  callbacks_for_page_load_.clear();
}

void* ExtensionWebRequestEventRouter::GetCrossBrowserContext(
    void* browser_context) const {
  auto cross_browser_context = cross_browser_context_map_.find(browser_context);
  if (cross_browser_context == cross_browser_context_map_.end())
    return NULL;
  return cross_browser_context->second.second;
}

bool ExtensionWebRequestEventRouter::IsIncognitoBrowserContext(
    void* browser_context) const {
  auto cross_browser_context = cross_browser_context_map_.find(browser_context);
  if (cross_browser_context == cross_browser_context_map_.end())
    return false;
  return cross_browser_context->second.first;
}

bool ExtensionWebRequestEventRouter::WasSignaled(
    const WebRequestInfo& request) const {
  auto flag = signaled_requests_.find(request.id);
  return flag != signaled_requests_.end() && flag->second != 0;
}

void ExtensionWebRequestEventRouter::GetMatchingListenersImpl(
    void* browser_context,
    const WebRequestInfo* request,
    const InfoMap* extension_info_map,
    bool crosses_incognito,
    const std::string& event_name,
    bool is_request_from_extension,
    int* extra_info_spec,
    RawListeners* matching_listeners) {
  std::string web_request_event_name(event_name);
  if (request->is_web_view) {
    web_request_event_name.replace(
        0, sizeof(kWebRequestEventPrefix) - 1, webview::kWebViewEventPrefix);
  }

  Listeners& listeners = listeners_[browser_context][web_request_event_name];
  for (std::unique_ptr<EventListener>& listener : listeners) {
    if (!listener->ipc_sender.get()) {
      // The IPC sender has been deleted. This listener will be removed soon
      // via a call to RemoveEventListener. For now, just skip it.
      continue;
    }

    if (request->is_web_view) {
      // If this is a PlzNavigate request, then we can skip this check. IDs will
      // be -1 and the request is trusted.
      if (!request->is_browser_side_navigation &&
          (listener->id.embedder_process_id !=
           request->web_view_embedder_process_id)) {
        continue;
      }

      if (listener->id.web_view_instance_id != request->web_view_instance_id)
        continue;
    }

    // Filter requests from other extensions / apps. This does not work for
    // content scripts, or extension pages in non-extension processes.
    if (is_request_from_extension &&
        listener->id.embedder_process_id != request->render_process_id) {
      continue;
    }

    if (!listener->filter.urls.is_empty() &&
        !listener->filter.urls.MatchesURL(request->url)) {
      continue;
    }

    // Check if the tab id and window id match, if they were set in the
    // listener params.
    if ((listener->filter.tab_id != -1 && request->frame_data &&
         request->frame_data->tab_id != listener->filter.tab_id) ||
        (listener->filter.window_id != -1 && request->frame_data &&
         request->frame_data->window_id != listener->filter.window_id)) {
      continue;
    }

    const std::vector<WebRequestResourceType>& types = listener->filter.types;
    if (!types.empty() &&
        !base::ContainsValue(types, request->web_request_type)) {
      continue;
    }

    if (!request->is_web_view) {
      PermissionsData::PageAccess access =
          WebRequestPermissions::CanExtensionAccessURL(
              extension_info_map, listener->id.extension_id, request->url,
              request->frame_data ? request->frame_data->tab_id : -1,
              crosses_incognito,
              WebRequestPermissions::REQUIRE_HOST_PERMISSION_FOR_URL,
              request->initiator);

      if (access != PermissionsData::PageAccess::kAllowed) {
        if (access == PermissionsData::PageAccess::kWithheld) {
          DCHECK(ExtensionsAPIClient::Get());
          ExtensionsAPIClient::Get()->NotifyWebRequestWithheld(
              request->render_process_id, request->frame_id,
              listener->id.extension_id);
        }
        continue;
      }
    }

    bool blocking_listener =
        (listener->extra_info_spec &
         (ExtraInfoSpec::BLOCKING | ExtraInfoSpec::ASYNC_BLOCKING)) != 0;

    // We do not want to notify extensions about XHR requests that are
    // triggered by themselves. This is a workaround to prevent deadlocks
    // in case of synchronous XHR requests that block the extension renderer
    // and therefore prevent the extension from processing the request
    // handler. This is only a problem for blocking listeners.
    // http://crbug.com/105656
    bool synchronous_xhr_from_extension =
        !request->is_async && is_request_from_extension &&
        request->web_request_type == WebRequestResourceType::XHR;

    // Only send webRequest events for URLs the extension has access to.
    if (blocking_listener && synchronous_xhr_from_extension)
      continue;

    matching_listeners->push_back(listener.get());
    *extra_info_spec |= listener->extra_info_spec;
  }
}

ExtensionWebRequestEventRouter::RawListeners
ExtensionWebRequestEventRouter::GetMatchingListeners(
    void* browser_context,
    const InfoMap* extension_info_map,
    const std::string& event_name,
    const WebRequestInfo* request,
    int* extra_info_spec) {
  // TODO(mpcomplete): handle browser_context == NULL (should collect all
  // listeners).
  *extra_info_spec = 0;

  bool is_request_from_extension =
      IsRequestFromExtension(*request, extension_info_map);

  RawListeners matching_listeners;
  GetMatchingListenersImpl(browser_context, request, extension_info_map, false,
                           event_name, is_request_from_extension,
                           extra_info_spec, &matching_listeners);
  void* cross_browser_context = GetCrossBrowserContext(browser_context);
  if (cross_browser_context) {
    GetMatchingListenersImpl(cross_browser_context, request, extension_info_map,
                             true, event_name, is_request_from_extension,
                             extra_info_spec, &matching_listeners);
  }

  return matching_listeners;
}

namespace {

helpers::EventResponseDelta* CalculateDelta(
    ExtensionWebRequestEventRouter::BlockedRequest* blocked_request,
    ExtensionWebRequestEventRouter::EventResponse* response) {
  switch (blocked_request->event) {
    case ExtensionWebRequestEventRouter::kOnBeforeRequest:
      return helpers::CalculateOnBeforeRequestDelta(
          response->extension_id, response->extension_install_time,
          response->cancel, response->new_url);
    case ExtensionWebRequestEventRouter::kOnBeforeSendHeaders: {
      net::HttpRequestHeaders* old_headers = blocked_request->request_headers;
      net::HttpRequestHeaders* new_headers = response->request_headers.get();
      return helpers::CalculateOnBeforeSendHeadersDelta(
          response->extension_id, response->extension_install_time,
          response->cancel, old_headers, new_headers);
    }
    case ExtensionWebRequestEventRouter::kOnHeadersReceived: {
      const net::HttpResponseHeaders* old_headers =
          blocked_request->original_response_headers.get();
      helpers::ResponseHeaders* new_headers =
          response->response_headers.get();
      return helpers::CalculateOnHeadersReceivedDelta(
          response->extension_id, response->extension_install_time,
          response->cancel, blocked_request->request->url, response->new_url,
          old_headers, new_headers);
    }
    case ExtensionWebRequestEventRouter::kOnAuthRequired:
      return helpers::CalculateOnAuthRequiredDelta(
          response->extension_id, response->extension_install_time,
          response->cancel, &response->auth_credentials);
    default:
      NOTREACHED();
      return nullptr;
  }
}

std::unique_ptr<base::Value> SerializeResponseHeaders(
    const helpers::ResponseHeaders& headers) {
  auto serialized_headers = std::make_unique<base::ListValue>();
  for (const auto& it : headers) {
    serialized_headers->Append(
        helpers::CreateHeaderDictionary(it.first, it.second));
  }
  return std::move(serialized_headers);
}

// Convert a RequestCookieModifications/ResponseCookieModifications object to a
// base::ListValue which summarizes the changes made.  This is templated since
// the two types (request/response) are different but contain essentially the
// same fields.
template <typename CookieType>
std::unique_ptr<base::ListValue> SummarizeCookieModifications(
    const std::vector<linked_ptr<CookieType>>& modifications) {
  auto cookie_modifications = std::make_unique<base::ListValue>();
  for (const auto& it : modifications) {
    auto summary = std::make_unique<base::DictionaryValue>();
    const CookieType& mod = *(it.get());
    switch (mod.type) {
      case helpers::ADD:
        summary->SetString(activity_log::kCookieModificationTypeKey,
                           activity_log::kCookieModificationAdd);
        break;
      case helpers::EDIT:
        summary->SetString(activity_log::kCookieModificationTypeKey,
                           activity_log::kCookieModificationEdit);
        break;
      case helpers::REMOVE:
        summary->SetString(activity_log::kCookieModificationTypeKey,
                           activity_log::kCookieModificationRemove);
        break;
    }
    if (mod.filter) {
      if (mod.filter->name) {
        summary->SetString(activity_log::kCookieFilterNameKey,
                           *mod.modification->name);
      }
      if (mod.filter->domain) {
        summary->SetString(activity_log::kCookieFilterDomainKey,
                           *mod.modification->name);
      }
    }
    if (mod.modification) {
      if (mod.modification->name) {
        summary->SetString(activity_log::kCookieModDomainKey,
                           *mod.modification->name);
      }
      if (mod.modification->domain) {
        summary->SetString(activity_log::kCookieModDomainKey,
                           *mod.modification->name);
      }
    }
    cookie_modifications->Append(std::move(summary));
  }
  return cookie_modifications;
}

// Converts an EventResponseDelta object to a dictionary value suitable for the
// activity log.
std::unique_ptr<base::DictionaryValue> SummarizeResponseDelta(
    const std::string& event_name,
    const helpers::EventResponseDelta& delta) {
  std::unique_ptr<base::DictionaryValue> details(new base::DictionaryValue());
  if (delta.cancel)
    details->SetBoolean(activity_log::kCancelKey, true);
  if (!delta.new_url.is_empty())
    details->SetString(activity_log::kNewUrlKey, delta.new_url.spec());

  std::unique_ptr<base::ListValue> modified_headers(new base::ListValue());
  net::HttpRequestHeaders::Iterator iter(delta.modified_request_headers);
  while (iter.GetNext()) {
    modified_headers->Append(
        helpers::CreateHeaderDictionary(iter.name(), iter.value()));
  }
  if (!modified_headers->empty()) {
    details->Set(activity_log::kModifiedRequestHeadersKey,
                 std::move(modified_headers));
  }

  std::unique_ptr<base::ListValue> deleted_headers(new base::ListValue());
  deleted_headers->AppendStrings(delta.deleted_request_headers);
  if (!deleted_headers->empty()) {
    details->Set(activity_log::kDeletedRequestHeadersKey,
                 std::move(deleted_headers));
  }

  if (!delta.added_response_headers.empty()) {
    details->Set(activity_log::kAddedRequestHeadersKey,
                 SerializeResponseHeaders(delta.added_response_headers));
  }
  if (!delta.deleted_response_headers.empty()) {
    details->Set(activity_log::kDeletedResponseHeadersKey,
                 SerializeResponseHeaders(delta.deleted_response_headers));
  }
  if (delta.auth_credentials) {
    details->SetString(
        activity_log::kAuthCredentialsKey,
        base::UTF16ToUTF8(delta.auth_credentials->username()) + ":*");
  }

  if (!delta.response_cookie_modifications.empty()) {
    details->Set(
        activity_log::kResponseCookieModificationsKey,
        SummarizeCookieModifications(delta.response_cookie_modifications));
  }

  return details;
}

}  // namespace

void ExtensionWebRequestEventRouter::DecrementBlockCount(
    void* browser_context,
    const std::string& extension_id,
    const std::string& event_name,
    uint64_t request_id,
    EventResponse* response) {
  std::unique_ptr<EventResponse> response_scoped(response);

  // It's possible that this request was deleted, or cancelled by a previous
  // event handler. If so, ignore this response.
  auto it = blocked_requests_.find(request_id);
  if (it == blocked_requests_.end())
    return;

  BlockedRequest& blocked_request = it->second;
  int num_handlers_blocking = --blocked_request.num_handlers_blocking;
  CHECK_GE(num_handlers_blocking, 0);

  if (response) {
    helpers::EventResponseDelta* delta =
        CalculateDelta(&blocked_request, response);

    activity_monitor::OnWebRequestApiUsed(
        static_cast<content::BrowserContext*>(browser_context), extension_id,
        blocked_request.request->url, blocked_request.is_incognito, event_name,
        SummarizeResponseDelta(event_name, *delta));

    blocked_request.response_deltas.push_back(
        linked_ptr<helpers::EventResponseDelta>(delta));
  }

  if (num_handlers_blocking == 0) {
    blocked_request.request->logger->LogUnblocked();
    ExecuteDeltas(browser_context, blocked_request.request, true);
  } else {
    // Log that the request was blocked by an extension.
    Listeners& listeners = listeners_[browser_context][event_name];

    for (const auto& listener : listeners) {
      if (!base::ContainsKey(listener->blocked_requests, request_id))
        continue;
      std::string delegate_info = l10n_util::GetStringFUTF8(
          IDS_LOAD_STATE_PARAMETER_EXTENSION,
          base::UTF8ToUTF16(listener->extension_name));
      blocked_request.request->logger->LogBlockedBy(delegate_info);
      break;
    }
  }
}

void ExtensionWebRequestEventRouter::SendMessages(
    void* browser_context,
    const BlockedRequest& blocked_request) {
  const helpers::EventResponseDeltas& deltas = blocked_request.response_deltas;
  for (const auto& delta : deltas) {
    const std::set<std::string>& messages = delta->messages_to_extension;
    for (const std::string& message : messages) {
      std::unique_ptr<WebRequestEventDetails> event_details(CreateEventDetails(
          *blocked_request.request, /* extra_info_spec */ 0));
      event_details->SetString(keys::kMessageKey, message);
      event_details->SetString(keys::kStageKey,
                               GetRequestStageAsString(blocked_request.event));
      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::UI},
          base::Bind(&SendOnMessageEventOnUI, browser_context,
                     delta->extension_id, blocked_request.request->is_web_view,
                     blocked_request.request->web_view_instance_id,
                     base::Passed(&event_details)));
    }
  }
}

int ExtensionWebRequestEventRouter::ExecuteDeltas(void* browser_context,
                                                  const WebRequestInfo* request,
                                                  bool call_callback) {
  BlockedRequest& blocked_request = blocked_requests_[request->id];
  CHECK_EQ(0, blocked_request.num_handlers_blocking);
  helpers::EventResponseDeltas& deltas = blocked_request.response_deltas;
  base::TimeDelta block_time =
      base::Time::Now() - blocked_request.blocking_time;
  request_time_tracker_->IncrementTotalBlockTime(request->id, block_time);

  bool request_headers_modified = false;
  bool response_headers_modified = false;
  bool credentials_set = false;

  deltas.sort(&helpers::InDecreasingExtensionInstallationTimeOrder);

  bool canceled = false;
  helpers::MergeCancelOfResponses(blocked_request.response_deltas, &canceled,
                                  request->logger.get());

  extension_web_request_api_helpers::IgnoredActions ignored_actions;
  if (blocked_request.event == kOnBeforeRequest) {
    CHECK(!blocked_request.callback.is_null());
    helpers::MergeOnBeforeRequestResponses(
        request->url, blocked_request.response_deltas, blocked_request.new_url,
        &ignored_actions, request->logger.get());
  } else if (blocked_request.event == kOnBeforeSendHeaders) {
    CHECK(!blocked_request.callback.is_null());
    helpers::MergeOnBeforeSendHeadersResponses(
        request->url, blocked_request.response_deltas,
        blocked_request.request_headers, &ignored_actions,
        request->logger.get(), &request_headers_modified);
  } else if (blocked_request.event == kOnHeadersReceived) {
    CHECK(!blocked_request.callback.is_null());
    helpers::MergeOnHeadersReceivedResponses(
        request->url, blocked_request.response_deltas,
        blocked_request.original_response_headers.get(),
        blocked_request.override_response_headers, blocked_request.new_url,
        &ignored_actions, request->logger.get(), &response_headers_modified);
  } else if (blocked_request.event == kOnAuthRequired) {
    CHECK(blocked_request.callback.is_null());
    CHECK(!blocked_request.auth_callback.is_null());
    credentials_set = helpers::MergeOnAuthRequiredResponses(
        blocked_request.response_deltas, blocked_request.auth_credentials,
        &ignored_actions, request->logger.get());
  } else {
    NOTREACHED();
  }

  SendMessages(browser_context, blocked_request);

  if (!ignored_actions.empty()) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&NotifyIgnoredActionsOnUI, browser_context, request->id,
                       std::move(ignored_actions)));
  }

  const bool redirected =
      blocked_request.new_url && !blocked_request.new_url->is_empty();

  if (canceled)
    request_time_tracker_->SetRequestCanceled(request->id);
  else if (redirected)
    request_time_tracker_->SetRequestRedirected(request->id);

  // Log UMA metrics. Note: We are not necessarily concerned with the final
  // action taken. Instead we are interested in how frequently the different
  // actions are used by extensions. Hence multiple actions may be logged for a
  // single delta execution.
  if (canceled)
    LogRequestAction(RequestAction::CANCEL);
  if (redirected)
    LogRequestAction(RequestAction::REDIRECT);
  if (request_headers_modified)
    LogRequestAction(RequestAction::MODIFY_REQUEST_HEADERS);
  if (response_headers_modified)
    LogRequestAction(RequestAction::MODIFY_RESPONSE_HEADERS);
  if (credentials_set)
    LogRequestAction(RequestAction::SET_AUTH_CREDENTIALS);

  // This triggers onErrorOccurred if canceled is true.
  int rv = canceled ? net::ERR_BLOCKED_BY_CLIENT : net::OK;

  if (!blocked_request.callback.is_null()) {
    net::CompletionOnceCallback callback = std::move(blocked_request.callback);
    // Ensure that request is removed before callback because the callback
    // might trigger the next event.
    blocked_requests_.erase(request->id);
    if (call_callback)
      std::move(callback).Run(rv);
  } else if (!blocked_request.auth_callback.is_null()) {
    net::NetworkDelegate::AuthRequiredResponse response;
    if (canceled)
      response = net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_CANCEL_AUTH;
    else if (credentials_set)
      response = net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_SET_AUTH;
    else
      response = net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION;

    net::NetworkDelegate::AuthCallback callback =
        std::move(blocked_request.auth_callback);
    blocked_requests_.erase(request->id);
    if (call_callback)
      std::move(callback).Run(response);
  } else {
    blocked_requests_.erase(request->id);
  }
  return rv;
}

bool ExtensionWebRequestEventRouter::ProcessDeclarativeRules(
    void* browser_context,
    const InfoMap* extension_info_map,
    const std::string& event_name,
    const WebRequestInfo* request,
    RequestStage request_stage,
    const net::HttpResponseHeaders* original_response_headers) {
  int rules_registry_id = request->is_web_view
                              ? request->web_view_rules_registry_id
                              : RulesRegistryService::kDefaultRulesRegistryID;

  RulesRegistryKey rules_key(browser_context, rules_registry_id);
  // If this check fails, check that the active stages are up to date in
  // extensions/browser/api/declarative_webrequest/request_stage.h .
  DCHECK(request_stage & kActiveStages);

  // Rules of the current |browser_context| may apply but we need to check also
  // whether there are applicable rules from extensions whose background page
  // spans from regular to incognito mode.

  // First parameter identifies the registry, the second indicates whether the
  // registry belongs to the cross browser_context.
  using RelevantRegistry = std::pair<WebRequestRulesRegistry*, bool>;
  std::vector<RelevantRegistry> relevant_registries;

  auto rules_key_it = rules_registries_.find(rules_key);
  if (rules_key_it != rules_registries_.end()) {
    relevant_registries.push_back(
        std::make_pair(rules_key_it->second.get(), false));
  }

  void* cross_browser_context = GetCrossBrowserContext(browser_context);
  RulesRegistryKey cross_browser_context_rules_key(cross_browser_context,
                                                   rules_registry_id);
  if (cross_browser_context) {
    auto it = rules_registries_.find(cross_browser_context_rules_key);
    if (it != rules_registries_.end())
      relevant_registries.push_back(std::make_pair(it->second.get(), true));
  }

  // The following block is experimentally enabled and its impact on load time
  // logged with UMA Extensions.NetworkDelayRegistryLoad. crbug.com/175961
  for (auto it : relevant_registries) {
    WebRequestRulesRegistry* rules_registry = it.first;
    if (rules_registry->ready().is_signaled())
      continue;

    // The rules registry is still loading. Block this request until it
    // finishes.
    // This Unretained is safe because the ExtensionWebRequestEventRouter
    // singleton is leaked.
    rules_registry->ready().Post(
        FROM_HERE,
        base::Bind(&ExtensionWebRequestEventRouter::OnRulesRegistryReady,
                   base::Unretained(this), browser_context, event_name,
                   request->id, request_stage));
    BlockedRequest& blocked_request = blocked_requests_[request->id];
    blocked_request.num_handlers_blocking++;
    blocked_request.request = request;
    blocked_request.is_incognito |= IsIncognitoBrowserContext(browser_context);
    blocked_request.blocking_time = base::Time::Now();
    blocked_request.original_response_headers = original_response_headers;
    blocked_request.extension_info_map = extension_info_map;
    return true;
  }

  if (request->is_web_view) {
    const bool has_declarative_rules = !relevant_registries.empty();
    UMA_HISTOGRAM_BOOLEAN(
        "Extensions.DeclarativeWebRequest.WebViewRequestDeclarativeRules",
        has_declarative_rules);
  }

  bool deltas_created = false;
  for (const auto& it : relevant_registries) {
    WebRequestRulesRegistry* rules_registry = it.first;
    helpers::EventResponseDeltas result = rules_registry->CreateDeltas(
        extension_info_map,
        WebRequestData(request, request_stage, original_response_headers),
        it.second);

    if (!result.empty()) {
      helpers::EventResponseDeltas& deltas =
          blocked_requests_[request->id].response_deltas;
      deltas.insert(deltas.end(), result.begin(), result.end());
      deltas_created = true;
    }
  }

  return deltas_created;
}

void ExtensionWebRequestEventRouter::OnRulesRegistryReady(
    void* browser_context,
    const std::string& event_name,
    uint64_t request_id,
    RequestStage request_stage) {
  // It's possible that this request was deleted, or cancelled by a previous
  // event handler. If so, ignore this response.
  auto it = blocked_requests_.find(request_id);
  if (it == blocked_requests_.end())
    return;

  BlockedRequest& blocked_request = it->second;
  base::TimeDelta block_time =
      base::Time::Now() - blocked_request.blocking_time;
  UMA_HISTOGRAM_TIMES("Extensions.NetworkDelayRegistryLoad", block_time);

  ProcessDeclarativeRules(browser_context, blocked_request.extension_info_map,
                          event_name, blocked_request.request, request_stage,
                          blocked_request.original_response_headers.get());
  // Reset to null so that nobody relies on this being set.
  blocked_request.extension_info_map = nullptr;
  DecrementBlockCount(browser_context, std::string(), event_name, request_id,
                      nullptr);
}

bool ExtensionWebRequestEventRouter::GetAndSetSignaled(uint64_t request_id,
                                                       EventTypes event_type) {
  auto iter = signaled_requests_.find(request_id);
  if (iter == signaled_requests_.end()) {
    signaled_requests_[request_id] = event_type;
    return false;
  }
  bool was_signaled_before = (iter->second & event_type) != 0;
  iter->second |= event_type;
  return was_signaled_before;
}

void ExtensionWebRequestEventRouter::ClearSignaled(uint64_t request_id,
                                                   EventTypes event_type) {
  auto iter = signaled_requests_.find(request_id);
  if (iter == signaled_requests_.end())
    return;
  iter->second &= ~event_type;
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
  ClearCacheQuotaHeuristic(const Config& config, BucketMapper* map)
      : QuotaLimitHeuristic(
            config,
            map,
            "MAX_HANDLER_BEHAVIOR_CHANGED_CALLS_PER_10_MINUTES"),
        callback_registered_(false),
        weak_ptr_factory_(this) {}
  ~ClearCacheQuotaHeuristic() override {}
  bool Apply(Bucket* bucket, const base::TimeTicks& event_time) override;

 private:
  // Callback that is triggered by the ExtensionWebRequestEventRouter on a page
  // load.
  //
  // We don't need to take care of the life time of |bucket|: It is owned by the
  // BucketMapper of our base class in |QuotaLimitHeuristic::bucket_mapper_|. As
  // long as |this| exists, the respective BucketMapper and its bucket will
  // exist as well.
  void OnPageLoad(Bucket* bucket);

  // Flag to prevent that we register more than one call back in-between
  // clearing the cache.
  bool callback_registered_;

  base::WeakPtrFactory<ClearCacheQuotaHeuristic> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ClearCacheQuotaHeuristic);
};

bool ClearCacheQuotaHeuristic::Apply(Bucket* bucket,
                                     const base::TimeTicks& event_time) {
  if (event_time > bucket->expiration())
    bucket->Reset(config(), event_time);

  // Call bucket->DeductToken() on a new page load, this is when
  // webRequest.handlerBehaviorChanged() clears the cache.
  if (!callback_registered_) {
    ExtensionWebRequestEventRouter::GetInstance()->AddCallbackForPageLoad(
        base::Bind(&ClearCacheQuotaHeuristic::OnPageLoad,
                   weak_ptr_factory_.GetWeakPtr(),
                   bucket));
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
  // Argument 0 is the callback, which we don't use here.
  ExtensionWebRequestEventRouter::RequestFilter filter;
  base::DictionaryValue* value = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &value));
  // Failure + an empty error string means a fatal error.
  std::string error;
  EXTENSION_FUNCTION_VALIDATE(filter.InitFromValue(*value, &error) ||
                              !error.empty());
  if (!error.empty())
    return RespondNow(Error(error));

  int extra_info_spec = 0;
  if (HasOptionalArgument(2)) {
    base::ListValue* value = NULL;
    EXTENSION_FUNCTION_VALIDATE(args_->GetList(2, &value));
    EXTENSION_FUNCTION_VALIDATE(
        ExtraInfoSpec::InitFromValue(*value, &extra_info_spec));
  }

  std::string event_name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(3, &event_name));

  std::string sub_event_name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(4, &sub_event_name));

  int web_view_instance_id = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(5, &web_view_instance_id));

  base::WeakPtr<IOThreadExtensionMessageFilter> ipc_sender = ipc_sender_weak();
  int embedder_process_id = ipc_sender ? ipc_sender->render_process_id() : 0;

  const Extension* extension =
      extension_info_map()->extensions().GetByID(extension_id_safe());
  std::string extension_name =
      extension ? extension->name() : extension_id_safe();

  if (!web_view_instance_id) {
    // We check automatically whether the extension has the 'webRequest'
    // permission. For blocking calls we require the additional permission
    // 'webRequestBlocking'.
    if ((extra_info_spec &
         (ExtraInfoSpec::BLOCKING | ExtraInfoSpec::ASYNC_BLOCKING)) &&
        !extension->permissions_data()->HasAPIPermission(
            APIPermission::kWebRequestBlocking)) {
      return RespondNow(Error(keys::kBlockingPermissionRequired));
    }

    // We allow to subscribe to patterns that are broader than the host
    // permissions. E.g., we could subscribe to http://www.example.com/*
    // while having host permissions for http://www.example.com/foo/* and
    // http://www.example.com/bar/*.
    // For this reason we do only a coarse check here to warn the extension
    // developer if they do something obviously wrong.
    // When restrictions are enabled in Public Session, allow all URLs for
    // webRequests initiated by a regular extension.
    if (!(ArePublicSessionRestrictionsEnabled() && extension->is_extension()) &&
        extension->permissions_data()
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
      ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
          profile_id(), extension_id_safe(), extension_name,
          GetEventHistogramValue(event_name), event_name, sub_event_name,
          filter, extra_info_spec, embedder_process_id, web_view_instance_id,
          ipc_sender_weak());
  EXTENSION_FUNCTION_VALIDATE(success);

  helpers::ClearCacheOnNavigation();

  return RespondNow(NoArguments());
}

void WebRequestInternalEventHandledFunction::OnError(
    const std::string& event_name,
    const std::string& sub_event_name,
    uint64_t request_id,
    std::unique_ptr<ExtensionWebRequestEventRouter::EventResponse> response) {
  ExtensionWebRequestEventRouter::GetInstance()->OnEventHandled(
      profile_id(),
      extension_id_safe(),
      event_name,
      sub_event_name,
      request_id,
      response.release());
}

ExtensionFunction::ResponseAction
WebRequestInternalEventHandledFunction::Run() {
  std::string event_name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &event_name));

  std::string sub_event_name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &sub_event_name));

  std::string request_id_str;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(2, &request_id_str));
  uint64_t request_id;
  EXTENSION_FUNCTION_VALIDATE(base::StringToUint64(request_id_str,
                                                   &request_id));

  std::unique_ptr<ExtensionWebRequestEventRouter::EventResponse> response;
  if (HasOptionalArgument(3)) {
    base::DictionaryValue* value = NULL;
    EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(3, &value));

    if (!value->empty()) {
      base::Time install_time =
          extension_info_map()->GetInstallTime(extension_id_safe());
      response.reset(new ExtensionWebRequestEventRouter::EventResponse(
          extension_id_safe(), install_time));
    }

    // In Public Session we restrict everything but "cancel" (except for
    // whitelisted extensions which have no such restrictions).
    if (ArePublicSessionRestrictionsEnabled() &&
        !extensions::IsWhitelistedForPublicSession(extension_id_safe()) &&
        (value->HasKey("redirectUrl") ||
         value->HasKey(keys::kAuthCredentialsKey) ||
         value->HasKey("requestHeaders") ||
         value->HasKey("responseHeaders"))) {
      OnError(event_name, sub_event_name, request_id, std::move(response));
      return RespondNow(Error(keys::kInvalidPublicSessionBlockingResponse));
    }

    if (value->HasKey("cancel")) {
      // Don't allow cancel mixed with other keys.
      if (value->size() != 1) {
        OnError(event_name, sub_event_name, request_id, std::move(response));
        return RespondNow(Error(keys::kInvalidBlockingResponse));
      }

      bool cancel = false;
      EXTENSION_FUNCTION_VALIDATE(value->GetBoolean("cancel", &cancel));
      response->cancel = cancel;
    }

    if (value->HasKey("redirectUrl")) {
      std::string new_url_str;
      EXTENSION_FUNCTION_VALIDATE(value->GetString("redirectUrl",
                                                   &new_url_str));
      response->new_url = GURL(new_url_str);
      if (!response->new_url.is_valid()) {
        OnError(event_name, sub_event_name, request_id, std::move(response));
        return RespondNow(Error(keys::kInvalidRedirectUrl, new_url_str));
      }
    }

    const bool has_request_headers = value->HasKey("requestHeaders");
    const bool has_response_headers = value->HasKey("responseHeaders");
    if (has_request_headers || has_response_headers) {
      if (has_request_headers && has_response_headers) {
        // Allow only one of the keys, not both.
        OnError(event_name, sub_event_name, request_id, std::move(response));
        return RespondNow(Error(keys::kInvalidHeaderKeyCombination));
      }

      base::ListValue* headers_value = NULL;
      std::unique_ptr<net::HttpRequestHeaders> request_headers;
      std::unique_ptr<helpers::ResponseHeaders> response_headers;
      if (has_request_headers) {
        request_headers.reset(new net::HttpRequestHeaders());
        EXTENSION_FUNCTION_VALIDATE(value->GetList(keys::kRequestHeadersKey,
                                                   &headers_value));
      } else {
        response_headers.reset(new helpers::ResponseHeaders());
        EXTENSION_FUNCTION_VALIDATE(value->GetList(keys::kResponseHeadersKey,
                                                   &headers_value));
      }

      for (size_t i = 0; i < headers_value->GetSize(); ++i) {
        base::DictionaryValue* header_value = NULL;
        std::string name;
        std::string value;
        EXTENSION_FUNCTION_VALIDATE(
            headers_value->GetDictionary(i, &header_value));
        if (!FromHeaderDictionary(header_value, &name, &value)) {
          std::string serialized_header;
          base::JSONWriter::Write(*header_value, &serialized_header);
          OnError(event_name, sub_event_name, request_id, std::move(response));
          return RespondNow(Error(keys::kInvalidHeader, serialized_header));
        }
        if (!net::HttpUtil::IsValidHeaderName(name)) {
          OnError(event_name, sub_event_name, request_id, std::move(response));
          return RespondNow(Error(keys::kInvalidHeaderName));
        }
        if (!net::HttpUtil::IsValidHeaderValue(value)) {
          OnError(event_name, sub_event_name, request_id, std::move(response));
          return RespondNow(Error(keys::kInvalidHeaderValue, name));
        }
        if (has_request_headers)
          request_headers->SetHeader(name, value);
        else
          response_headers->push_back(helpers::ResponseHeader(name, value));
      }
      if (has_request_headers)
        response->request_headers = std::move(request_headers);
      else
        response->response_headers = std::move(response_headers);
    }

    if (value->HasKey(keys::kAuthCredentialsKey)) {
      base::DictionaryValue* credentials_value = NULL;
      EXTENSION_FUNCTION_VALIDATE(value->GetDictionary(
          keys::kAuthCredentialsKey,
          &credentials_value));
      base::string16 username;
      base::string16 password;
      EXTENSION_FUNCTION_VALIDATE(
          credentials_value->GetString(keys::kUsernameKey, &username));
      EXTENSION_FUNCTION_VALIDATE(
          credentials_value->GetString(keys::kPasswordKey, &password));
      response->auth_credentials.reset(
          new net::AuthCredentials(username, password));
    }
  }

  ExtensionWebRequestEventRouter::GetInstance()->OnEventHandled(
      profile_id(), extension_id_safe(), event_name, sub_event_name, request_id,
      response.release());

  return RespondNow(NoArguments());
}

void WebRequestHandlerBehaviorChangedFunction::GetQuotaLimitHeuristics(
    QuotaLimitHeuristics* heuristics) const {
  QuotaLimitHeuristic::Config config = {
      // See web_request.json for current value.
      web_request::MAX_HANDLER_BEHAVIOR_CHANGED_CALLS_PER_10_MINUTES,
      base::TimeDelta::FromMinutes(10)};
  QuotaLimitHeuristic::BucketMapper* bucket_mapper =
      new QuotaLimitHeuristic::SingletonBucketMapper();
  heuristics->push_back(
      std::make_unique<ClearCacheQuotaHeuristic>(config, bucket_mapper));
}

void WebRequestHandlerBehaviorChangedFunction::OnQuotaExceeded(
    const std::string& violation_error) {
  // Post warning message.
  WarningSet warnings;
  warnings.insert(
      Warning::CreateRepeatedCacheFlushesWarning(extension_id_safe()));
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::Bind(&WarningService::NotifyWarningsOnUI, profile_id(), warnings));

  // Continue gracefully.
  RunWithValidation()->Execute();
}

ExtensionFunction::ResponseAction
WebRequestHandlerBehaviorChangedFunction::Run() {
  helpers::ClearCacheOnNavigation();
  return RespondNow(NoArguments());
}

ExtensionWebRequestEventRouter::EventListener::ID::ID(
    void* browser_context,
    const std::string& extension_id,
    const std::string& sub_event_name,
    int embedder_process_id,
    int web_view_instance_id)
    : browser_context(browser_context),
      extension_id(extension_id),
      sub_event_name(sub_event_name),
      embedder_process_id(embedder_process_id),
      web_view_instance_id(web_view_instance_id) {}

bool ExtensionWebRequestEventRouter::EventListener::ID::LooselyMatches(
    const ID& that) const {
  if (web_view_instance_id == 0 && that.web_view_instance_id == 0) {
    // Since EventListeners are segmented by browser_context, check that
    // last, as it is exceedingly unlikely to be different.
    return extension_id == that.extension_id &&
           sub_event_name == that.sub_event_name &&
           browser_context == that.browser_context;
  }

  return *this == that;
}

bool ExtensionWebRequestEventRouter::EventListener::ID::operator==(
    const ID& that) const {
  // Since EventListeners are segmented by browser_context, check that
  // last, as it is exceedingly unlikely to be different.
  return extension_id == that.extension_id &&
         sub_event_name == that.sub_event_name &&
         web_view_instance_id == that.web_view_instance_id &&
         embedder_process_id == that.embedder_process_id &&
         browser_context == that.browser_context;
}

}  // namespace extensions
