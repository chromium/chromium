// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_api.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/cxx20_erase_vector.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/api/activity_log/web_request_constants.h"
#include "extensions/browser/api/declarative/rules_registry_service.h"
#include "extensions/browser/api/declarative_net_request/request_action.h"
#include "extensions/browser/api/declarative_net_request/rules_monitor_service.h"
#include "extensions/browser/api/declarative_webrequest/request_stage.h"
#include "extensions/browser/api/declarative_webrequest/webrequest_constants.h"
#include "extensions/browser/api/declarative_webrequest/webrequest_rules_registry.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/web_request/permission_helper.h"
#include "extensions/browser/api/web_request/web_request_api_constants.h"
#include "extensions/browser/api/web_request/web_request_api_helpers.h"
#include "extensions/browser/api/web_request/web_request_event_details.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/browser/api/web_request/web_request_proxying_url_loader_factory.h"
#include "extensions/browser/api/web_request/web_request_proxying_websocket.h"
#include "extensions/browser/api/web_request/web_request_proxying_webtransport.h"
#include "extensions/browser/api/web_request/web_request_resource_type.h"
#include "extensions/browser/api/web_request/web_request_time_tracker.h"
#include "extensions/browser/api_activity_monitor.h"
#include "extensions/browser/browser_frame_context_data.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_navigation_ui_data.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/guest_view/guest_view_events.h"
#include "extensions/browser/guest_view/web_view/web_view_constants.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/warning_service.h"
#include "extensions/browser/warning_set.h"
#include "extensions/common/api/web_request.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/identifiability_metrics.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/auth.h"
#include "net/base/net_errors.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_util.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/web_transport.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using content::BrowserThread;
using extension_web_request_api_helpers::ExtraInfoSpec;
using extensions::mojom::APIPermissionID;

namespace activity_log = activity_log_web_request_constants;
namespace helpers = extension_web_request_api_helpers;
namespace keys = extension_web_request_api_constants;
using URLLoaderFactoryType =
    content::ContentBrowserClient::URLLoaderFactoryType;
using DNRRequestAction = extensions::declarative_net_request::RequestAction;

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
  TRACE_EVENT1("extensions", "WebRequestAction", "action", action);
}

// Returns the corresponding EventTypes for the given |event_name|. If
// |event_name| is an invalid event, returns EventTypes::kInvalidEvent.
ExtensionWebRequestEventRouter::EventTypes GetEventTypeFromEventName(
    base::StringPiece event_name) {
  static base::NoDestructor<const base::flat_map<
      base::StringPiece, ExtensionWebRequestEventRouter::EventTypes>>
      kRequestStageMap(
          {{keys::kOnBeforeRequest,
            ExtensionWebRequestEventRouter::kOnBeforeRequest},
           {keys::kOnBeforeSendHeaders,
            ExtensionWebRequestEventRouter::kOnBeforeSendHeaders},
           {keys::kOnSendHeaders,
            ExtensionWebRequestEventRouter::kOnSendHeaders},
           {keys::kOnHeadersReceived,
            ExtensionWebRequestEventRouter::kOnHeadersReceived},
           {keys::kOnBeforeRedirect,
            ExtensionWebRequestEventRouter::kOnBeforeRedirect},
           {keys::kOnAuthRequired,
            ExtensionWebRequestEventRouter::kOnAuthRequired},
           {keys::kOnResponseStarted,
            ExtensionWebRequestEventRouter::kOnResponseStarted},
           {keys::kOnErrorOccurred,
            ExtensionWebRequestEventRouter::kOnErrorOccurred},
           {keys::kOnCompleted, ExtensionWebRequestEventRouter::kOnCompleted}});

  DCHECK_EQ(kRequestStageMap->size(), std::size(kWebRequestEvents));

  static const size_t kWebRequestEventPrefixLen =
      strlen(kWebRequestEventPrefix);
  static const size_t kWebViewEventPrefixLen =
      strlen(webview::kWebViewEventPrefix);

  // Canonicalize the |event_name| to the request stage.
  if (base::StartsWith(event_name, kWebRequestEventPrefix))
    event_name.remove_prefix(kWebRequestEventPrefixLen);
  else if (base::StartsWith(event_name, webview::kWebViewEventPrefix))
    event_name.remove_prefix(kWebViewEventPrefixLen);
  else
    return ExtensionWebRequestEventRouter::kInvalidEvent;

  auto it = kRequestStageMap->find(event_name);
  if (it == kRequestStageMap->end())
    return ExtensionWebRequestEventRouter::kInvalidEvent;

  return it->second;
}

bool IsWebRequestEvent(base::StringPiece event_name) {
  return GetEventTypeFromEventName(event_name) !=
         ExtensionWebRequestEventRouter::kInvalidEvent;
}

// Returns whether |request| has been triggered by an extension enabled in
// |context|.
bool IsRequestFromExtension(const WebRequestInfo& request,
                            content::BrowserContext* context) {
  if (request.render_process_id == -1)
    return false;

  const std::set<std::string> extension_ids =
      ProcessMap::Get(context)->GetExtensionsInProcess(
          request.render_process_id);
  if (extension_ids.empty())
    return false;

  // Treat hosted apps as normal web pages (crbug.com/526413).
  for (const std::string& extension_id : extension_ids) {
    const Extension* extension =
        ExtensionRegistry::Get(context)->enabled_extensions().GetByID(
            extension_id);
    if (extension && !extension->is_hosted_app())
      return true;
  }
  return false;
}

// Converts a HttpHeaders dictionary to a |name|, |value| pair. Returns
// true if successful.
bool FromHeaderDictionary(const base::Value::Dict& header_value,
                          std::string* name,
                          std::string* out_value) {
  const std::string* name_ptr = header_value.FindString(keys::kHeaderNameKey);
  if (!name)
    return false;
  *name = *name_ptr;

  // We require either a "value" or a "binaryValue" entry.
  const base::Value* value = header_value.Find(keys::kHeaderValueKey);
  const base::Value* binary_value =
      header_value.Find(keys::kHeaderBinaryValueKey);
  if (!((value != nullptr) ^ (binary_value != nullptr))) {
    return false;
  }

  if (value) {
    if (!value->is_string())
      return false;
    *out_value = value->GetString();
  } else if (binary_value) {
    if (!binary_value->is_list() ||
        !helpers::CharListToString(binary_value->GetList(), out_value)) {
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
    content::BrowserContext* browser_context,
    const std::string& extension_id,
    bool is_web_view_guest,
    int web_view_instance_id,
    std::unique_ptr<WebRequestEventDetails> event_details) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!ExtensionsBrowserClient::Get()->IsValidContext(browser_context))
    return;

  base::Value::List event_args;
  event_args.Append(event_details->GetAndClearDict());

  EventRouter* event_router = EventRouter::Get(browser_context);

  mojom::EventFilteringInfoPtr event_filtering_info =
      mojom::EventFilteringInfo::New();

  events::HistogramValue histogram_value = events::UNKNOWN;
  std::string event_name;
  // The instance ID uniquely identifies a <webview> instance within an embedder
  // process. We use a filter here so that only event listeners for a particular
  // <webview> will fire.
  if (is_web_view_guest) {
    event_filtering_info->has_instance_id = true;
    event_filtering_info->instance_id = web_view_instance_id;
    histogram_value = events::WEB_VIEW_INTERNAL_ON_MESSAGE;
    event_name = webview::kEventMessage;
  } else {
    histogram_value = events::DECLARATIVE_WEB_REQUEST_ON_MESSAGE;
    event_name = declarative_keys::kOnMessage;
  }

  auto event = std::make_unique<Event>(
      histogram_value, event_name, std::move(event_args), browser_context,
      GURL(), EventRouter::USER_GESTURE_UNKNOWN,
      std::move(event_filtering_info));
  event_router->DispatchEventToExtension(extension_id, std::move(event));
}

// Helper to dispatch the "onActionIgnored" event.
void NotifyIgnoredActionsOnUI(
    content::BrowserContext* browser_context,
    uint64_t request_id,
    extension_web_request_api_helpers::IgnoredActions ignored_actions) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!ExtensionsBrowserClient::Get()->IsValidContext(browser_context))
    return;

  EventRouter* event_router = EventRouter::Get(browser_context);
  web_request::OnActionIgnored::Details details;
  details.request_id = base::NumberToString(request_id);
  details.action = web_request::IgnoredActionType::kNone;
  for (const auto& ignored_action : ignored_actions) {
    DCHECK_NE(web_request::IgnoredActionType::kNone,
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
  static_assert(std::size(kWebRequestEvents) == std::size(values_and_names),
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
bool ShouldHideEvent(content::BrowserContext* browser_context,
                     const WebRequestInfo& request) {
  return (!browser_context ||
          WebRequestPermissions::HideRequest(
              PermissionHelper::Get(browser_context), request));
}

// Returns event details for a given request.
std::unique_ptr<WebRequestEventDetails> CreateEventDetails(
    const WebRequestInfo& request,
    int extra_info_spec) {
  return std::make_unique<WebRequestEventDetails>(request, extra_info_spec);
}

// Checks whether the extension has any permissions that would intercept or
// modify network requests.
bool HasAnyWebRequestPermissions(const Extension* extension) {
  static const APIPermissionID kWebRequestPermissions[] = {
      APIPermissionID::kWebRequest,
      APIPermissionID::kWebRequestBlocking,
      APIPermissionID::kDeclarativeWebRequest,
      APIPermissionID::kDeclarativeNetRequest,
      APIPermissionID::kDeclarativeNetRequestWithHostAccess,
      APIPermissionID::kWebView,
  };

  const PermissionsData* permissions = extension->permissions_data();
  for (auto permission : kWebRequestPermissions) {
    if (permissions->HasAPIPermission(permission))
      return true;
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

void LogEventListenerFlag(WebRequestEventListenerFlag flag) {
  UMA_HISTOGRAM_ENUMERATION("Extensions.WebRequest.EventListenerFlag", flag);
}

void RecordAddEventListenerUMAs(int extra_info_spec) {
  LogEventListenerFlag(WebRequestEventListenerFlag::kTotal);
  if (extra_info_spec == 0) {
    LogEventListenerFlag(WebRequestEventListenerFlag::kNone);
    return;
  }

  if (extra_info_spec & ExtraInfoSpec::REQUEST_HEADERS)
    LogEventListenerFlag(WebRequestEventListenerFlag::kRequestHeaders);
  if (extra_info_spec & ExtraInfoSpec::RESPONSE_HEADERS)
    LogEventListenerFlag(WebRequestEventListenerFlag::kResponseHeaders);
  if (extra_info_spec & ExtraInfoSpec::BLOCKING)
    LogEventListenerFlag(WebRequestEventListenerFlag::kBlocking);
  if (extra_info_spec & ExtraInfoSpec::ASYNC_BLOCKING)
    LogEventListenerFlag(WebRequestEventListenerFlag::kAsyncBlocking);
  if (extra_info_spec & ExtraInfoSpec::REQUEST_BODY)
    LogEventListenerFlag(WebRequestEventListenerFlag::kRequestBody);
  if (extra_info_spec & ExtraInfoSpec::EXTRA_HEADERS)
    LogEventListenerFlag(WebRequestEventListenerFlag::kExtraHeaders);
}

// Helper to record a matched DNR action in RulesetManager's ActionTracker.
void OnDNRActionMatched(content::BrowserContext* browser_context,
                        const WebRequestInfo& request,
                        const DNRRequestAction& action) {
  if (action.tracked)
    return;

  declarative_net_request::ActionTracker& action_tracker =
      declarative_net_request::RulesMonitorService::Get(browser_context)
          ->action_tracker();

  action_tracker.OnRuleMatched(action, request);
  action.tracked = true;
}

}  // namespace

void WebRequestAPI::Proxy::HandleAuthRequest(
    const net::AuthChallengeInfo& auth_info,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    int32_t request_id,
    AuthRequestCallback callback) {
  // Default implementation cancels the request.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), absl::nullopt,
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
        FROM_HERE, base::BindOnce(std::move(callback), absl::nullopt,
                                  false /* should_cancel */));
    return;
  }

  proxy->HandleAuthRequest(auth_info, std::move(response_headers),
                           request_id.request_id, std::move(callback));
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
  for (size_t i = 0; i < std::size(kWebRequestEvents); ++i) {
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
  proxies_.reset();
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
  extensions::ExtensionRegistry::Get(browser_context_)->RemoveObserver(this);
  ExtensionWebRequestEventRouter::GetInstance()->OnBrowserContextShutdown(
      browser_context_);
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
    remove_listener = base::BindOnce(
        &ExtensionWebRequestEventRouter::RemoveLazyListener,
        base::Unretained(ExtensionWebRequestEventRouter::GetInstance()),
        browser_context_, details.extension_id, sub_event_name);
  } else {
    // This was an active listener registration.
    auto update_type =
        ExtensionWebRequestEventRouter::ListenerUpdateType::kRemove;
    if (details.service_worker_version_id !=
        blink::mojom::kInvalidServiceWorkerVersionId) {
      // This was a listener removed for a service worker, but it wasn't the
      // lazy listener registration. In this case, we only deactivate the
      // listener (rather than removing it).
      update_type =
          ExtensionWebRequestEventRouter::ListenerUpdateType::kDeactivate;
    }

    remove_listener = base::BindOnce(
        &ExtensionWebRequestEventRouter::UpdateActiveListener,
        base::Unretained(ExtensionWebRequestEventRouter::GetInstance()),
        update_type,
        ExtensionWebRequestEventRouter::GetBrowserContextID(
            details.browser_context.get()),
        details.extension_id, sub_event_name, details.worker_thread_id,
        details.service_worker_version_id);
  }

  // This PostTask is necessary even though we are already on the UI thread to
  // allow cases where blocking listeners remove themselves inside the handler.
  // This Unretained is safe because the ExtensionWebRequestEventRouter
  // singleton is leaked.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(remove_listener));
}

bool WebRequestAPI::MaybeProxyURLLoaderFactory(
    content::BrowserContext* browser_context,
    content::RenderFrameHost* frame,
    int render_process_id,
    URLLoaderFactoryType type,
    absl::optional<int64_t> navigation_id,
    ukm::SourceIdObj ukm_source_id,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>* factory_receiver,
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
        header_client,
    const url::Origin& request_initiator) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!MayHaveProxies()) {
    bool skip_proxy = true;
    // There are a few internal WebUIs that use WebView tag that are allowlisted
    // for webRequest.
    if (WebViewGuest::IsGuest(frame)) {
      content::RenderFrameHost* embedder =
          frame->GetOutermostMainFrameOrEmbedder();
      const auto& embedder_url = embedder->GetLastCommittedURL();
      if (embedder_url.SchemeIs(content::kChromeUIScheme)) {
        auto* feature = FeatureProvider::GetAPIFeature("webRequestInternal");
        if (feature
                ->IsAvailableToContext(
                    nullptr, Feature::WEBUI_CONTEXT, embedder_url,
                    util::GetBrowserContextId(browser_context),
                    BrowserFrameContextData(frame))
                .is_available()) {
          skip_proxy = false;
        }
      }
    }

    // Create a proxy URLLoader even when there is no CRX
    // installed with webRequest permissions. This allows the extension
    // requests to be intercepted for CRX telemetry service if enabled.
    // TODO(zackhan): This is here for the current implementation, but if it's
    // expanded, it should live somewhere else so that we don't have to create
    // a full proxy just for telemetry.
    const std::string& request_scheme = request_initiator.scheme();
    if (extensions::kExtensionScheme == request_scheme &&
        ExtensionsBrowserClient::Get()->IsExtensionTelemetryServiceEnabled(
            browser_context) &&
        base::FeatureList::IsEnabled(
            safe_browsing::kExtensionTelemetryReportContactedHosts)) {
      skip_proxy = false;
    }
    if (skip_proxy)
      return false;
  }

  auto proxied_receiver = std::move(*factory_receiver);
  mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory_remote;
  *factory_receiver = target_factory_remote.InitWithNewPipeAndPassReceiver();

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
  if (header_client)
    header_client_receiver = header_client->InitWithNewPipeAndPassReceiver();

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
      std::move(navigation_id), ukm_source_id, std::move(proxied_receiver),
      std::move(target_factory_remote), std::move(header_client_receiver),
      proxies_.get(), type);
  return true;
}

bool WebRequestAPI::MaybeProxyAuthRequest(
    content::BrowserContext* browser_context,
    const net::AuthChallengeInfo& auth_info,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    const content::GlobalRequestID& request_id,
    bool is_main_frame,
    AuthRequestCallback callback) {
  if (!MayHaveProxies())
    return false;

  content::GlobalRequestID proxied_request_id = request_id;
  if (is_main_frame)
    proxied_request_id.child_id = -1;

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
    const absl::optional<std::string>& user_agent,
    mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
        handshake_client) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(MayHaveProxies() || MayHaveWebsocketProxiesForExtensionTelemetry());

  const bool has_extra_headers =
      ExtensionWebRequestEventRouter::GetInstance()->HasAnyExtraHeadersListener(
          frame->GetProcess()->GetBrowserContext());

  const ukm::SourceIdObj& ukm_source_id =
      ukm::SourceIdObj::FromInt64(frame->GetPageUkmSourceId());

  WebRequestProxyingWebSocket::StartProxying(
      std::move(factory), url, site_for_cookies, user_agent,
      std::move(handshake_client), has_extra_headers,
      frame->GetProcess()->GetID(), frame->GetRoutingID(), ukm_source_id,
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
    std::move(callback).Run(std::move(handshake_client), absl::nullopt);
    return;
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
  return ExtensionsBrowserClient::Get()->IsExtensionTelemetryServiceEnabled(
             browser_context_) &&
         base::FeatureList::IsEnabled(
             safe_browsing::kExtensionTelemetryReportContactedHosts) &&
         base::FeatureList::IsEnabled(
             safe_browsing::
                 kExtensionTelemetryReportHostsContactedViaWebSocket);
}

bool WebRequestAPI::HasExtraHeadersListenerForTesting() {
  return ExtensionWebRequestEventRouter::GetInstance()
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
}

// Represents a single unique listener to an event, along with whatever filter
// parameters and extra_info_spec were specified at the time the listener was
// added.
// NOTE(benjhayden) New APIs should not use this sub_event_name trick! It does
// not play well with event pages. See downloads.onDeterminingFilename and
// ExtensionDownloadsEventRouter for an alternative approach.
ExtensionWebRequestEventRouter::EventListener::EventListener(ID id) : id(id) {}
ExtensionWebRequestEventRouter::EventListener::~EventListener() = default;

// Contains info about requests that are blocked waiting for a response from
// an extension.
struct ExtensionWebRequestEventRouter::BlockedRequest {
  BlockedRequest() = default;

  // Information about the request that is being blocked. Not owned.
  raw_ptr<const WebRequestInfo> request = nullptr;

  // Whether the request originates from an incognito tab.
  bool is_incognito = false;

  // The event that we're currently blocked on.
  EventTypes event = kInvalidEvent;

  // The number of event handlers that we are awaiting a response from.
  int num_handlers_blocking = 0;

  // The callback to call when we get a response from all event handlers.
  net::CompletionOnceCallback callback;

  // The callback to invoke for onBeforeSendHeaders. If
  // |before_send_headers_callback.is_null()| is false, |callback| must be NULL.
  // Only valid for OnBeforeSendHeaders.
  BeforeSendHeadersCallback before_send_headers_callback;

  // The callback to invoke for auth. If |auth_callback.is_null()| is false,
  // |callback| must be NULL.
  // Only valid for OnAuthRequired.
  AuthCallback auth_callback;

  // If non-empty, this contains the auth credentials that may be filled in.
  // Only valid for OnAuthRequired.
  raw_ptr<net::AuthCredentials> auth_credentials = nullptr;

  // If non-empty, this contains the new URL that the request will redirect to.
  // Only valid for OnBeforeRequest and OnHeadersReceived.
  raw_ptr<GURL> new_url = nullptr;

  // The request headers that will be issued along with this request. Only valid
  // for OnBeforeSendHeaders.
  raw_ptr<net::HttpRequestHeaders> request_headers = nullptr;

  // The response headers that were received from the server. Only valid for
  // OnHeadersReceived.
  scoped_refptr<const net::HttpResponseHeaders> original_response_headers;

  // Location where to override response headers. Only valid for
  // OnHeadersReceived.
  raw_ptr<scoped_refptr<net::HttpResponseHeaders>> override_response_headers =
      nullptr;

  // Time the request was paused. Used for logging purposes.
  base::Time blocking_time;

  // Changes requested by extensions.
  helpers::EventResponseDeltas response_deltas;
};

bool ExtensionWebRequestEventRouter::RequestFilter::InitFromValue(
    const base::Value::Dict& value,
    std::string* error) {
  if (!value.Find("urls"))
    return false;

  for (const auto dict_item : value) {
    if (dict_item.first == "urls" && dict_item.second.is_list()) {
      for (const auto& item : dict_item.second.GetList()) {
        std::string url;
        URLPattern pattern(URLPattern::SCHEME_HTTP | URLPattern::SCHEME_HTTPS |
                           URLPattern::SCHEME_FTP | URLPattern::SCHEME_FILE |
                           URLPattern::SCHEME_EXTENSION |
                           URLPattern::SCHEME_WS | URLPattern::SCHEME_WSS |
                           URLPattern::SCHEME_UUID_IN_PACKAGE);
        if (item.is_string())
          url = item.GetString();
        // Parse will fail on an empty url, so we don't need to distinguish
        // between `item` not being a string and `item` being an empty string.
        if (url.empty() ||
            pattern.Parse(url) != URLPattern::ParseResult::kSuccess) {
          *error = ErrorUtils::FormatErrorMessage(
              keys::kInvalidRequestFilterUrl, url);
          return false;
        }
        urls.AddPattern(pattern);
      }
    } else if (dict_item.first == "types" && dict_item.second.is_list()) {
      for (const auto& type : dict_item.second.GetList()) {
        std::string type_str;
        if (type.is_string())
          type_str = type.GetString();
        types.push_back(WebRequestResourceType::OTHER);
        if (type_str.empty() ||
            !ParseWebRequestResourceType(type_str, &types.back())) {
          return false;
        }
      }
    } else if (dict_item.first == "tabId" && dict_item.second.is_int()) {
      tab_id = dict_item.second.GetInt();
    } else if (dict_item.first == "windowId" && dict_item.second.is_int()) {
      window_id = dict_item.second.GetInt();
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
    : tab_id(-1), window_id(-1) {}
ExtensionWebRequestEventRouter::RequestFilter::~RequestFilter() = default;

ExtensionWebRequestEventRouter::RequestFilter::RequestFilter(
    RequestFilter&& other) = default;
ExtensionWebRequestEventRouter::RequestFilter&
ExtensionWebRequestEventRouter::RequestFilter::operator=(
    RequestFilter&& other) = default;

ExtensionWebRequestEventRouter::BrowserContextData::BrowserContextData() =
    default;
ExtensionWebRequestEventRouter::BrowserContextData::BrowserContextData(
    BrowserContextData&&) = default;
ExtensionWebRequestEventRouter::BrowserContextData::~BrowserContextData() =
    default;
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
    content::BrowserContext* browser_context,
    int rules_registry_id,
    scoped_refptr<WebRequestRulesRegistry> rules_registry) {
  RulesRegistryKey key(BrowserContextID(browser_context), rules_registry_id);
  if (rules_registry.get()) {
    rules_registries_[key] = rules_registry;
  } else {
    rules_registries_.erase(key);
  }
}

int ExtensionWebRequestEventRouter::OnBeforeRequest(
    content::BrowserContext* browser_context,
    WebRequestInfo* request,
    net::CompletionOnceCallback callback,
    GURL* new_url,
    bool* should_collapse_initiator) {
  DCHECK(should_collapse_initiator);

  if (ShouldHideEvent(browser_context, *request)) {
    request->dnr_actions = std::vector<DNRRequestAction>();
    return net::OK;
  }

  if (IsPageLoad(*request))
    NotifyPageLoad();

  bool has_listener = false;
  for (const auto& kv :
       data_[GetBrowserContextID(browser_context)].active_listeners) {
    if (!kv.second.empty()) {
      has_listener = true;
      break;
    }
  }
  request_time_tracker_->LogRequestStartTime(
      request->id, base::TimeTicks::Now(), has_listener,
      HasExtraHeadersListenerForRequest(browser_context, request));

  const bool is_incognito_context = browser_context->IsOffTheRecord();

  // CRX requests information can be intercepted here.
  // May be null for browser-initiated requests such as navigations.
  if (request->initiator) {
    const std::string& scheme = request->initiator->scheme();
    const std::string& extension_id = request->initiator->host();
    const GURL& request_url = request->url;
    if (scheme == extensions::kExtensionScheme) {
      ExtensionsBrowserClient::Get()->NotifyExtensionRemoteHostContacted(
          browser_context, extension_id, request_url);
    }
  }

  // Whether to initialized |blocked_requests_|.
  bool initialize_blocked_requests = false;

  initialize_blocked_requests |= ProcessDeclarativeRules(
      browser_context, web_request::OnBeforeRequest::kEventName, request,
      ON_BEFORE_REQUEST, nullptr);

  int extra_info_spec = 0;
  RawListeners listeners = GetMatchingListeners(
      browser_context, web_request::OnBeforeRequest::kEventName, request,
      &extra_info_spec);
  if (!listeners.empty() && !GetAndSetSignaled(request->id, kOnBeforeRequest)) {
    std::unique_ptr<WebRequestEventDetails> event_details(
        CreateEventDetails(*request, extra_info_spec));
    event_details->SetRequestBody(request);

    request_time_tracker_->LogBeforeRequestDispatchTime(request->id,
                                                        base::TimeTicks::Now());

    initialize_blocked_requests |= DispatchEvent(
        browser_context, request, listeners, std::move(event_details));
  }

  // Handle Declarative Net Request API rules. In case the request is blocked or
  // redirected, we un-block the request and ignore any subsequent responses
  // from webRequestBlocking listeners. Note: We don't remove the request from
  // the |EventListener::blocked_requests| set of any blocking listeners it was
  // dispatched to, since the listener's response will be ignored in
  // |DecrementBlockCount| anyway.

  // Only checking the rules in the OnBeforeRequest stage works, since the rules
  // currently only depend on the request url, initiator and resource type,
  // which should stay the same during the diffierent network request stages. A
  // redirect should cause another OnBeforeRequest call.
  declarative_net_request::RulesetManager* ruleset_manager =
      declarative_net_request::RulesMonitorService::Get(browser_context)
          ->ruleset_manager();

  if (ruleset_manager->has_rulesets()) {
    request_time_tracker_->LogBeforeRequestDNRStartTime(request->id,
                                                        base::TimeTicks::Now());

    auto record_completion_time = [](ExtensionWebRequestTimeTracker* tracker,
                                     int64_t request_id) {
      tracker->LogBeforeRequestDNRCompletionTime(request_id,
                                                 base::TimeTicks::Now());
    };

    const std::vector<DNRRequestAction>& actions =
        ruleset_manager->EvaluateRequest(*request, is_incognito_context);
    base::ScopedClosureRunner scoped_timer;
    if (!actions.empty()) {
      // We only record completion time if there's at least one relevant rule.
      // Otherwise, we'd record evaluation for every request even if the user
      // only had a single rule added.
      scoped_timer = base::ScopedClosureRunner(base::BindOnce(
          record_completion_time, request_time_tracker_.get(), request->id));
    }

    for (const auto& action : actions) {
      switch (action.type) {
        case DNRRequestAction::Type::BLOCK:
          ClearPendingCallbacks(*request);
          DCHECK_EQ(1u, actions.size());
          OnDNRActionMatched(browser_context, *request, action);
          RecordNetworkRequestBlocked(request->ukm_source_id,
                                      action.extension_id);
          return net::ERR_BLOCKED_BY_CLIENT;
        case DNRRequestAction::Type::COLLAPSE:
          ClearPendingCallbacks(*request);
          DCHECK_EQ(1u, actions.size());
          OnDNRActionMatched(browser_context, *request, action);
          *should_collapse_initiator = true;
          RecordNetworkRequestBlocked(request->ukm_source_id,
                                      action.extension_id);
          return net::ERR_BLOCKED_BY_CLIENT;
        case DNRRequestAction::Type::ALLOW:
        case DNRRequestAction::Type::ALLOW_ALL_REQUESTS:
          DCHECK_EQ(1u, actions.size());
          OnDNRActionMatched(browser_context, *request, action);
          break;
        case DNRRequestAction::Type::REDIRECT:
        case DNRRequestAction::Type::UPGRADE:
          ClearPendingCallbacks(*request);
          DCHECK_EQ(1u, actions.size());
          DCHECK(action.redirect_url);
          OnDNRActionMatched(browser_context, *request, action);
          *new_url = action.redirect_url.value();
          return net::OK;
        case DNRRequestAction::Type::MODIFY_HEADERS:
          // Unlike other actions, allow web request extensions to intercept
          // the request here. The headers will be modified during subsequent
          // request stages.
          DCHECK(base::ranges::all_of(
              *request->dnr_actions, [](const auto& action) {
                return action.type == DNRRequestAction::Type::MODIFY_HEADERS;
              }));
          break;
      }
    }
  } else {
    // Later methods require `dnr_actions` to be populated; give it an empty
    // set.
    request->dnr_actions = std::vector<DNRRequestAction>();
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
    content::BrowserContext* browser_context,
    const WebRequestInfo* request,
    BeforeSendHeadersCallback callback,
    net::HttpRequestHeaders* headers) {
  if (ShouldHideEvent(browser_context, *request))
    return net::OK;

  bool initialize_blocked_requests = false;

  initialize_blocked_requests |=
      ProcessDeclarativeRules(browser_context, keys::kOnBeforeSendHeadersEvent,
                              request, ON_BEFORE_SEND_HEADERS, nullptr);

  DCHECK(request->dnr_actions);
  initialize_blocked_requests |= base::ranges::any_of(
      *request->dnr_actions, [](const DNRRequestAction& action) {
        return action.type == DNRRequestAction::Type::MODIFY_HEADERS &&
               !action.request_headers_to_modify.empty();
      });

  int extra_info_spec = 0;
  RawListeners listeners =
      GetMatchingListeners(browser_context, keys::kOnBeforeSendHeadersEvent,
                           request, &extra_info_spec);
  if (!listeners.empty() &&
      !GetAndSetSignaled(request->id, kOnBeforeSendHeaders)) {
    std::unique_ptr<WebRequestEventDetails> event_details(
        CreateEventDetails(*request, extra_info_spec));
    event_details->SetRequestHeaders(*headers);

    initialize_blocked_requests |= DispatchEvent(
        browser_context, request, listeners, std::move(event_details));
  }

  if (!initialize_blocked_requests)
    return net::OK;  // Nobody saw a reason for modifying the request.

  BlockedRequest& blocked_request = blocked_requests_[request->id];
  blocked_request.event = kOnBeforeSendHeaders;
  blocked_request.is_incognito |= browser_context->IsOffTheRecord();
  blocked_request.request = request;
  blocked_request.before_send_headers_callback = std::move(callback);
  blocked_request.request_headers = headers;

  if (blocked_request.num_handlers_blocking == 0) {
    // If there are no blocking handlers, only the declarative rules tried
    // to modify the request and we can respond synchronously.
    return ExecuteDeltas(browser_context, request, false /* call_callback*/);
  }
  return net::ERR_IO_PENDING;
}

void ExtensionWebRequestEventRouter::OnSendHeaders(
    content::BrowserContext* browser_context,
    const WebRequestInfo* request,
    const net::HttpRequestHeaders& headers) {
  if (ShouldHideEvent(browser_context, *request))
    return;

  if (GetAndSetSignaled(request->id, kOnSendHeaders))
    return;

  ClearSignaled(request->id, kOnBeforeRedirect);

  int extra_info_spec = 0;
  RawListeners listeners = GetMatchingListeners(
      browser_context, keys::kOnSendHeadersEvent, request, &extra_info_spec);
  if (listeners.empty())
    return;

  std::unique_ptr<WebRequestEventDetails> event_details(
      CreateEventDetails(*request, extra_info_spec));
  event_details->SetRequestHeaders(headers);

  DispatchEvent(browser_context, request, listeners, std::move(event_details));
}

int ExtensionWebRequestEventRouter::OnHeadersReceived(
    content::BrowserContext* browser_context,
    const WebRequestInfo* request,
    net::CompletionOnceCallback callback,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    GURL* preserve_fragment_on_redirect_url) {
  if (ShouldHideEvent(browser_context, *request))
    return net::OK;

  bool initialize_blocked_requests = false;

  DCHECK(request->dnr_actions);
  initialize_blocked_requests |= base::ranges::any_of(
      *request->dnr_actions, [](const DNRRequestAction& action) {
        return action.type == DNRRequestAction::Type::MODIFY_HEADERS &&
               !action.response_headers_to_modify.empty();
      });

  initialize_blocked_requests |= ProcessDeclarativeRules(
      browser_context, keys::kOnHeadersReceivedEvent, request,
      ON_HEADERS_RECEIVED, original_response_headers);

  int extra_info_spec = 0;
  RawListeners listeners =
      GetMatchingListeners(browser_context, keys::kOnHeadersReceivedEvent,
                           request, &extra_info_spec);

  if (!listeners.empty() &&
      !GetAndSetSignaled(request->id, kOnHeadersReceived)) {
    std::unique_ptr<WebRequestEventDetails> event_details(
        CreateEventDetails(*request, extra_info_spec));
    event_details->SetResponseHeaders(*request, original_response_headers);

    initialize_blocked_requests |= DispatchEvent(
        browser_context, request, listeners, std::move(event_details));
  }

  if (!initialize_blocked_requests)
    return net::OK;  // Nobody saw a reason for modifying the request.

  BlockedRequest& blocked_request = blocked_requests_[request->id];
  blocked_request.event = kOnHeadersReceived;
  blocked_request.is_incognito |= browser_context->IsOffTheRecord();
  blocked_request.request = request;
  blocked_request.callback = std::move(callback);
  blocked_request.override_response_headers = override_response_headers;
  blocked_request.original_response_headers = original_response_headers;
  blocked_request.new_url = preserve_fragment_on_redirect_url;

  if (blocked_request.num_handlers_blocking == 0) {
    // If there are no blocking handlers, only the declarative rules tried
    // to modify the request and we can respond synchronously.
    return ExecuteDeltas(browser_context, request, false /* call_callback*/);
  }
  return net::ERR_IO_PENDING;
}

ExtensionWebRequestEventRouter::AuthRequiredResponse
ExtensionWebRequestEventRouter::OnAuthRequired(
    content::BrowserContext* browser_context,
    const WebRequestInfo* request,
    const net::AuthChallengeInfo& auth_info,
    AuthCallback callback,
    net::AuthCredentials* credentials) {
  // No browser_context means that this is for authentication challenges in the
  // system context. Skip in that case. Also skip sensitive requests.
  if (!browser_context ||
      WebRequestPermissions::HideRequest(PermissionHelper::Get(browser_context),
                                         *request)) {
    return AuthRequiredResponse::AUTH_REQUIRED_RESPONSE_NO_ACTION;
  }

  int extra_info_spec = 0;
  RawListeners listeners = GetMatchingListeners(
      browser_context, keys::kOnAuthRequiredEvent, request, &extra_info_spec);
  if (listeners.empty())
    return AuthRequiredResponse::AUTH_REQUIRED_RESPONSE_NO_ACTION;

  std::unique_ptr<WebRequestEventDetails> event_details(
      CreateEventDetails(*request, extra_info_spec));
  event_details->SetResponseHeaders(*request, request->response_headers.get());
  event_details->SetAuthInfo(auth_info);

  if (DispatchEvent(browser_context, request, listeners,
                    std::move(event_details))) {
    BlockedRequest& blocked_request = blocked_requests_[request->id];
    blocked_request.event = kOnAuthRequired;
    blocked_request.is_incognito |= browser_context->IsOffTheRecord();
    blocked_request.request = request;
    blocked_request.auth_callback = std::move(callback);
    blocked_request.auth_credentials = credentials;
    return AuthRequiredResponse::AUTH_REQUIRED_RESPONSE_IO_PENDING;
  }
  return AuthRequiredResponse::AUTH_REQUIRED_RESPONSE_NO_ACTION;
}

void ExtensionWebRequestEventRouter::OnBeforeRedirect(
    content::BrowserContext* browser_context,
    const WebRequestInfo* request,
    const GURL& new_location) {
  if (ShouldHideEvent(browser_context, *request))
    return;

  if (GetAndSetSignaled(request->id, kOnBeforeRedirect))
    return;

  ClearSignaled(request->id, kOnBeforeRequest);
  ClearSignaled(request->id, kOnBeforeSendHeaders);
  ClearSignaled(request->id, kOnSendHeaders);
  ClearSignaled(request->id, kOnHeadersReceived);

  int extra_info_spec = 0;
  RawListeners listeners = GetMatchingListeners(
      browser_context, keys::kOnBeforeRedirectEvent, request, &extra_info_spec);
  if (listeners.empty())
    return;

  std::unique_ptr<WebRequestEventDetails> event_details(
      CreateEventDetails(*request, extra_info_spec));
  event_details->SetResponseHeaders(*request, request->response_headers.get());
  event_details->SetResponseSource(*request);
  event_details->SetString(keys::kRedirectUrlKey, new_location.spec());

  DispatchEvent(browser_context, request, listeners, std::move(event_details));
}

void ExtensionWebRequestEventRouter::OnResponseStarted(
    content::BrowserContext* browser_context,
    const WebRequestInfo* request,
    int net_error) {
  DCHECK_NE(net::ERR_IO_PENDING, net_error);

  if (ShouldHideEvent(browser_context, *request))
    return;

  // OnResponseStarted is even triggered, when the request was cancelled.
  if (net_error != net::OK)
    return;

  int extra_info_spec = 0;
  RawListeners listeners =
      GetMatchingListeners(browser_context, keys::kOnResponseStartedEvent,
                           request, &extra_info_spec);
  if (listeners.empty())
    return;

  std::unique_ptr<WebRequestEventDetails> event_details(
      CreateEventDetails(*request, extra_info_spec));
  event_details->SetResponseHeaders(*request, request->response_headers.get());
  event_details->SetResponseSource(*request);

  DispatchEvent(browser_context, request, listeners, std::move(event_details));
}

void ExtensionWebRequestEventRouter::OnCompleted(
    content::BrowserContext* browser_context,
    const WebRequestInfo* request,
    int net_error) {
  // We hide events from the system context as well as sensitive requests.
  // However, if the request first became sensitive after redirecting we have
  // already signaled it and thus we have to signal the end of it. This is
  // risk-free because the handler cannot modify the request now.
  if (!browser_context ||
      (WebRequestPermissions::HideRequest(
           PermissionHelper::Get(browser_context), *request) &&
       !WasSignaled(*request))) {
    return;
  }

  request_time_tracker_->LogRequestEndTime(request->id, base::TimeTicks::Now());

  // See comment in OnErrorOccurred regarding net::ERR_WS_UPGRADE.
  DCHECK(net_error == net::OK || net_error == net::ERR_WS_UPGRADE);

  DCHECK(!GetAndSetSignaled(request->id, kOnCompleted));

  ClearPendingCallbacks(*request);

  int extra_info_spec = 0;
  RawListeners listeners = GetMatchingListeners(
      browser_context, keys::kOnCompletedEvent, request, &extra_info_spec);
  if (listeners.empty())
    return;

  std::unique_ptr<WebRequestEventDetails> event_details(
      CreateEventDetails(*request, extra_info_spec));
  event_details->SetResponseHeaders(*request, request->response_headers.get());
  event_details->SetResponseSource(*request);

  DispatchEvent(browser_context, request, listeners, std::move(event_details));
}

void ExtensionWebRequestEventRouter::OnErrorOccurred(
    content::BrowserContext* browser_context,
    const WebRequestInfo* request,
    bool started,
    int net_error) {
  // When WebSocket handshake request finishes, the request is cancelled with an
  // ERR_WS_UPGRADE code (see WebSocketStreamRequestImpl::PerformUpgrade).
  // WebRequest API reports this as a completed request.
  if (net_error == net::ERR_WS_UPGRADE) {
    OnCompleted(browser_context, request, net_error);
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
      (WebRequestPermissions::HideRequest(
           PermissionHelper::Get(browser_context), *request) &&
       !WasSignaled(*request))) {
    return;
  }

  request_time_tracker_->LogRequestEndTime(request->id, base::TimeTicks::Now());

  DCHECK_NE(net::OK, net_error);
  DCHECK_NE(net::ERR_IO_PENDING, net_error);

  DCHECK(!GetAndSetSignaled(request->id, kOnErrorOccurred));

  ClearPendingCallbacks(*request);

  int extra_info_spec = 0;
  RawListeners listeners = GetMatchingListeners(
      browser_context, web_request::OnErrorOccurred::kEventName, request,
      &extra_info_spec);
  if (listeners.empty())
    return;

  std::unique_ptr<WebRequestEventDetails> event_details(
      CreateEventDetails(*request, extra_info_spec));
  if (started)
    event_details->SetResponseSource(*request);
  else
    event_details->SetBoolean(keys::kFromCache, request->response_from_cache);
  event_details->SetString(keys::kErrorKey, net::ErrorToString(net_error));

  DispatchEvent(browser_context, request, listeners, std::move(event_details));
}

void ExtensionWebRequestEventRouter::OnRequestWillBeDestroyed(
    content::BrowserContext* browser_context,
    const WebRequestInfo* request) {
  ClearPendingCallbacks(*request);
  signaled_requests_.erase(request->id);
  request_time_tracker_->LogRequestEndTime(request->id, base::TimeTicks::Now());
}

void ExtensionWebRequestEventRouter::ClearPendingCallbacks(
    const WebRequestInfo& request) {
  blocked_requests_.erase(request.id);
}

bool ExtensionWebRequestEventRouter::DispatchEvent(
    content::BrowserContext* browser_context,
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
      ++num_handlers_blocking;
    }
  }

  DispatchEventToListeners(browser_context, std::move(listeners_to_dispatch),
                           request->id, std::move(event_details));

  if (num_handlers_blocking > 0) {
    BlockedRequest& blocked_request = blocked_requests_[request->id];
    blocked_request.request = request;
    blocked_request.is_incognito |= browser_context->IsOffTheRecord();
    blocked_request.num_handlers_blocking += num_handlers_blocking;
    blocked_request.blocking_time = base::Time::Now();
    return true;
  }

  return false;
}

void ExtensionWebRequestEventRouter::DispatchEventToListeners(
    content::BrowserContext* browser_context,
    std::unique_ptr<ListenerIDs> listener_ids,
    uint64_t request_id,
    std::unique_ptr<WebRequestEventDetails> event_details) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!listener_ids->empty());
  DCHECK(event_details.get());

  std::string event_name =
      EventRouter::GetBaseEventName((*listener_ids)[0].sub_event_name);
  DCHECK(IsWebRequestEvent(event_name));

  BrowserContextData& data = data_[GetBrowserContextID(browser_context)];

  // Gather all potential sources for listeners. They may be in:
  // - The active listeners for this context.
  // - The inactive listeners for this context.
  // - The active listeners for the cross-browser context.
  // - The inactive listeners for the cross-browser context.
  Listeners& active_listeners = data.active_listeners[event_name];
  Listeners& inactive_listeners = data.inactive_listeners[event_name];
  Listeners* cross_active_listeners = nullptr;
  Listeners* cross_inactive_listeners = nullptr;
  if (data.cross_context) {
    auto& cross_data = data_[GetBrowserContextID(data.cross_context.get())];
    cross_active_listeners = &cross_data.active_listeners[event_name];
    cross_inactive_listeners = &cross_data.inactive_listeners[event_name];
  }

  for (const EventListener::ID& id : *listener_ids) {
    // Look for the event listener in the different listener sources.
    bool is_active = id.render_process_id != -1;
    Listeners* on_the_record_listeners =
        is_active ? &active_listeners : &inactive_listeners;
    Listeners* cross_listeners =
        is_active ? cross_active_listeners : cross_inactive_listeners;

    bool crosses_incognito = false;
    const EventListener* listener =
        FindEventListenerInContainer(id, *on_the_record_listeners);
    if (!listener && cross_listeners) {
      listener = FindEventListenerInContainer(id, *cross_listeners);
      crosses_incognito = true;
    }

    // It's possible the listener was removed. If so, bail.
    if (!listener)
      continue;

    DCHECK(listener->id == id);

    // Check that the listener's process is also still valid. This only applies
    // for active listeners.
    content::RenderProcessHost* render_process = nullptr;
    if (is_active) {
      render_process = content::RenderProcessHost::FromID(id.render_process_id);
      if (!render_process)
        continue;  // The process for an active listener shut down. Bail.
    }

    // Filter out the optional keys that this listener didn't request.
    base::Value::List args_filtered;

    args_filtered.Append(event_details->GetFilteredDict(
        listener->extra_info_spec, PermissionHelper::Get(browser_context),
        listener->id.extension_id, crosses_incognito));

    if (is_active) {
      DCHECK(render_process);
      // Active listeners use a bespoke dispatching mechanism.
      // TODO(devlin): Now that the webRequest API is entirely handled on the
      // UI thread (it used to be on the IO thread), can we just use the
      // regular event dispatching code for this case, as well?
      EventRouter::DispatchEventToSender(
          render_process, id.browser_context, listener->id.extension_id,
          listener->histogram_value, listener->id.sub_event_name,
          listener->id.render_process_id, listener->id.worker_thread_id,
          listener->id.service_worker_version_id, std::move(args_filtered),
          mojom::EventFilteringInfo::New());
    } else {
      DCHECK_EQ(-1, id.service_worker_version_id);
      // In the event of a lazy listener, we go through normal extension
      // event dispatching code, which is responsible for waking up the
      // lazy context.
      std::unique_ptr<Event> event =
          std::make_unique<Event>(listener->histogram_value, id.sub_event_name,
                                  std::move(args_filtered));
      // Add a callback to the event in case we find we cannot dispatch to the
      // extension listener (as can happen if the extension fails to re-register
      // the event listener synchronously). If this happens, we treat the event
      // as handled so as to not block indefinitely.
      // The below base::Unretained is safe because this object is leaked.
      event->cannot_dispatch_callback = base::BindRepeating(
          &ExtensionWebRequestEventRouter::OnEventHandled,
          base::Unretained(this), id.browser_context, id.extension_id,
          event_name, id.sub_event_name, request_id, id.render_process_id,
          id.web_view_instance_id, id.worker_thread_id,
          id.service_worker_version_id, nullptr);
      EventRouter::Get(id.browser_context)
          ->DispatchEventToExtension(id.extension_id, std::move(event));
    }
  }
}

void ExtensionWebRequestEventRouter::OnEventHandled(
    content::BrowserContext* browser_context,
    const std::string& extension_id,
    const std::string& event_name,
    const std::string& sub_event_name,
    uint64_t request_id,
    int render_process_id,
    int web_view_instance_id,
    int worker_thread_id,
    int64_t service_worker_version_id,
    EventResponse* response) {
  BrowserContextData& context_data =
      data_[GetBrowserContextID(browser_context)];
  EventListener::ID id(browser_context, extension_id, sub_event_name,
                       render_process_id, web_view_instance_id,
                       worker_thread_id, service_worker_version_id);
  EventListener* listener = nullptr;

  // Check if the "handled" event was for an inactive listener (indicated by
  // having neither a render process nor service worker version). This happens
  // when we fail to dispatch an event to a lazy service worker listener.
  // In this case, we still treat the event as handled, because otherwise the
  // request will hang indefinitely.
  if (render_process_id == -1 &&
      service_worker_version_id ==
          blink::mojom::kInvalidServiceWorkerVersionId) {
    listener = FindEventListenerInContainer(
        id, context_data.inactive_listeners[event_name]);
  } else {
    listener = FindEventListenerInContainer(
        id, context_data.active_listeners[event_name]);
  }

  // This might happen, for example, if the extension has been unloaded.
  if (!listener) {
    return;
  }

  listener->blocked_requests.erase(request_id);
  DecrementBlockCount(browser_context, extension_id, event_name, request_id,
                      response, listener->extra_info_spec);
}

bool ExtensionWebRequestEventRouter::AddEventListener(
    content::BrowserContext* browser_context,
    const std::string& extension_id,
    const std::string& extension_name,
    events::HistogramValue histogram_value,
    const std::string& event_name,
    const std::string& sub_event_name,
    RequestFilter filter,
    int extra_info_spec,
    int render_process_id,
    int web_view_instance_id,
    int worker_thread_id,
    int64_t service_worker_version_id) {
  if (!IsWebRequestEvent(event_name))
    return false;

  if (event_name != EventRouter::GetBaseEventName(sub_event_name))
    return false;

  EventListener::ID id(browser_context, extension_id, sub_event_name,
                       render_process_id, web_view_instance_id,
                       worker_thread_id, service_worker_version_id);
  if (FindEventListener(id) != nullptr) {
    // This is likely an abuse of the API by a malicious extension.
    return false;
  }

  std::unique_ptr<EventListener> listener(new EventListener(id));
  listener->extension_name = extension_name;
  listener->histogram_value = histogram_value;
  listener->filter = std::move(filter);
  listener->extra_info_spec = extra_info_spec;
  if (web_view_instance_id) {
    base::RecordAction(
        base::UserMetricsAction("WebView.WebRequest.AddListener"));
  }

  RecordAddEventListenerUMAs(extra_info_spec);

  BrowserContextID browser_context_id = GetBrowserContextID(browser_context);

  // This might be a reactivated listener - a listener being added for a
  // lazy context where it was shut down and then respawned. This can only
  // happen for service worker listeners.
  bool is_reactivated = false;
  if (service_worker_version_id !=
      blink::mojom::kInvalidServiceWorkerVersionId) {
    auto& listeners = data_[browser_context_id].inactive_listeners[event_name];
    // Search for any listener with the same sub-event name.
    // NOTE: The sub-event name will be the same for any listener registered in
    // the *same order* in the extension. In practice, this should pretty much
    // always be the case, because we require listeners to be set up
    // synchronously.
    size_t erased = base::EraseIf(
        listeners, [browser_context, extension_id, sub_event_name](
                       const std::unique_ptr<EventListener>& listener) {
          return listener->id.browser_context == browser_context &&
                 listener->id.extension_id == extension_id &&
                 listener->id.sub_event_name == sub_event_name;
        });
    // Only a single listener should ever match. It's possible no listener will
    // match if this is a new listener in a worker context.
    DCHECK_LE(erased, 1u);
    is_reactivated = erased > 0u;
  }

  data_[browser_context_id].active_listeners[event_name].push_back(
      std::move(listener));

  // If the listener was previously registered, there's no need to adjust the
  // extra headers count.
  if (!is_reactivated && extra_info_spec & ExtraInfoSpec::EXTRA_HEADERS)
    IncrementExtraHeadersListenerCount(browser_context);

  return true;
}

ExtensionWebRequestEventRouter::EventListener*
ExtensionWebRequestEventRouter::FindEventListener(const EventListener::ID& id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::string event_name = EventRouter::GetBaseEventName(id.sub_event_name);
  Listeners& listeners = data_[GetBrowserContextID(id.browser_context.get())]
                             .active_listeners[event_name];
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

// static
std::unique_ptr<ExtensionWebRequestEventRouter::EventListener>
ExtensionWebRequestEventRouter::RemoveMatchingListener(
    Listeners& listeners,
    const ExtensionId& extension_id,
    const std::string& sub_event_name,
    absl::optional<int> worker_thread_id,
    absl::optional<int64_t> service_worker_version_id,
    BrowserContextID browser_context_id) {
  Listeners removed_listeners;
  for (auto iter = listeners.begin(); iter != listeners.end();) {
    std::unique_ptr<EventListener>& listener = *iter;
    const EventListener::ID& id = listener->id;
    DCHECK_EQ(browser_context_id,
              GetBrowserContextID(id.browser_context.get()));
    bool listener_matches =
        extension_id == id.extension_id &&
        sub_event_name == id.sub_event_name &&
        (!worker_thread_id || worker_thread_id == id.worker_thread_id) &&
        (!service_worker_version_id ||
         service_worker_version_id == id.service_worker_version_id);
    if (!listener_matches) {
      ++iter;
      continue;
    }

    removed_listeners.push_back(std::move(listener));
    iter = listeners.erase(iter);
  }

  DCHECK_LE(removed_listeners.size(), 1u);
  return removed_listeners.empty() ? nullptr
                                   : std::move(removed_listeners.front());
}

void ExtensionWebRequestEventRouter::RemoveLazyListener(
    content::BrowserContext* original_context,
    const ExtensionId& extension_id,
    const std::string& sub_event_name) {
  std::string event_name = EventRouter::GetBaseEventName(sub_event_name);

  BrowserContextID original_context_id = GetBrowserContextID(original_context);

  // Remove any active or inactive listeners that match the sub-event name.
  // Due to https://crbug.com/1347597, we only have a single lazy listener
  // registration shared for both the on- and off-the-record contexts, so we
  // need to remove it from both. This means there may be a listener in any
  // of our four sets (active and inactive for both the on-the-record and
  // off-the-record contexts).
  BrowserContextData& data = data_[original_context_id];
  Listeners removed_listeners;
  auto check_list = [&removed_listeners, extension_id, sub_event_name](
                        Listeners& listeners,
                        BrowserContextID browser_context_id) {
    auto listener = RemoveMatchingListener(listeners, extension_id,
                                           sub_event_name, absl::nullopt,
                                           absl::nullopt, browser_context_id);
    if (listener)
      removed_listeners.push_back(std::move(listener));
  };

  check_list(data.active_listeners[event_name], original_context_id);
  check_list(data.inactive_listeners[event_name], original_context_id);
  if (data.cross_context) {
    BrowserContextID cross_context_id =
        GetBrowserContextID(data.cross_context.get());
    BrowserContextData& cross_data = data_[cross_context_id];
    check_list(cross_data.active_listeners[event_name], cross_context_id);
    check_list(cross_data.inactive_listeners[event_name], cross_context_id);
  }

  // We should only have a maximum of two listeners removed - one for the
  // on-the-record and one for the off-the-record profile - since each listener
  // must either be active *or* inactive.
  DCHECK_LE(removed_listeners.size(), 2u);

  for (const auto& listener : removed_listeners) {
    CleanUpForListener(*listener, ListenerUpdateType::kRemove);
  }
}

void ExtensionWebRequestEventRouter::UpdateActiveListener(
    ListenerUpdateType update_type,
    BrowserContextID browser_context_id,
    const ExtensionId& extension_id,
    const std::string& sub_event_name,
    int worker_thread_id,
    int64_t service_worker_version_id) {
  std::string event_name = EventRouter::GetBaseEventName(sub_event_name);

  BrowserContextData& data = data_[browser_context_id];
  auto matching_listener = RemoveMatchingListener(
      data.active_listeners[event_name], extension_id, sub_event_name,
      worker_thread_id, service_worker_version_id, browser_context_id);
  if (!matching_listener)
    return;

  CleanUpForListener(*matching_listener, update_type);

  // If this is only deactivating the listener, reset the process-specific bits
  // for the listener and move it to inactive listeners.
  if (update_type == ListenerUpdateType::kDeactivate) {
    matching_listener->id.worker_thread_id = kMainThreadId;
    matching_listener->id.service_worker_version_id =
        blink::mojom::kInvalidServiceWorkerVersionId;
    matching_listener->id.render_process_id = -1;
    data.inactive_listeners[event_name].push_back(std::move(matching_listener));
  }
}

void ExtensionWebRequestEventRouter::CleanUpForListener(
    const EventListener& listener,
    ListenerUpdateType update_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::string event_name =
      EventRouter::GetBaseEventName(listener.id.sub_event_name);

  // Unblock any request that this event listener may have been blocking.
  // Note that we do this even for deactivations, since if the service worker
  // is shut down (which would happen if it reached the hard lifetime timeout),
  // it won't be able to respond to the request.
  // TODO(https://crbug.com/1024211): This likely won't be sufficient, since it
  // means requests can leak through.
  for (uint64_t blocked_request_id : listener.blocked_requests) {
    DecrementBlockCount(listener.id.browser_context, listener.id.extension_id,
                        event_name, blocked_request_id, nullptr,
                        0 /* extra_info_spec */);
  }

  // Update the extra headers count and clear the cache only if the listener is
  // fully removed; otherwise, these values are still correct.
  if (update_type == ListenerUpdateType::kRemove) {
    if (listener.extra_info_spec & ExtraInfoSpec::EXTRA_HEADERS) {
      DecrementExtraHeadersListenerCount(listener.id.browser_context);
    }
    helpers::ClearCacheOnNavigation();
  }
}

void ExtensionWebRequestEventRouter::RemoveWebViewEventListeners(
    content::BrowserContext* browser_context,
    int render_process_id,
    int web_view_instance_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Iterate over all listeners of all WebRequest events to delete
  // any listeners that belong to the provided <webview>.
  BrowserContextData& data = data_[GetBrowserContextID(browser_context)];
  for (auto& kv : data.active_listeners) {
    Listeners& listeners = kv.second;
    for (auto iter = listeners.begin(); iter != listeners.end();) {
      std::unique_ptr<EventListener>& listener = *iter;
      bool listener_matches =
          listener->id.render_process_id == render_process_id &&
          listener->id.web_view_instance_id == web_view_instance_id;
      if (!listener_matches) {
        ++iter;
        continue;
      }
      CleanUpForListener(**iter, ListenerUpdateType::kRemove);
      iter = listeners.erase(iter);
    }
  }
}

void ExtensionWebRequestEventRouter::OnOTRBrowserContextCreated(
    content::BrowserContext* original_browser_context,
    content::BrowserContext* otr_browser_context) {
  data_[GetBrowserContextID(original_browser_context)].cross_context =
      otr_browser_context;
  auto& otr_data = data_[GetBrowserContextID(otr_browser_context)];
  otr_data.cross_context = original_browser_context;
}

void ExtensionWebRequestEventRouter::OnOTRBrowserContextDestroyed(
    content::BrowserContext* original_browser_context,
    content::BrowserContext* otr_browser_context) {
  data_[GetBrowserContextID(original_browser_context)].cross_context = nullptr;
  OnBrowserContextShutdown(otr_browser_context);
  DCHECK(!base::Contains(data_, GetBrowserContextID(otr_browser_context)));
}

void ExtensionWebRequestEventRouter::AddCallbackForPageLoad(
    base::OnceClosure callback) {
  callbacks_for_page_load_.push_back(std::move(callback));
}

bool ExtensionWebRequestEventRouter::HasExtraHeadersListenerForRequest(
    content::BrowserContext* browser_context,
    const WebRequestInfo* request) {
  DCHECK(request);
  if (ShouldHideEvent(browser_context, *request))
    return false;

  int extra_info_spec = 0;
  for (const char* name : kWebRequestEvents) {
    GetMatchingListeners(browser_context, name, request, &extra_info_spec);
    if (extra_info_spec & ExtraInfoSpec::EXTRA_HEADERS)
      return true;
  }

  // Check declarative net request API rulesets.
  return declarative_net_request::RulesMonitorService::Get(browser_context)
      ->ruleset_manager()
      ->HasExtraHeadersMatcherForRequest(*request,
                                         browser_context->IsOffTheRecord());
}

bool ExtensionWebRequestEventRouter::HasAnyExtraHeadersListener(
    content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (HasAnyExtraHeadersListenerImpl(browser_context))
    return true;

  content::BrowserContext* cross_browser_context =
      GetCrossBrowserContext(browser_context);
  if (cross_browser_context)
    return HasAnyExtraHeadersListenerImpl(cross_browser_context);

  return false;
}

void ExtensionWebRequestEventRouter::IncrementExtraHeadersListenerCount(
    content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  BrowserContextData& data = data_[GetBrowserContextID(browser_context)];
  DCHECK_GE(data.extra_headers_count, 0);
  data.extra_headers_count++;
}

void ExtensionWebRequestEventRouter::DecrementExtraHeadersListenerCount(
    content::BrowserContext* browser_context) {
  BrowserContextData& data = data_[GetBrowserContextID(browser_context)];
  data.extra_headers_count--;
  DCHECK_GE(data.extra_headers_count, 0);
}

void ExtensionWebRequestEventRouter::OnBrowserContextShutdown(
    content::BrowserContext* browser_context) {
  data_.erase(GetBrowserContextID(browser_context));
}

size_t ExtensionWebRequestEventRouter::GetListenerCountForTesting(
    content::BrowserContext* browser_context,
    const std::string& event_name) {
  return data_[GetBrowserContextID(browser_context)]
      .active_listeners[event_name]
      .size();
}

size_t ExtensionWebRequestEventRouter::GetInactiveListenerCountForTesting(
    content::BrowserContext* browser_context,
    const std::string& event_name) {
  return data_[GetBrowserContextID(browser_context)]
      .inactive_listeners[event_name]
      .size();
}

bool ExtensionWebRequestEventRouter::HasAnyExtraHeadersListenerImpl(
    content::BrowserContext* browser_context) {
  auto iter = data_.find(GetBrowserContextID(browser_context));
  return iter != data_.end() && iter->second.extra_headers_count > 0;
}

bool ExtensionWebRequestEventRouter::IsPageLoad(
    const WebRequestInfo& request) const {
  return request.web_request_type == WebRequestResourceType::MAIN_FRAME;
}

void ExtensionWebRequestEventRouter::NotifyPageLoad() {
  for (auto& callback : callbacks_for_page_load_)
    std::move(callback).Run();
  callbacks_for_page_load_.clear();
}

content::BrowserContext* ExtensionWebRequestEventRouter::GetCrossBrowserContext(
    content::BrowserContext* browser_context) const {
  auto iter = data_.find(GetBrowserContextID(browser_context));
  if (iter == data_.end())
    return nullptr;
  return iter->second.cross_context;
}

bool ExtensionWebRequestEventRouter::WasSignaled(
    const WebRequestInfo& request) const {
  auto flag = signaled_requests_.find(request.id);
  return flag != signaled_requests_.end() && flag->second != 0;
}

ExtensionWebRequestEventRouter::RawListeners
ExtensionWebRequestEventRouter::GetMatchingListeners(
    content::BrowserContext* browser_context,
    const std::string& event_name,
    const WebRequestInfo* request,
    int* extra_info_spec) {
  // TODO(mpcomplete): handle browser_context == NULL (should collect all
  // listeners).
  *extra_info_spec = 0;

  bool is_request_from_extension =
      IsRequestFromExtension(*request, browser_context);

  std::string web_request_event_name(event_name);
  if (request->is_web_view) {
    web_request_event_name.replace(
        0, sizeof(kWebRequestEventPrefix) - 1, webview::kWebViewEventPrefix);
  }

  RawListeners matching_listeners;

  auto& browser_context_data = data_[GetBrowserContextID(browser_context)];
  GetMatchingListenersForRequest(
      browser_context_data.active_listeners[web_request_event_name], *request,
      *browser_context, is_request_from_extension,
      /*crosses_incognito=*/false, &matching_listeners, extra_info_spec);
  GetMatchingListenersForRequest(
      browser_context_data.inactive_listeners[web_request_event_name], *request,
      *browser_context, is_request_from_extension,
      /*crosses_incognito=*/false, &matching_listeners, extra_info_spec);

  content::BrowserContext* cross_browser_context =
      GetCrossBrowserContext(browser_context);
  if (cross_browser_context) {
    auto& cross_context_data =
        data_[GetBrowserContextID(cross_browser_context)];
    GetMatchingListenersForRequest(
        cross_context_data.active_listeners[web_request_event_name], *request,
        *cross_browser_context, is_request_from_extension,
        /*crosses_incognito=*/true, &matching_listeners, extra_info_spec);
    GetMatchingListenersForRequest(
        cross_context_data.inactive_listeners[web_request_event_name], *request,
        *cross_browser_context, is_request_from_extension,
        /*crosses_incognito=*/true, &matching_listeners, extra_info_spec);
  }

  return matching_listeners;
}

bool ExtensionWebRequestEventRouter::ListenerMatchesRequest(
    const EventListener& listener,
    const WebRequestInfo& request,
    content::BrowserContext& browser_context,
    bool is_request_from_extension,
    bool crosses_incognito) {
  if (!content::RenderProcessHost::FromID(listener.id.render_process_id) &&
      listener.id.service_worker_version_id >= 0) {
    // The IPC sender has been deleted. This listener will be removed soon
    // via a call to `CleanUpForListener()`. For now, just skip it.
    return false;
  }

  if (request.is_web_view) {
    // If this is a navigation request, then we can skip this check. IDs will
    // be -1 and the request is trusted.
    if (!request.is_navigation_request &&
        listener.id.render_process_id != request.web_view_embedder_process_id) {
      return false;
    }

    if (listener.id.web_view_instance_id != request.web_view_instance_id)
      return false;
  }

  // Filter requests from other extensions / apps. This does not work for
  // content scripts, or extension pages in non-extension processes.
  if (is_request_from_extension &&
      listener.id.render_process_id != request.render_process_id) {
    return false;
  }

  if (!listener.filter.urls.is_empty() &&
      !listener.filter.urls.MatchesURL(request.url)) {
    return false;
  }

  // Check if the tab id and window id match, if they were set in the
  // listener params.
  if ((listener.filter.tab_id != -1 &&
       request.frame_data.tab_id != listener.filter.tab_id) ||
      (listener.filter.window_id != -1 &&
       request.frame_data.window_id != listener.filter.window_id)) {
    return false;
  }

  const std::vector<WebRequestResourceType>& types = listener.filter.types;
  if (!types.empty() && !base::Contains(types, request.web_request_type)) {
    return false;
  }

  if (!request.is_web_view) {
    PermissionsData::PageAccess access =
        WebRequestPermissions::CanExtensionAccessURL(
            PermissionHelper::Get(&browser_context), listener.id.extension_id,
            request.url, request.frame_data.tab_id, crosses_incognito,
            WebRequestPermissions::
                REQUIRE_HOST_PERMISSION_FOR_URL_AND_INITIATOR,
            request.initiator, request.web_request_type);

    if (access != PermissionsData::PageAccess::kAllowed) {
      if (access == PermissionsData::PageAccess::kWithheld) {
        DCHECK(ExtensionsAPIClient::Get());
        ExtensionsAPIClient::Get()->NotifyWebRequestWithheld(
            request.render_process_id, request.frame_routing_id,
            listener.id.extension_id);
      }

      return false;
    }
  }

  bool blocking_listener =
      (listener.extra_info_spec &
       (ExtraInfoSpec::BLOCKING | ExtraInfoSpec::ASYNC_BLOCKING)) != 0;

  // We do not want to notify extensions about XHR requests that are
  // triggered by themselves. This is a workaround to prevent deadlocks
  // in case of synchronous XHR requests that block the extension renderer
  // and therefore prevent the extension from processing the request
  // handler. This is only a problem for blocking listeners.
  // http://crbug.com/105656
  bool synchronous_xhr_from_extension =
      !request.is_async && is_request_from_extension &&
      request.web_request_type == WebRequestResourceType::XHR;
  return !blocking_listener || !synchronous_xhr_from_extension;
}

void ExtensionWebRequestEventRouter::GetMatchingListenersForRequest(
    const Listeners& listeners,
    const WebRequestInfo& request,
    content::BrowserContext& browser_context,
    bool is_request_from_extension,
    bool crosses_incognito,
    RawListeners* listeners_out,
    int* extra_info_spec_out) {
  for (const auto& listener : listeners) {
    if (ListenerMatchesRequest(*listener, request, browser_context,
                               is_request_from_extension, crosses_incognito)) {
      listeners_out->push_back(listener.get());
      *extra_info_spec_out |= listener->extra_info_spec;
    }
  }
}

namespace {

helpers::EventResponseDelta CalculateDelta(
    content::BrowserContext* browser_context,
    ExtensionWebRequestEventRouter::BlockedRequest* blocked_request,
    ExtensionWebRequestEventRouter::EventResponse* response,
    int extra_info_spec) {
  switch (blocked_request->event) {
    case ExtensionWebRequestEventRouter::kOnBeforeRequest:
      return helpers::CalculateOnBeforeRequestDelta(
          response->extension_id, response->extension_install_time,
          response->cancel, response->new_url);
    case ExtensionWebRequestEventRouter::kOnBeforeSendHeaders: {
      net::HttpRequestHeaders* old_headers = blocked_request->request_headers;
      net::HttpRequestHeaders* new_headers = response->request_headers.get();
      return helpers::CalculateOnBeforeSendHeadersDelta(
          browser_context, response->extension_id,
          response->extension_install_time, response->cancel, old_headers,
          new_headers, extra_info_spec);
    }
    case ExtensionWebRequestEventRouter::kOnHeadersReceived: {
      const net::HttpResponseHeaders* old_headers =
          blocked_request->original_response_headers.get();
      helpers::ResponseHeaders* new_headers =
          response->response_headers.get();
      return helpers::CalculateOnHeadersReceivedDelta(
          response->extension_id, response->extension_install_time,
          response->cancel, blocked_request->request->url, response->new_url,
          old_headers, new_headers, extra_info_spec);
    }
    case ExtensionWebRequestEventRouter::kOnAuthRequired:
      return helpers::CalculateOnAuthRequiredDelta(
          response->extension_id, response->extension_install_time,
          response->cancel, response->auth_credentials);
    default:
      NOTREACHED();
      return helpers::EventResponseDelta("", base::Time());
  }
}

base::Value::List SerializeResponseHeaders(
    const helpers::ResponseHeaders& headers) {
  base::Value::List serialized_headers;
  for (const auto& it : headers) {
    serialized_headers.Append(
        helpers::CreateHeaderDictionary(it.first, it.second));
  }
  return serialized_headers;
}

// Convert a RequestCookieModifications/ResponseCookieModifications object to a
// base::Value::List which summarizes the changes made.  This is templated since
// the two types (request/response) are different but contain essentially the
// same fields.
template <typename CookieType>
base::Value::List SummarizeCookieModifications(
    const std::vector<CookieType>& modifications) {
  base::Value::List cookie_modifications;
  for (const CookieType& mod : modifications) {
    base::Value::Dict summary;
    switch (mod.type) {
      case helpers::ADD:
        summary.Set(activity_log::kCookieModificationTypeKey,
                    activity_log::kCookieModificationAdd);
        break;
      case helpers::EDIT:
        summary.Set(activity_log::kCookieModificationTypeKey,
                    activity_log::kCookieModificationEdit);
        break;
      case helpers::REMOVE:
        summary.Set(activity_log::kCookieModificationTypeKey,
                    activity_log::kCookieModificationRemove);
        break;
    }
    if (mod.filter) {
      if (mod.filter->name) {
        summary.Set(activity_log::kCookieFilterNameKey,
                    *mod.modification->name);
      }
      if (mod.filter->domain) {
        summary.Set(activity_log::kCookieFilterDomainKey,
                    *mod.modification->name);
      }
    }
    if (mod.modification) {
      if (mod.modification->name) {
        summary.Set(activity_log::kCookieModDomainKey, *mod.modification->name);
      }
      if (mod.modification->domain) {
        summary.Set(activity_log::kCookieModDomainKey, *mod.modification->name);
      }
    }
    cookie_modifications.Append(std::move(summary));
  }
  return cookie_modifications;
}

// Converts an EventResponseDelta object to a dictionary value suitable for the
// activity log.
base::Value::Dict SummarizeResponseDelta(
    const std::string& event_name,
    const helpers::EventResponseDelta& delta) {
  base::Value::Dict details;
  if (delta.cancel)
    details.Set(activity_log::kCancelKey, true);
  if (!delta.new_url.is_empty())
    details.Set(activity_log::kNewUrlKey, delta.new_url.spec());

  base::Value::List modified_headers;
  net::HttpRequestHeaders::Iterator iter(delta.modified_request_headers);
  while (iter.GetNext()) {
    modified_headers.Append(
        helpers::CreateHeaderDictionary(iter.name(), iter.value()));
  }
  if (!modified_headers.empty()) {
    details.Set(activity_log::kModifiedRequestHeadersKey,
                std::move(modified_headers));
  }

  base::Value::List deleted_headers;
  for (const std::string& header : delta.deleted_request_headers) {
    deleted_headers.Append(header);
  }
  if (!deleted_headers.empty()) {
    details.Set(activity_log::kDeletedRequestHeadersKey,
                std::move(deleted_headers));
  }

  if (!delta.added_response_headers.empty()) {
    details.Set(activity_log::kAddedRequestHeadersKey,
                SerializeResponseHeaders(delta.added_response_headers));
  }
  if (!delta.deleted_response_headers.empty()) {
    details.Set(activity_log::kDeletedResponseHeadersKey,
                SerializeResponseHeaders(delta.deleted_response_headers));
  }
  if (delta.auth_credentials.has_value()) {
    details.Set(activity_log::kAuthCredentialsKey,
                base::UTF16ToUTF8(delta.auth_credentials->username()) + ":*");
  }

  if (!delta.response_cookie_modifications.empty()) {
    details.Set(
        activity_log::kResponseCookieModificationsKey,
        SummarizeCookieModifications(delta.response_cookie_modifications));
  }

  return details;
}

}  // namespace

void ExtensionWebRequestEventRouter::DecrementBlockCount(
    content::BrowserContext* browser_context,
    const std::string& extension_id,
    const std::string& event_name,
    uint64_t request_id,
    EventResponse* response,
    int extra_info_spec) {
  std::unique_ptr<EventResponse> response_scoped(response);

  // It's possible that this request was deleted, or cancelled by a previous
  // event handler or handled by Declarative Net Request API. If so, ignore this
  // response.
  auto it = blocked_requests_.find(request_id);
  if (it == blocked_requests_.end())
    return;

  BlockedRequest& blocked_request = it->second;

  // Ensure that the response is for the event we are blocked on.
  DCHECK_EQ(blocked_request.event, GetEventTypeFromEventName(event_name));
  // Cache the event type; we use it below.
  EventTypes request_event = blocked_request.event;

  int num_handlers_blocking = --blocked_request.num_handlers_blocking;
  CHECK_GE(num_handlers_blocking, 0);

  if (response) {
    helpers::EventResponseDelta delta = CalculateDelta(
        browser_context, &blocked_request, response, extra_info_spec);

    activity_monitor::OnWebRequestApiUsed(
        static_cast<content::BrowserContext*>(browser_context), extension_id,
        blocked_request.request->url, blocked_request.is_incognito, event_name,
        SummarizeResponseDelta(event_name, delta));

    blocked_request.response_deltas.push_back(std::move(delta));
  }

  if (num_handlers_blocking == 0) {
    ExecuteDeltas(browser_context, blocked_request.request, true);
    // Note: `blocked_request` can be deleted here, depending on the outcome
    // of ExecuteDeltas(). Use the cached `request_event` and `request_id`
    // instead of using `blocked_request`.
    if (request_event == kOnBeforeRequest) {
      request_time_tracker_->LogBeforeRequestCompletionTime(
          request_id, base::TimeTicks::Now());
    }
  }
}

void ExtensionWebRequestEventRouter::SendMessages(
    content::BrowserContext* browser_context,
    const BlockedRequest& blocked_request) {
  const helpers::EventResponseDeltas& deltas = blocked_request.response_deltas;
  for (const auto& delta : deltas) {
    const std::set<std::string>& messages = delta.messages_to_extension;
    for (const std::string& message : messages) {
      std::unique_ptr<WebRequestEventDetails> event_details(CreateEventDetails(
          *blocked_request.request, /* extra_info_spec */ 0));
      event_details->SetString(keys::kMessageKey, message);
      event_details->SetString(keys::kStageKey,
                               GetRequestStageAsString(blocked_request.event));
      SendOnMessageEventOnUI(browser_context, delta.extension_id,
                             blocked_request.request->is_web_view,
                             blocked_request.request->web_view_instance_id,
                             std::move(event_details));
    }
  }
}

int ExtensionWebRequestEventRouter::ExecuteDeltas(
    content::BrowserContext* browser_context,
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
  // The set of request headers which were removed or set to new values.
  std::set<std::string> request_headers_removed;
  std::set<std::string> request_headers_set;

  deltas.sort(&helpers::InDecreasingExtensionInstallationTimeOrder);

  absl::optional<ExtensionId> canceled_by_extension;
  helpers::MergeCancelOfResponses(blocked_request.response_deltas,
                                  &canceled_by_extension);

  extension_web_request_api_helpers::IgnoredActions ignored_actions;
  std::vector<const DNRRequestAction*> matched_dnr_actions;
  if (blocked_request.event == kOnBeforeRequest) {
    CHECK(!blocked_request.callback.is_null());
    helpers::MergeOnBeforeRequestResponses(
        request->url, blocked_request.response_deltas, blocked_request.new_url,
        &ignored_actions);
  } else if (blocked_request.event == kOnBeforeSendHeaders) {
    CHECK(!blocked_request.before_send_headers_callback.is_null());
    helpers::MergeOnBeforeSendHeadersResponses(
        *request, blocked_request.response_deltas,
        blocked_request.request_headers, &ignored_actions,
        &request_headers_removed, &request_headers_set,
        &request_headers_modified, &matched_dnr_actions);
  } else if (blocked_request.event == kOnHeadersReceived) {
    CHECK(!blocked_request.callback.is_null());
    helpers::MergeOnHeadersReceivedResponses(
        *request, blocked_request.response_deltas,
        blocked_request.original_response_headers.get(),
        blocked_request.override_response_headers, blocked_request.new_url,
        &ignored_actions, &response_headers_modified, &matched_dnr_actions);
  } else if (blocked_request.event == kOnAuthRequired) {
    CHECK(blocked_request.callback.is_null());
    CHECK(!blocked_request.auth_callback.is_null());
    credentials_set = helpers::MergeOnAuthRequiredResponses(
        blocked_request.response_deltas, blocked_request.auth_credentials,
        &ignored_actions);
  } else {
    NOTREACHED();
  }

  SendMessages(browser_context, blocked_request);

  if (!ignored_actions.empty()) {
    NotifyIgnoredActionsOnUI(browser_context, request->id,
                             std::move(ignored_actions));
  }

  for (const DNRRequestAction* action : matched_dnr_actions)
    OnDNRActionMatched(browser_context, *request, *action);

  const bool redirected =
      blocked_request.new_url && !blocked_request.new_url->is_empty();

  if (canceled_by_extension)
    request_time_tracker_->SetRequestCanceled(request->id);
  else if (redirected)
    request_time_tracker_->SetRequestRedirected(request->id);

  // Log UMA metrics. Note: We are not necessarily concerned with the final
  // action taken. Instead we are interested in how frequently the different
  // actions are used by extensions. Hence multiple actions may be logged for a
  // single delta execution.
  if (canceled_by_extension)
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
  int rv = net::OK;
  if (canceled_by_extension) {
    rv = net::ERR_BLOCKED_BY_CLIENT;
    RecordNetworkRequestBlocked(request->ukm_source_id,
                                canceled_by_extension.value());
    TRACE_EVENT2("extensions", "NetworkRequestBlockedByClient", "extension",
                 canceled_by_extension.value(), "id", request->id);
  }

  if (!blocked_request.callback.is_null()) {
    net::CompletionOnceCallback callback = std::move(blocked_request.callback);
    // Ensure that request is removed before callback because the callback
    // might trigger the next event.
    blocked_requests_.erase(request->id);
    if (call_callback)
      std::move(callback).Run(rv);
  } else if (!blocked_request.before_send_headers_callback.is_null()) {
    auto callback = std::move(blocked_request.before_send_headers_callback);
    // Ensure that request is removed before callback because the callback
    // might trigger the next event.
    blocked_requests_.erase(request->id);
    if (call_callback)
      std::move(callback).Run(request_headers_removed, request_headers_set, rv);
  } else if (!blocked_request.auth_callback.is_null()) {
    ExtensionWebRequestEventRouter::AuthRequiredResponse response;
    if (canceled_by_extension)
      response = AuthRequiredResponse::AUTH_REQUIRED_RESPONSE_CANCEL_AUTH;
    else if (credentials_set)
      response = AuthRequiredResponse::AUTH_REQUIRED_RESPONSE_SET_AUTH;
    else
      response = AuthRequiredResponse::AUTH_REQUIRED_RESPONSE_NO_ACTION;

    AuthCallback callback = std::move(blocked_request.auth_callback);
    blocked_requests_.erase(request->id);
    if (call_callback)
      std::move(callback).Run(response);
  } else {
    blocked_requests_.erase(request->id);
  }
  return rv;
}

bool ExtensionWebRequestEventRouter::ProcessDeclarativeRules(
    content::BrowserContext* browser_context,
    const std::string& event_name,
    const WebRequestInfo* request,
    RequestStage request_stage,
    const net::HttpResponseHeaders* original_response_headers) {
  int rules_registry_id = request->is_web_view
                              ? request->web_view_rules_registry_id
                              : RulesRegistryService::kDefaultRulesRegistryID;

  RulesRegistryKey rules_key(BrowserContextID(browser_context),
                             rules_registry_id);
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

  content::BrowserContext* cross_browser_context =
      GetCrossBrowserContext(browser_context);
  RulesRegistryKey cross_browser_context_rules_key(
      BrowserContextID(cross_browser_context), rules_registry_id);
  if (cross_browser_context) {
    auto it = rules_registries_.find(cross_browser_context_rules_key);
    if (it != rules_registries_.end())
      relevant_registries.push_back(std::make_pair(it->second.get(), true));
  }

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
        base::BindOnce(&ExtensionWebRequestEventRouter::OnRulesRegistryReady,
                       base::Unretained(this), browser_context, event_name,
                       request->id, request_stage));
    BlockedRequest& blocked_request = blocked_requests_[request->id];
    blocked_request.num_handlers_blocking++;
    blocked_request.request = request;
    blocked_request.is_incognito |= browser_context->IsOffTheRecord();
    blocked_request.blocking_time = base::Time::Now();
    blocked_request.original_response_headers = original_response_headers;
    return true;
  }

  bool deltas_created = false;
  for (const auto& it : relevant_registries) {
    WebRequestRulesRegistry* rules_registry = it.first;
    helpers::EventResponseDeltas result = rules_registry->CreateDeltas(
        PermissionHelper::Get(browser_context),
        WebRequestData(request, request_stage, original_response_headers),
        it.second);

    if (!result.empty()) {
      helpers::EventResponseDeltas& deltas =
          blocked_requests_[request->id].response_deltas;
      deltas.insert(deltas.end(), std::make_move_iterator(result.begin()),
                    std::make_move_iterator(result.end()));
      deltas_created = true;
    }
  }

  return deltas_created;
}

void ExtensionWebRequestEventRouter::OnRulesRegistryReady(
    content::BrowserContext* browser_context,
    const std::string& event_name,
    uint64_t request_id,
    RequestStage request_stage) {
  // It's possible that this request was deleted, or cancelled by a previous
  // event handler. If so, ignore this response.
  auto it = blocked_requests_.find(request_id);
  if (it == blocked_requests_.end())
    return;

  BlockedRequest& blocked_request = it->second;
  ProcessDeclarativeRules(browser_context, event_name, blocked_request.request,
                          request_stage,
                          blocked_request.original_response_headers.get());
  DecrementBlockCount(browser_context, std::string(), event_name, request_id,
                      nullptr, 0 /* extra_info_spec */);
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

  base::WeakPtrFactory<ClearCacheQuotaHeuristic> weak_ptr_factory_{this};
};

bool ClearCacheQuotaHeuristic::Apply(Bucket* bucket,
                                     const base::TimeTicks& event_time) {
  if (event_time > bucket->expiration())
    bucket->Reset(config(), event_time);

  // Call bucket->DeductToken() on a new page load, this is when
  // webRequest.handlerBehaviorChanged() clears the cache.
  if (!callback_registered_) {
    ExtensionWebRequestEventRouter::GetInstance()->AddCallbackForPageLoad(
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
  ExtensionWebRequestEventRouter::RequestFilter filter;
  EXTENSION_FUNCTION_VALIDATE(args()[1].is_dict());
  // Failure + an empty error string means a fatal error.
  std::string error;
  EXTENSION_FUNCTION_VALIDATE(
      filter.InitFromValue(args()[1].GetDict(), &error) || !error.empty());
  if (!error.empty())
    return RespondNow(Error(std::move(error)));

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

  if (!web_view_instance_id) {
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
      ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
          browser_context(), extension_id_safe(), extension_name,
          GetEventHistogramValue(event_name), event_name, sub_event_name,
          std::move(filter), extra_info_spec, render_process_id,
          web_view_instance_id, worker_thread_id(),
          service_worker_version_id());
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
    std::unique_ptr<ExtensionWebRequestEventRouter::EventResponse> response) {
  ExtensionWebRequestEventRouter::GetInstance()->OnEventHandled(
      browser_context(), extension_id_safe(), event_name, sub_event_name,
      request_id, render_process_id, web_view_instance_id, worker_thread_id(),
      service_worker_version_id(), response.release());
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
  EXTENSION_FUNCTION_VALIDATE(base::StringToUint64(request_id_str,
                                                   &request_id));

  int render_process_id = source_process_id();

  std::unique_ptr<ExtensionWebRequestEventRouter::EventResponse> response;
  if (HasOptionalArgument(4)) {
    EXTENSION_FUNCTION_VALIDATE(args()[4].is_dict());
    const base::Value::Dict& dict_value = args()[4].GetDict();

    if (!dict_value.empty()) {
      base::Time install_time = ExtensionPrefs::Get(browser_context())
                                    ->GetLastUpdateTime(extension_id_safe());
      response =
          std::make_unique<ExtensionWebRequestEventRouter::EventResponse>(
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

  ExtensionWebRequestEventRouter::GetInstance()->OnEventHandled(
      browser_context(), extension_id_safe(), event_name, sub_event_name,
      request_id, render_process_id, web_view_instance_id, worker_thread_id(),
      service_worker_version_id(), response.release());

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

ExtensionWebRequestEventRouter::EventListener::ID::ID(
    content::BrowserContext* browser_context,
    const std::string& extension_id,
    const std::string& sub_event_name,
    int render_process_id,
    int web_view_instance_id,
    int worker_thread_id,
    int64_t service_worker_version_id)
    : browser_context(browser_context),
      extension_id(extension_id),
      sub_event_name(sub_event_name),
      render_process_id(render_process_id),
      web_view_instance_id(web_view_instance_id),
      worker_thread_id(worker_thread_id),
      service_worker_version_id(service_worker_version_id) {}

ExtensionWebRequestEventRouter::EventListener::ID::ID(const ID& source) =
    default;

bool ExtensionWebRequestEventRouter::EventListener::ID::operator==(
    const ID& that) const {
  // Since EventListeners are segmented by browser_context, check that
  // last, as it is exceedingly unlikely to be different.
  return extension_id == that.extension_id &&
         sub_event_name == that.sub_event_name &&
         web_view_instance_id == that.web_view_instance_id &&
         render_process_id == that.render_process_id &&
         worker_thread_id == that.worker_thread_id &&
         service_worker_version_id == that.service_worker_version_id &&
         browser_context == that.browser_context;
}

}  // namespace extensions
