// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/extension_web_request_event_router.h"

#include <algorithm>
#include <string_view>
#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/api/declarative/rules_registry_service.h"
#include "extensions/browser/api/declarative_net_request/action_tracker.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/request_action.h"
#include "extensions/browser/api/declarative_net_request/rules_monitor_service.h"
#include "extensions/browser/api/declarative_webrequest/request_stage.h"
#include "extensions/browser/api/declarative_webrequest/webrequest_constants.h"
#include "extensions/browser/api/declarative_webrequest/webrequest_rules_registry.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/web_accessible_resources/web_accessible_resources_router.h"
#include "extensions/browser/api/web_request/permission_helper.h"
#include "extensions/browser/api/web_request/web_request_api_constants.h"
#include "extensions/browser/api/web_request/web_request_api_helpers.h"
#include "extensions/browser/api/web_request/web_request_event_details.h"
#include "extensions/browser/api/web_request/web_request_event_router_factory.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/browser/api/web_request/web_request_permissions.h"
#include "extensions/browser/api/web_request/web_request_time_tracker.h"
#include "extensions/browser/api_activity_monitor.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/process_map.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/api/web_request/web_request_activity_log_constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"

#if BUILDFLAG(ENABLE_GUEST_VIEW)
#include "extensions/browser/guest_view/guest_view_events.h"
#endif

using content::BrowserThread;
using extension_web_request_api_helpers::ExtraInfoSpec;

using DNRRequestAction = extensions::declarative_net_request::RequestAction;

namespace extensions {

namespace {

namespace activity_log = web_request_activity_log_constants;
namespace declarative_keys = declarative_webrequest_constants;
namespace helpers = extension_web_request_api_helpers;
namespace keys = extension_web_request_api_constants;
namespace web_request = api::web_request;

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

constexpr char kEventMessage[] = "webViewInternal.onMessage";

constexpr char kWebRequestEventPrefix[] = "webRequest.";
constexpr char kWebViewEventPrefix[] = "webViewInternal.";

constexpr size_t kWebRequestEventPrefixLen =
    std::char_traits<char>::length(kWebRequestEventPrefix);
constexpr size_t kWebViewEventPrefixLen =
    std::char_traits<char>::length(kWebViewEventPrefix);

// List of all the webRequest events. Note: this doesn't include
// "onActionIgnored" which is not related to a request's lifecycle and is
// handled as a normal event (as opposed to a WebRequestEvent at the bindings
// layer).
constexpr const char* kWebRequestEvents[] = {
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

events::HistogramValue GetEventHistogramValue(const std::string& event_name) {
  // Event names will either be webRequest events, or guest view (probably web
  // view) events that map to webRequest events. Check webRequest first.
  static constexpr struct ValueAndName {
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
    if (value_and_name.event_name == event_name) {
      return value_and_name.histogram_value;
    }
  }

#if BUILDFLAG(ENABLE_GUEST_VIEW)
  // If there is no webRequest event, it might be a guest view webRequest event.
  events::HistogramValue guest_view_histogram_value =
      guest_view_events::GetEventHistogramValue(event_name);
  if (guest_view_histogram_value != events::UNKNOWN) {
    return guest_view_histogram_value;
  }
#endif

  // There is no histogram value for this event name. It should be added to
  // either the mapping here, or in guest_view_events.
  NOTREACHED_IN_MIGRATION()
      << "Event " << event_name << " must have a histogram value";
  return events::UNKNOWN;
}

const char* GetRequestStageAsString(WebRequestEventRouter::EventTypes type) {
  switch (type) {
    case WebRequestEventRouter::kInvalidEvent:
      return "Invalid";
    case WebRequestEventRouter::kOnBeforeRequest:
      return keys::kOnBeforeRequest;
    case WebRequestEventRouter::kOnBeforeSendHeaders:
      return keys::kOnBeforeSendHeaders;
    case WebRequestEventRouter::kOnSendHeaders:
      return keys::kOnSendHeaders;
    case WebRequestEventRouter::kOnHeadersReceived:
      return keys::kOnHeadersReceived;
    case WebRequestEventRouter::kOnBeforeRedirect:
      return keys::kOnBeforeRedirect;
    case WebRequestEventRouter::kOnAuthRequired:
      return keys::kOnAuthRequired;
    case WebRequestEventRouter::kOnResponseStarted:
      return keys::kOnResponseStarted;
    case WebRequestEventRouter::kOnErrorOccurred:
      return keys::kOnErrorOccurred;
    case WebRequestEventRouter::kOnCompleted:
      return keys::kOnCompleted;
  }
  NOTREACHED_IN_MIGRATION();
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
WebRequestEventRouter::EventTypes GetEventTypeFromEventName(
    std::string_view event_name) {
  constexpr auto kRequestStageMap = base::MakeFixedFlatMap<
      std::string_view, WebRequestEventRouter::EventTypes>(
      {{keys::kOnBeforeRequest, WebRequestEventRouter::kOnBeforeRequest},
       {keys::kOnBeforeSendHeaders,
        WebRequestEventRouter::kOnBeforeSendHeaders},
       {keys::kOnSendHeaders, WebRequestEventRouter::kOnSendHeaders},
       {keys::kOnHeadersReceived, WebRequestEventRouter::kOnHeadersReceived},
       {keys::kOnBeforeRedirect, WebRequestEventRouter::kOnBeforeRedirect},
       {keys::kOnAuthRequired, WebRequestEventRouter::kOnAuthRequired},
       {keys::kOnResponseStarted, WebRequestEventRouter::kOnResponseStarted},
       {keys::kOnErrorOccurred, WebRequestEventRouter::kOnErrorOccurred},
       {keys::kOnCompleted, WebRequestEventRouter::kOnCompleted}});
  static_assert(kRequestStageMap.size() == std::size(kWebRequestEvents));

  // Canonicalize the |event_name| to the request stage.
  if (base::StartsWith(event_name, kWebRequestEventPrefix)) {
    event_name.remove_prefix(kWebRequestEventPrefixLen);
  } else if (base::StartsWith(event_name, kWebViewEventPrefix)) {
    event_name.remove_prefix(kWebViewEventPrefixLen);
  } else {
    return WebRequestEventRouter::kInvalidEvent;
  }

  const auto it = kRequestStageMap.find(event_name);
  return it == kRequestStageMap.end() ? WebRequestEventRouter::kInvalidEvent
                                      : it->second;
}

bool IsWebRequestEvent(std::string_view event_name) {
  return GetEventTypeFromEventName(event_name) !=
         WebRequestEventRouter::kInvalidEvent;
}

// Returns whether |request| has been triggered by an extension enabled in
// |context|.
bool IsRequestFromExtension(const WebRequestInfo& request,
                            content::BrowserContext* context) {
  if (request.render_process_id == -1) {
    return false;
  }

  const Extension* extension =
      ProcessMap::Get(context)->GetEnabledExtensionByProcessID(
          request.render_process_id);
  return extension && !extension->is_hosted_app();
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
    const ExtensionId& extension_id,
    bool is_web_view_guest,
    int web_view_instance_id,
    std::unique_ptr<WebRequestEventDetails> event_details) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!ExtensionsBrowserClient::Get()->IsValidContext(browser_context)) {
    return;
  }

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
    event_name = kEventMessage;
  } else {
    histogram_value = events::DECLARATIVE_WEB_REQUEST_ON_MESSAGE;
    event_name = declarative_keys::kOnMessage;
  }

  auto event = std::make_unique<Event>(
      histogram_value, event_name, std::move(event_args), browser_context,
      /*restrict_to_context_type=*/std::nullopt, GURL(),
      EventRouter::USER_GESTURE_UNKNOWN, std::move(event_filtering_info));
  event_router->DispatchEventToExtension(extension_id, std::move(event));
}

// Helper to dispatch the "onActionIgnored" event.
void NotifyIgnoredActionsOnUI(
    content::BrowserContext* browser_context,
    uint64_t request_id,
    extension_web_request_api_helpers::IgnoredActions ignored_actions) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!ExtensionsBrowserClient::Get()->IsValidContext(browser_context)) {
    return;
  }

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

// We hide events from the system context as well as sensitive requests.
bool ShouldHideEvent(content::BrowserContext* browser_context,
                     const WebRequestInfo& request) {
  return !browser_context ||
         WebRequestPermissions::HideRequest(
             PermissionHelper::Get(browser_context), request);
}

// Returns event details for a given request.
std::unique_ptr<WebRequestEventDetails> CreateEventDetails(
    const WebRequestInfo& request,
    int extra_info_spec) {
  return std::make_unique<WebRequestEventDetails>(request, extra_info_spec);
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

  if (extra_info_spec & ExtraInfoSpec::REQUEST_HEADERS) {
    LogEventListenerFlag(WebRequestEventListenerFlag::kRequestHeaders);
  }
  if (extra_info_spec & ExtraInfoSpec::RESPONSE_HEADERS) {
    LogEventListenerFlag(WebRequestEventListenerFlag::kResponseHeaders);
  }
  if (extra_info_spec & ExtraInfoSpec::BLOCKING) {
    LogEventListenerFlag(WebRequestEventListenerFlag::kBlocking);
  }
  if (extra_info_spec & ExtraInfoSpec::ASYNC_BLOCKING) {
    LogEventListenerFlag(WebRequestEventListenerFlag::kAsyncBlocking);
  }
  if (extra_info_spec & ExtraInfoSpec::REQUEST_BODY) {
    LogEventListenerFlag(WebRequestEventListenerFlag::kRequestBody);
  }
  if (extra_info_spec & ExtraInfoSpec::EXTRA_HEADERS) {
    LogEventListenerFlag(WebRequestEventListenerFlag::kExtraHeaders);
  }
}

// Helper to record a matched DNR action in RulesetManager's ActionTracker.
void OnDNRActionMatched(content::BrowserContext* browser_context,
                        const WebRequestInfo& request,
                        const DNRRequestAction& action) {
  if (action.tracked) {
    return;
  }

  declarative_net_request::ActionTracker& action_tracker =
      declarative_net_request::RulesMonitorService::Get(browser_context)
          ->action_tracker();

  action_tracker.OnRuleMatched(action, request);
  action.tracked = true;

  // If `action` is tracked and it may match an entry in
  // `request.max_priority_allow_action`, the entry doesn't need to have its
  // `tracked` updated.
  // `request.ShouldRecordMatchedAllowRuleInOnHeadersReceived` will only record
  // an allow rule matched in OnHeadersReceived with a greater priority than one
  // matched in OnBeforeRequest.
}

// The `use_dynamic_url` feature for web accessible resources requires that the
// requested url be a dynamic url. A dynamic url is one where a session GUID is
// used for the host instead of the static extension id.
GURL GetNewUrl(const GURL& redirect_url,
               content::BrowserContext* browser_context) {
  auto dynamic_url =
      TransformToDynamicURLIfNecessary(redirect_url, browser_context);
  return dynamic_url.value_or(redirect_url);
}

using CallbacksForPageLoad = std::list<base::OnceClosure>;

// TODO(crbug.com/40264286): We need to investigate why this is a global
// structure instead of a per-BrowserContext structure. It seems incorrect
// that a page load in one BrowserContext should interact with a page load
// in another one.
CallbacksForPageLoad& GetCallbacksForPageLoad() {
  static base::NoDestructor<CallbacksForPageLoad> instance;
  return *instance.get();
}

ExtensionWebRequestTimeTracker& GetExtensionWebRequestTimeTracker() {
  static base::NoDestructor<ExtensionWebRequestTimeTracker> instance;
  return *instance.get();
}

class CrossContextData {
 public:
  CrossContextData() = default;
  ~CrossContextData() = default;
  CrossContextData(const CrossContextData&) = delete;
  CrossContextData& operator=(const CrossContextData&) = delete;

  static CrossContextData& Get() {
    static base::NoDestructor<CrossContextData> instance;
    return *instance.get();
  }

  content::BrowserContext* GetCrossBrowserContext(
      content::BrowserContext* browser_context) {
    const auto it = cross_context_data_.find(browser_context);
    return it == cross_context_data_.end() ? nullptr : it->second;
  }

  void AddContext(content::BrowserContext* original_browser_context,
                  content::BrowserContext* otr_browser_context) {
    cross_context_data_[original_browser_context] = otr_browser_context;
    cross_context_data_[otr_browser_context] = original_browser_context;
  }

  void RemoveContext(content::BrowserContext* browser_context) {
    // This context can be either the original one, or the OTR one. Either
    // way, we need to remove both entries.
    auto it = cross_context_data_.find(browser_context);
    if (it != cross_context_data_.end()) {
      cross_context_data_.erase(it->second);
      cross_context_data_.erase(it);
    }
  }

 private:
  using CrossContextMap =
      std::map<content::BrowserContext*,
               raw_ptr<content::BrowserContext, CtnExperimental>>;

  // For each each on-the-record context that has an off-the-record context,
  // this bi-map contains an entry for both contexts where the value is the
  // other context.
  CrossContextMap cross_context_data_;
};

void ClearCrossContextData(content::BrowserContext* browser_context) {
  CrossContextData::Get().RemoveContext(browser_context);
}

}  // namespace

WebRequestEventRouter::WebRequestEventRouter(content::BrowserContext* context)
    : browser_context_(context) {}

WebRequestEventRouter::~WebRequestEventRouter() = default;

void WebRequestEventRouter::Shutdown() {
  // TODO(crbug.com/40264286): This overlaps with OnOTRBrowserContextDestroyed.
  // We should decide whether this can be cleaned up.
  OnBrowserContextShutdown(browser_context_);
  ClearCrossContextData(browser_context_);
}

// static
WebRequestEventRouter* WebRequestEventRouter::Get(
    content::BrowserContext* browser_context) {
  return WebRequestEventRouterFactory::GetForBrowserContext(browser_context);
}

// static
std::vector<std::string> WebRequestEventRouter::GetEventNames() {
  std::vector<std::string> result;
  result.reserve(std::size(kWebRequestEvents) * 2);
  for (std::string event_name : kWebRequestEvents) {
    // The webRequest event.
    result.push_back(event_name);

    // The corresponding webview event name.
    event_name.replace(0, kWebRequestEventPrefixLen, kWebViewEventPrefix);
    result.push_back(event_name);
  }

  return result;
}

// Represents a single unique listener to an event, along with whatever filter
// parameters and extra_info_spec were specified at the time the listener was
// added.
// NOTE(benjhayden) New APIs should not use this sub_event_name trick! It does
// not play well with event pages. See downloads.onDeterminingFilename and
// ExtensionDownloadsEventRouter for an alternative approach.
WebRequestEventRouter::EventListener::EventListener(ID id)
    : id(std::move(id)) {}
WebRequestEventRouter::EventListener::~EventListener() = default;

// Contains info about requests that are blocked waiting for a response from
// an extension.
struct WebRequestEventRouter::BlockedRequest {
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

namespace {

helpers::EventResponseDelta CalculateDelta(
    content::BrowserContext* browser_context,
    WebRequestEventRouter::BlockedRequest* blocked_request,
    WebRequestEventRouter::EventResponse* response,
    int extra_info_spec) {
  switch (blocked_request->event) {
    case WebRequestEventRouter::kOnBeforeRequest:
      return helpers::CalculateOnBeforeRequestDelta(
          response->extension_id, response->extension_install_time,
          response->cancel, response->new_url);
    case WebRequestEventRouter::kOnBeforeSendHeaders: {
      net::HttpRequestHeaders* old_headers = blocked_request->request_headers;
      net::HttpRequestHeaders* new_headers = response->request_headers.get();
      return helpers::CalculateOnBeforeSendHeadersDelta(
          browser_context, response->extension_id,
          response->extension_install_time, response->cancel, old_headers,
          new_headers, extra_info_spec);
    }
    case WebRequestEventRouter::kOnHeadersReceived: {
      const net::HttpResponseHeaders* old_headers =
          blocked_request->original_response_headers.get();
      helpers::ResponseHeaders* new_headers = response->response_headers.get();
      return helpers::CalculateOnHeadersReceivedDelta(
          response->extension_id, response->extension_install_time,
          response->cancel, blocked_request->request->url, response->new_url,
          old_headers, new_headers, extra_info_spec);
    }
    case WebRequestEventRouter::kOnAuthRequired:
      return helpers::CalculateOnAuthRequiredDelta(
          response->extension_id, response->extension_install_time,
          response->cancel, response->auth_credentials);
    default:
      NOTREACHED_IN_MIGRATION();
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
  if (delta.cancel) {
    details.Set(activity_log::kCancelKey, true);
  }
  if (delta.new_url.is_valid()) {
    details.Set(activity_log::kNewUrlKey, delta.new_url.spec());
  }

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

bool WebRequestEventRouter::RequestFilter::InitFromValue(
    const base::Value::Dict& value,
    std::string* error) {
  if (!value.Find("urls")) {
    return false;
  }

  for (const auto dict_item : value) {
    if (dict_item.first == "urls" && dict_item.second.is_list()) {
      for (const auto& item : dict_item.second.GetList()) {
        std::string url;
        URLPattern pattern(URLPattern::SCHEME_HTTP | URLPattern::SCHEME_HTTPS |
                           URLPattern::SCHEME_FTP | URLPattern::SCHEME_FILE |
                           URLPattern::SCHEME_EXTENSION |
                           URLPattern::SCHEME_WS | URLPattern::SCHEME_WSS |
                           URLPattern::SCHEME_UUID_IN_PACKAGE);
        if (item.is_string()) {
          url = item.GetString();
        }

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
        if (type.is_string()) {
          type_str = type.GetString();
        }
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

WebRequestEventRouter::EventResponse::EventResponse(
    const ExtensionId& extension_id,
    const base::Time& extension_install_time)
    : extension_id(extension_id),
      extension_install_time(extension_install_time),
      cancel(false) {}

WebRequestEventRouter::EventResponse::~EventResponse() = default;

WebRequestEventRouter::RequestFilter::RequestFilter()
    : tab_id(-1), window_id(-1) {}
WebRequestEventRouter::RequestFilter::~RequestFilter() = default;

WebRequestEventRouter::RequestFilter::RequestFilter(RequestFilter&& other) =
    default;
WebRequestEventRouter::RequestFilter&
WebRequestEventRouter::RequestFilter::operator=(RequestFilter&& other) =
    default;

WebRequestEventRouter::SignaledRequestIDTracker::SignaledRequestIDTracker() =
    default;
WebRequestEventRouter::SignaledRequestIDTracker::~SignaledRequestIDTracker() =
    default;
WebRequestEventRouter::SignaledRequestIDTracker::SignaledRequestIDTracker(
    SignaledRequestIDTracker&&) = default;

bool WebRequestEventRouter::SignaledRequestIDTracker::GetAndSet(
    uint64_t request_id,
    EventTypes event_type) {
  auto iter = signaled_requests_.find(request_id);
  if (iter == signaled_requests_.end()) {
    signaled_requests_[request_id] = event_type;
    return false;
  }
  bool was_signaled_before = iter->second & event_type;
  iter->second |= event_type;
  return was_signaled_before;
}

void WebRequestEventRouter::SignaledRequestIDTracker::ClearEventType(
    uint64_t request_id,
    EventTypes event_type) {
  auto iter = signaled_requests_.find(request_id);
  if (iter != signaled_requests_.end()) {
    iter->second &= ~event_type;
  }
}

WebRequestEventRouter::BrowserContextData::BrowserContextData() = default;
WebRequestEventRouter::BrowserContextData::BrowserContextData(
    BrowserContextData&&) = default;
WebRequestEventRouter::BrowserContextData::~BrowserContextData() = default;

WebRequestEventRouter::EventListener::ID::ID(
    content::BrowserContext* browser_context,
    const ExtensionId& extension_id,
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

WebRequestEventRouter::EventListener::ID::ID(const ID& source) = default;
WebRequestEventRouter::EventListener::ID::ID(ID&& source) = default;

bool WebRequestEventRouter::EventListener::ID::operator==(
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

//
//  WebRequestEventRouter
//

void WebRequestEventRouter::RegisterRulesRegistry(
    content::BrowserContext* browser_context,
    int rules_registry_id,
    scoped_refptr<WebRequestRulesRegistry> rules_registry) {
  BrowserContextData& data = data_[GetBrowserContextID(browser_context)];
  if (rules_registry.get()) {
    data.rules_registries[rules_registry_id] = rules_registry;
  } else {
    data.rules_registries.erase(rules_registry_id);
  }
}

int WebRequestEventRouter::OnBeforeRequest(
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

  if (IsPageLoad(*request)) {
    NotifyPageLoad();
  }

  bool has_listener = false;
  for (const auto& kv :
       data_[GetBrowserContextID(browser_context)].active_listeners) {
    if (!kv.second.empty()) {
      has_listener = true;
      break;
    }
  }
  GetExtensionWebRequestTimeTracker().LogRequestStartTime(
      request->id, base::TimeTicks::Now(), has_listener,
      HasExtraHeadersListenerForRequest(browser_context, request));

  const bool is_incognito_context = browser_context->IsOffTheRecord();

  // CRX requests information can be intercepted here.
  // May be null for browser-initiated requests such as navigations.
  if (request->initiator) {
    const std::string& scheme = request->initiator->scheme();
    const ExtensionId& extension_id = request->initiator->host();
    const GURL& request_url = request->url;
    if (scheme == extensions::kExtensionScheme) {
      ExtensionsBrowserClient::Get()->NotifyExtensionRemoteHostContacted(
          browser_context, extension_id, request_url);
    }
  }

  // Whether to initialized `blocked_requests_`.
  bool initialize_blocked_requests = false;

  initialize_blocked_requests |= ProcessDeclarativeRules(
      browser_context, web_request::OnBeforeRequest::kEventName, request,
      ON_BEFORE_REQUEST, nullptr);

  int extra_info_spec = 0;
  RawListeners listeners = GetMatchingListeners(
      browser_context, web_request::OnBeforeRequest::kEventName, request,
      &extra_info_spec);
  if (!listeners.empty() &&
      !GetAndSetSignaled(browser_context, request->id, kOnBeforeRequest)) {
    std::unique_ptr<WebRequestEventDetails> event_details(
        CreateEventDetails(*request, extra_info_spec));
    event_details->SetRequestBody(request);

    GetExtensionWebRequestTimeTracker().LogBeforeRequestDispatchTime(
        request->id, base::TimeTicks::Now());

    initialize_blocked_requests |= DispatchEvent(
        browser_context, request, listeners, std::move(event_details));
  }

  // Handle Declarative Net Request API rules matched in this request phase.
  // In case the request is blocked or redirected, we un-block the request and
  // ignore any subsequent responses from webRequestBlocking listeners. Note: We
  // don't remove the request from the `EventListener::blocked_requests` set of
  // any blocking listeners it was dispatched to, since the listener's response
  // will be ignored in `DecrementBlockCount` anyway.
  declarative_net_request::RulesetManager* ruleset_manager =
      declarative_net_request::RulesMonitorService::Get(browser_context)
          ->ruleset_manager();

  if (ruleset_manager->HasRulesets(
          declarative_net_request::RulesetMatchingStage::kOnBeforeRequest)) {
    GetExtensionWebRequestTimeTracker().LogBeforeRequestDNRStartTime(
        request->id, base::TimeTicks::Now());

    auto record_completion_time = [](ExtensionWebRequestTimeTracker* tracker,
                                     int64_t request_id) {
      tracker->LogBeforeRequestDNRCompletionTime(request_id,
                                                 base::TimeTicks::Now());
    };

    const std::vector<DNRRequestAction>& actions =
        ruleset_manager->EvaluateBeforeRequest(*request, is_incognito_context);
    base::ScopedClosureRunner scoped_timer;
    if (!actions.empty()) {
      // We only record completion time if there's at least one relevant rule.
      // Otherwise, we'd record evaluation for every request even if the user
      // only had a single rule added.
      scoped_timer = base::ScopedClosureRunner(
          base::BindOnce(record_completion_time,
                         &GetExtensionWebRequestTimeTracker(), request->id));

      // Only the first action in `actions` need to be checked, as all action
      // types except MODIFY_HEADERS expect only one action, and for
      // MODIFY_HEADERS, a check is performed to make sure all `actions` are
      // MODIFY_HEADERS.
      const DNRRequestAction& action = actions[0];
      switch (action.type) {
        case DNRRequestAction::Type::BLOCK:
          ClearPendingCallbacks(browser_context, *request);
          DCHECK_EQ(1u, actions.size());
          OnDNRActionMatched(browser_context, *request, action);
          return net::ERR_BLOCKED_BY_CLIENT;
        case DNRRequestAction::Type::COLLAPSE:
          ClearPendingCallbacks(browser_context, *request);
          DCHECK_EQ(1u, actions.size());
          OnDNRActionMatched(browser_context, *request, action);
          *should_collapse_initiator = true;
          return net::ERR_BLOCKED_BY_CLIENT;
        case DNRRequestAction::Type::ALLOW:
        case DNRRequestAction::Type::ALLOW_ALL_REQUESTS:
          DCHECK_EQ(1u, actions.size());
          OnDNRActionMatched(browser_context, *request, action);
          break;
        case DNRRequestAction::Type::REDIRECT:
        case DNRRequestAction::Type::UPGRADE:
          ClearPendingCallbacks(browser_context, *request);
          DCHECK_EQ(1u, actions.size());
          DCHECK(action.redirect_url);
          OnDNRActionMatched(browser_context, *request, action);
          *new_url = GetNewUrl(action.redirect_url.value(), browser_context);
          // Collect redirect action data for the Extension Telemetry Service.
          if (action.type == DNRRequestAction::Type::REDIRECT) {
            ExtensionsBrowserClient::Get()
                ->NotifyExtensionDeclarativeNetRequestRedirectAction(
                    browser_context, action.extension_id, request->url,
                    action.redirect_url.value());
          }

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

  if (!initialize_blocked_requests) {
    return net::OK;  // Nobody saw a reason for modifying the request.
  }

  BlockedRequest& blocked_request =
      GetOrAddBlockedRequest(browser_context, request->id);
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

int WebRequestEventRouter::OnBeforeSendHeaders(
    content::BrowserContext* browser_context,
    const WebRequestInfo* request,
    BeforeSendHeadersCallback callback,
    net::HttpRequestHeaders* headers) {
  if (ShouldHideEvent(browser_context, *request)) {
    return net::OK;
  }

  bool initialize_blocked_requests = false;

  initialize_blocked_requests |=
      ProcessDeclarativeRules(browser_context, keys::kOnBeforeSendHeadersEvent,
                              request, ON_BEFORE_SEND_HEADERS, nullptr);

  CHECK(request->dnr_actions);
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
      !GetAndSetSignaled(browser_context, request->id, kOnBeforeSendHeaders)) {
    std::unique_ptr<WebRequestEventDetails> event_details(
        CreateEventDetails(*request, extra_info_spec));
    event_details->SetRequestHeaders(*headers);

    initialize_blocked_requests |= DispatchEvent(
        browser_context, request, listeners, std::move(event_details));
  }

  if (!initialize_blocked_requests) {
    return net::OK;  // Nobody saw a reason for modifying the request.
  }

  BlockedRequest& blocked_request =
      GetOrAddBlockedRequest(browser_context, request->id);
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

void WebRequestEventRouter::OnSendHeaders(
    content::BrowserContext* browser_context,
    const WebRequestInfo* request,
    const net::HttpRequestHeaders& headers) {
  if (ShouldHideEvent(browser_context, *request)) {
    return;
  }

  if (GetAndSetSignaled(browser_context, request->id, kOnSendHeaders)) {
    return;
  }

  ClearSignaled(browser_context, request->id, kOnBeforeRedirect);

  int extra_info_spec = 0;
  RawListeners listeners = GetMatchingListeners(
      browser_context, keys::kOnSendHeadersEvent, request, &extra_info_spec);
  if (listeners.empty()) {
    return;
  }

  std::unique_ptr<WebRequestEventDetails> event_details(
      CreateEventDetails(*request, extra_info_spec));
  event_details->SetRequestHeaders(headers);

  DispatchEvent(browser_context, request, listeners, std::move(event_details));
}

int WebRequestEventRouter::OnHeadersReceived(
    content::BrowserContext* browser_context,
    WebRequestInfo* request,
    net::CompletionOnceCallback callback,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    GURL* preserve_fragment_on_redirect_url,
    bool* should_collapse_initiator) {
  CHECK(should_collapse_initiator);

  if (ShouldHideEvent(browser_context, *request)) {
    return net::OK;
  }

  bool initialize_blocked_requests = false;
  const bool is_incognito_context = browser_context->IsOffTheRecord();

  CHECK(request->dnr_actions);

  initialize_blocked_requests |= ProcessDeclarativeRules(
      browser_context, keys::kOnHeadersReceivedEvent, request,
      ON_HEADERS_RECEIVED, original_response_headers);

  int extra_info_spec = 0;
  RawListeners listeners =
      GetMatchingListeners(browser_context, keys::kOnHeadersReceivedEvent,
                           request, &extra_info_spec);

  if (!listeners.empty() &&
      !GetAndSetSignaled(browser_context, request->id, kOnHeadersReceived)) {
    std::unique_ptr<WebRequestEventDetails> event_details(
        CreateEventDetails(*request, extra_info_spec));
    event_details->SetResponseHeaders(*request, original_response_headers);

    initialize_blocked_requests |= DispatchEvent(
        browser_context, request, listeners, std::move(event_details));
  }

  declarative_net_request::RulesetManager* ruleset_manager =
      declarative_net_request::RulesMonitorService::Get(browser_context)
          ->ruleset_manager();

  if (ruleset_manager->HasRulesets(
          declarative_net_request::RulesetMatchingStage::kOnHeadersReceived)) {
    std::vector<DNRRequestAction> actions =
        ruleset_manager->EvaluateRequestWithHeaders(
            *request, original_response_headers, is_incognito_context);

    // TODO(crbug.com/40727004): This shares a lot of logic with the equivalent
    // block in OnBeforeRequest. Refactor into a common method once all action
    // types are supported.
    if (!actions.empty()) {
      // Similar to OnBeforeRequest, only the first action needs to be examined.
      // In the case of MODIFY_HEADERS, any operations needed to re-compute
      // `request->dnr_actions` only needs to be executed once.
      const DNRRequestAction& action = actions[0];

      switch (action.type) {
        case DNRRequestAction::Type::BLOCK:
          ClearPendingCallbacks(browser_context, *request);
          DCHECK_EQ(1u, actions.size());
          OnDNRActionMatched(browser_context, *request, action);
          return net::ERR_BLOCKED_BY_CLIENT;
        case DNRRequestAction::Type::COLLAPSE:
          ClearPendingCallbacks(browser_context, *request);
          DCHECK_EQ(1u, actions.size());
          OnDNRActionMatched(browser_context, *request, action);
          *should_collapse_initiator = true;
          return net::ERR_BLOCKED_BY_CLIENT;
        case DNRRequestAction::Type::ALLOW:
        case DNRRequestAction::Type::ALLOW_ALL_REQUESTS:
          DCHECK_EQ(1u, actions.size());
          // Prune any actions matched during previous request stages that are
          // outprioritized by allow rules matched during this request stage.
          request->EraseOutprioritizedDNRActions();

          if (request->ShouldRecordMatchedAllowRuleInOnHeadersReceived(
                  action)) {
            OnDNRActionMatched(browser_context, *request, action);
          }

          break;
        case DNRRequestAction::Type::REDIRECT:
        case DNRRequestAction::Type::UPGRADE:
          ClearPendingCallbacks(browser_context, *request);
          DCHECK_EQ(1u, actions.size());
          DCHECK(action.redirect_url);
          OnDNRActionMatched(browser_context, *request, action);

          if (!override_response_headers->get()) {
            *override_response_headers =
                base::MakeRefCounted<net::HttpResponseHeaders>(
                    original_response_headers->raw_headers());
          }

          extension_web_request_api_helpers::
              RedirectRequestAfterHeadersReceived(
                  GetNewUrl(action.redirect_url.value(), browser_context),
                  **override_response_headers,
                  preserve_fragment_on_redirect_url);
          return net::OK;
        case DNRRequestAction::Type::MODIFY_HEADERS:
          // Modify header actions can only combine with actions of the same
          // type, see RulesetManager::EvaluateRequestInternal for the
          // implementation.
          DCHECK(base::ranges::all_of(actions, [](const auto& action) {
            return action.type == DNRRequestAction::Type::MODIFY_HEADERS;
          }));

          // Prune any actions matched during previous request stages that are
          // outprioritized by allow rules matched during this request stage.
          request->EraseOutprioritizedDNRActions();

          // For other action types, actions matched here don't need to be saved
          // to `request->dnr_actions` since said action(s) can be taken on the
          // request now. Modify header actions need to be saved since they will
          // take effect later.
          // Note: Since `actions` will be moved here either way, `action` is
          // unsafe to use after this point!

          // If no modify header actions were matched in previous request
          // stages, then `request->dnr_actions` can simply be overwritten by
          // actions matched at this stage.
          if (request->dnr_actions->empty() ||
              (*request->dnr_actions)[0].type !=
                  DNRRequestAction::Type::MODIFY_HEADERS) {
            request->dnr_actions = std::move(actions);
          } else {
            // Otherwise, modify header actions from all request stages will
            // need to be merged. MergeModifyHeaderActions will also sort these
            // actions by descending order of priority.
            request->dnr_actions = ruleset_manager->MergeModifyHeaderActions(
                std::move(*request->dnr_actions), std::move(actions));

            // Verify that if `request->dnr_actions` contains any modify headers
            // actions, then all actions in `request->dnr_actions` must be
            // modify headers actions.
            DCHECK(base::ranges::all_of(
                *request->dnr_actions, [](const auto& action) {
                  return action.type == DNRRequestAction::Type::MODIFY_HEADERS;
                }));
          }

          break;
      }
    }
  }

  initialize_blocked_requests |= base::ranges::any_of(
      *request->dnr_actions, [](const DNRRequestAction& action) {
        return action.type == DNRRequestAction::Type::MODIFY_HEADERS &&
               !action.response_headers_to_modify.empty();
      });

  if (!initialize_blocked_requests) {
    return net::OK;  // Nobody saw a reason for modifying the request.
  }

  BlockedRequest& blocked_request =
      GetOrAddBlockedRequest(browser_context, request->id);
  blocked_request.event = kOnHeadersReceived;
  blocked_request.is_incognito |= is_incognito_context;
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

WebRequestEventRouter::AuthRequiredResponse
WebRequestEventRouter::OnAuthRequired(content::BrowserContext* browser_context,
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
  if (listeners.empty()) {
    return AuthRequiredResponse::AUTH_REQUIRED_RESPONSE_NO_ACTION;
  }

  std::unique_ptr<WebRequestEventDetails> event_details(
      CreateEventDetails(*request, extra_info_spec));
  event_details->SetResponseHeaders(*request, request->response_headers.get());
  event_details->SetAuthInfo(auth_info);

  if (DispatchEvent(browser_context, request, listeners,
                    std::move(event_details))) {
    BlockedRequest& blocked_request =
        GetOrAddBlockedRequest(browser_context, request->id);
    blocked_request.event = kOnAuthRequired;
    blocked_request.is_incognito |= browser_context->IsOffTheRecord();
    blocked_request.request = request;
    blocked_request.auth_callback = std::move(callback);
    blocked_request.auth_credentials = credentials;
    return AuthRequiredResponse::AUTH_REQUIRED_RESPONSE_IO_PENDING;
  }
  return AuthRequiredResponse::AUTH_REQUIRED_RESPONSE_NO_ACTION;
}

void WebRequestEventRouter::OnBeforeRedirect(
    content::BrowserContext* browser_context,
    const WebRequestInfo* request,
    const GURL& new_location) {
  if (ShouldHideEvent(browser_context, *request)) {
    return;
  }

  if (GetAndSetSignaled(browser_context, request->id, kOnBeforeRedirect)) {
    return;
  }

  ClearSignaled(browser_context, request->id, kOnBeforeRequest);
  ClearSignaled(browser_context, request->id, kOnBeforeSendHeaders);
  ClearSignaled(browser_context, request->id, kOnSendHeaders);
  ClearSignaled(browser_context, request->id, kOnHeadersReceived);

  int extra_info_spec = 0;
  RawListeners listeners = GetMatchingListeners(
      browser_context, keys::kOnBeforeRedirectEvent, request, &extra_info_spec);
  if (listeners.empty()) {
    return;
  }

  std::unique_ptr<WebRequestEventDetails> event_details(
      CreateEventDetails(*request, extra_info_spec));
  event_details->SetResponseHeaders(*request, request->response_headers.get());
  event_details->SetResponseSource(*request);
  event_details->SetString(keys::kRedirectUrlKey, new_location.spec());

  DispatchEvent(browser_context, request, listeners, std::move(event_details));
}

void WebRequestEventRouter::OnResponseStarted(
    content::BrowserContext* browser_context,
    const WebRequestInfo* request,
    int net_error) {
  DCHECK_NE(net::ERR_IO_PENDING, net_error);

  if (ShouldHideEvent(browser_context, *request)) {
    return;
  }

  // OnResponseStarted is even triggered, when the request was cancelled.
  if (net_error != net::OK) {
    return;
  }

  int extra_info_spec = 0;
  RawListeners listeners =
      GetMatchingListeners(browser_context, keys::kOnResponseStartedEvent,
                           request, &extra_info_spec);
  if (listeners.empty()) {
    return;
  }

  std::unique_ptr<WebRequestEventDetails> event_details(
      CreateEventDetails(*request, extra_info_spec));
  event_details->SetResponseHeaders(*request, request->response_headers.get());
  event_details->SetResponseSource(*request);

  DispatchEvent(browser_context, request, listeners, std::move(event_details));
}

void WebRequestEventRouter::OnCompleted(
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
       !WasSignaled(browser_context, request->id))) {
    return;
  }

  GetExtensionWebRequestTimeTracker().LogRequestEndTime(request->id,
                                                        base::TimeTicks::Now());

  // See comment in OnErrorOccurred regarding net::ERR_WS_UPGRADE.
  DCHECK(net_error == net::OK || net_error == net::ERR_WS_UPGRADE);

  DCHECK(!GetAndSetSignaled(browser_context, request->id, kOnCompleted));

  ClearPendingCallbacks(browser_context, *request);

  int extra_info_spec = 0;
  RawListeners listeners = GetMatchingListeners(
      browser_context, keys::kOnCompletedEvent, request, &extra_info_spec);
  if (listeners.empty()) {
    return;
  }

  std::unique_ptr<WebRequestEventDetails> event_details(
      CreateEventDetails(*request, extra_info_spec));
  event_details->SetResponseHeaders(*request, request->response_headers.get());
  event_details->SetResponseSource(*request);

  DispatchEvent(browser_context, request, listeners, std::move(event_details));
}

void WebRequestEventRouter::OnErrorOccurred(
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
       !WasSignaled(browser_context, request->id))) {
    return;
  }

  GetExtensionWebRequestTimeTracker().LogRequestEndTime(request->id,
                                                        base::TimeTicks::Now());

  DCHECK_NE(net::OK, net_error);
  DCHECK_NE(net::ERR_IO_PENDING, net_error);

  DCHECK(!GetAndSetSignaled(browser_context, request->id, kOnErrorOccurred));

  ClearPendingCallbacks(browser_context, *request);

  int extra_info_spec = 0;
  RawListeners listeners = GetMatchingListeners(
      browser_context, web_request::OnErrorOccurred::kEventName, request,
      &extra_info_spec);
  if (listeners.empty()) {
    return;
  }

  std::unique_ptr<WebRequestEventDetails> event_details(
      CreateEventDetails(*request, extra_info_spec));
  if (started) {
    event_details->SetResponseSource(*request);
  } else {
    event_details->SetBoolean(keys::kFromCache, request->response_from_cache);
  }
  event_details->SetString(keys::kErrorKey, net::ErrorToString(net_error));

  DispatchEvent(browser_context, request, listeners, std::move(event_details));
}

void WebRequestEventRouter::OnRequestWillBeDestroyed(
    content::BrowserContext* browser_context,
    const WebRequestInfo* request) {
  ClearPendingCallbacks(browser_context, *request);
  GetSignaledRequestIDTracker(browser_context).ClearRequest(request->id);
  GetExtensionWebRequestTimeTracker().LogRequestEndTime(request->id,
                                                        base::TimeTicks::Now());
}

void WebRequestEventRouter::ClearPendingCallbacks(
    content::BrowserContext* browser_context,
    const WebRequestInfo& request) {
  ClearBlockedRequest(browser_context, request.id);
}

bool WebRequestEventRouter::DispatchEvent(
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
    if (listener->IsBlocking()) {
      listener->blocked_requests.insert(request->id);
      ++num_handlers_blocking;
    }
  }

  DispatchEventToListeners(browser_context, std::move(listeners_to_dispatch),
                           request->id, std::move(event_details));

  if (num_handlers_blocking > 0) {
    BlockedRequest& blocked_request =
        GetOrAddBlockedRequest(browser_context, request->id);
    blocked_request.request = request;
    blocked_request.is_incognito |= browser_context->IsOffTheRecord();
    blocked_request.num_handlers_blocking += num_handlers_blocking;
    blocked_request.blocking_time = base::Time::Now();
    return true;
  }

  return false;
}

void WebRequestEventRouter::DispatchEventToListeners(
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
  content::BrowserContext* const cross_context =
      GetCrossBrowserContext(browser_context);
  if (cross_context) {
    auto& cross_data = data_[GetBrowserContextID(cross_context)];
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
    if (!listener) {
      continue;
    }

    DCHECK(listener->id == id);

    // Check that the listener's process is also still valid. This only applies
    // for active listeners.
    content::RenderProcessHost* render_process = nullptr;
    if (is_active) {
      render_process = content::RenderProcessHost::FromID(id.render_process_id);
      if (!render_process) {
        continue;  // The process for an active listener shut down. Bail.
      }
    }

    // Filter out the optional keys that this listener didn't request.
    base::Value::List args_filtered;

    args_filtered.Append(event_details->GetFilteredDict(
        listener->extra_info_spec, PermissionHelper::Get(browser_context),
        listener->id.extension_id, crosses_incognito));

    if (is_active) {
      DCHECK(render_process);
      // TODO(devlin): Upgrade this to a CHECK().
      if (ExtensionsBrowserClient::Get()->IsValidContext(browser_context)) {
        // Active listeners use a bespoke dispatching mechanism.
        // TODO(devlin): Now that the webRequest API is entirely handled on the
        // UI thread (it used to be on the IO thread), can we just use the
        // regular event dispatching code for this case, as well?
        EventRouter::Get(id.browser_context)
            ->DispatchEventToSender(
                render_process, id.browser_context,
                /*host_id=*/
                mojom::HostID(mojom::HostID::HostType::kExtensions,
                              listener->id.extension_id),
                listener->histogram_value, listener->id.sub_event_name,
                listener->id.worker_thread_id,
                listener->id.service_worker_version_id,
                std::move(args_filtered), mojom::EventFilteringInfo::New());
      }
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
      event->cannot_dispatch_callback = base::BindRepeating(
          &WebRequestEventRouter::OnEventHandled,
          weak_ptr_factory_.GetWeakPtr(), id.browser_context, id.extension_id,
          event_name, id.sub_event_name, request_id, id.render_process_id,
          id.web_view_instance_id, id.worker_thread_id,
          id.service_worker_version_id, nullptr);
      EventRouter::Get(id.browser_context)
          ->DispatchEventToExtension(id.extension_id, std::move(event));
    }
  }
}

void WebRequestEventRouter::OnEventHandled(
    content::BrowserContext* browser_context,
    const ExtensionId& extension_id,
    const std::string& event_name,
    const std::string& sub_event_name,
    uint64_t request_id,
    int render_process_id,
    int web_view_instance_id,
    int worker_thread_id,
    int64_t service_worker_version_id,
    std::unique_ptr<EventResponse> response) {
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
                      std::move(response), listener->extra_info_spec);
}

bool WebRequestEventRouter::AddEventListener(
    content::BrowserContext* browser_context,
    const ExtensionId& extension_id,
    const std::string& extension_name,
    const std::string& event_name,
    const std::string& sub_event_name,
    RequestFilter filter,
    int extra_info_spec,
    int render_process_id,
    int web_view_instance_id,
    int worker_thread_id,
    int64_t service_worker_version_id) {
  if (!IsWebRequestEvent(event_name)) {
    return false;
  }

  if (event_name != EventRouter::GetBaseEventName(sub_event_name)) {
    return false;
  }

  EventListener::ID id(browser_context, extension_id, sub_event_name,
                       render_process_id, web_view_instance_id,
                       worker_thread_id, service_worker_version_id);
  if (FindEventListener(id) != nullptr) {
    // This is likely an abuse of the API by a malicious extension.
    return false;
  }

  std::unique_ptr<EventListener> listener =
      std::make_unique<EventListener>(std::move(id));
  listener->extension_name = extension_name;
  listener->histogram_value = GetEventHistogramValue(event_name);
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
    size_t erased = std::erase_if(
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

  // If the listener was previously registered, there's no need to adjust the
  // extra headers count.
  if (!is_reactivated && listener->HasExtraHeaders()) {
    IncrementExtraHeadersListenerCount(browser_context);
  }

  data_[browser_context_id].active_listeners[event_name].push_back(
      std::move(listener));

  return true;
}

WebRequestEventRouter::EventListener* WebRequestEventRouter::FindEventListener(
    const EventListener::ID& id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::string event_name = EventRouter::GetBaseEventName(id.sub_event_name);
  Listeners& listeners = data_[GetBrowserContextID(id.browser_context.get())]
                             .active_listeners[event_name];
  return FindEventListenerInContainer(id, listeners);
}

WebRequestEventRouter::EventListener*
WebRequestEventRouter::FindEventListenerInContainer(
    const EventListener::ID& id,
    const Listeners& listeners) {
  auto it = std::find_if(listeners.begin(), listeners.end(),
                         [&id](const auto& entry) { return entry->id == id; });
  return it != listeners.end() ? it->get() : nullptr;
}

// static
std::unique_ptr<WebRequestEventRouter::EventListener>
WebRequestEventRouter::RemoveMatchingListener(
    Listeners& listeners,
    const ExtensionId& extension_id,
    const std::string& sub_event_name,
    std::optional<int> worker_thread_id,
    std::optional<int64_t> service_worker_version_id,
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

void WebRequestEventRouter::RemoveLazyListener(
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
    auto listener =
        RemoveMatchingListener(listeners, extension_id, sub_event_name,
                               std::nullopt, std::nullopt, browser_context_id);
    if (listener) {
      removed_listeners.push_back(std::move(listener));
    }
  };

  check_list(data.active_listeners[event_name], original_context_id);
  check_list(data.inactive_listeners[event_name], original_context_id);
  content::BrowserContext* const cross_context =
      GetCrossBrowserContext(original_context);
  if (cross_context) {
    BrowserContextID cross_context_id = GetBrowserContextID(cross_context);
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

void WebRequestEventRouter::UpdateActiveListener(
    content::BrowserContext* browser_context,
    ListenerUpdateType update_type,
    const ExtensionId& extension_id,
    const std::string& sub_event_name,
    int worker_thread_id,
    int64_t service_worker_version_id) {
  std::string event_name = EventRouter::GetBaseEventName(sub_event_name);

  const BrowserContextID browser_context_id =
      GetBrowserContextID(browser_context);
  BrowserContextData& data = data_[browser_context_id];
  auto matching_listener = RemoveMatchingListener(
      data.active_listeners[event_name], extension_id, sub_event_name,
      worker_thread_id, service_worker_version_id, browser_context_id);
  if (!matching_listener) {
    return;
  }

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

void WebRequestEventRouter::CleanUpForListener(const EventListener& listener,
                                               ListenerUpdateType update_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::string event_name =
      EventRouter::GetBaseEventName(listener.id.sub_event_name);

  // Unblock any request that this event listener may have been blocking.
  // Note that we do this even for deactivations, since if the service worker
  // is shut down (which would happen if it reached the hard lifetime timeout),
  // it won't be able to respond to the request.
  // TODO(crbug.com/40107353): This likely won't be sufficient, since it
  // means requests can leak through.
  for (uint64_t blocked_request_id : listener.blocked_requests) {
    DecrementBlockCount(listener.id.browser_context, listener.id.extension_id,
                        event_name, blocked_request_id, nullptr,
                        0 /* extra_info_spec */);
  }

  // Update the extra headers count and clear the cache only if the listener is
  // fully removed; otherwise, these values are still correct.
  if (update_type == ListenerUpdateType::kRemove) {
    if (listener.HasExtraHeaders()) {
      DecrementExtraHeadersListenerCount(listener.id.browser_context);
    }
    helpers::ClearCacheOnNavigation();
  }
}

void WebRequestEventRouter::RemoveWebViewEventListeners(
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

// static
void WebRequestEventRouter::OnOTRBrowserContextCreated(
    content::BrowserContext* original_browser_context,
    content::BrowserContext* otr_browser_context) {
  CrossContextData::Get().AddContext(original_browser_context,
                                     otr_browser_context);
}

// static
void WebRequestEventRouter::OnOTRBrowserContextDestroyed(
    content::BrowserContext* original_browser_context,
    content::BrowserContext* otr_browser_context) {
  ClearCrossContextData(original_browser_context);
  WebRequestEventRouter* event_router =
      WebRequestEventRouter::Get(original_browser_context);
  // Check if we get an instance before calling OnBrowserContextShutdown.
  // This is a workaround for tests that manipulate BrowserContext instances in
  // ways that break the expectations we have in production code.
  if (event_router) {
    event_router->OnBrowserContextShutdown(otr_browser_context);
    DCHECK(!base::Contains(event_router->data_,
                           GetBrowserContextID(otr_browser_context)));
  }
}

void WebRequestEventRouter::AddCallbackForPageLoad(base::OnceClosure callback) {
  GetCallbacksForPageLoad().push_back(std::move(callback));
}

bool WebRequestEventRouter::HasExtraHeadersListenerForRequest(
    content::BrowserContext* browser_context,
    const WebRequestInfo* request) {
  DCHECK(request);
  if (ShouldHideEvent(browser_context, *request)) {
    return false;
  }

  int extra_info_spec = 0;
  for (const char* name : kWebRequestEvents) {
    GetMatchingListeners(browser_context, name, request, &extra_info_spec);
    if (extra_info_spec & ExtraInfoSpec::EXTRA_HEADERS) {
      return true;
    }
  }

  // Check declarative net request API rulesets.
  return declarative_net_request::RulesMonitorService::Get(browser_context)
      ->ruleset_manager()
      ->HasExtraHeadersMatcherForRequest(*request,
                                         browser_context->IsOffTheRecord());
}

bool WebRequestEventRouter::HasAnyExtraHeadersListener(
    content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (HasAnyExtraHeadersListenerImpl(browser_context)) {
    return true;
  }

  content::BrowserContext* cross_browser_context =
      GetCrossBrowserContext(browser_context);
  if (cross_browser_context &&
      HasAnyExtraHeadersListenerImpl(cross_browser_context)) {
    return true;
  }

  // The RulesMonitorService instance is shared between the regular and
  // the OTR BrowserContext, so it doesn't matter which one we use.
  return declarative_net_request::RulesMonitorService::Get(browser_context)
      ->HasAnyExtraHeadersMatcher();
}

void WebRequestEventRouter::IncrementExtraHeadersListenerCount(
    content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  BrowserContextData& data = data_[GetBrowserContextID(browser_context)];
  DCHECK_GE(data.extra_headers_count, 0);
  data.extra_headers_count++;
}

void WebRequestEventRouter::DecrementExtraHeadersListenerCount(
    content::BrowserContext* browser_context) {
  BrowserContextData& data = data_[GetBrowserContextID(browser_context)];
  data.extra_headers_count--;
  DCHECK_GE(data.extra_headers_count, 0);
}

void WebRequestEventRouter::OnBrowserContextShutdown(
    content::BrowserContext* browser_context) {
  data_.erase(GetBrowserContextID(browser_context));
}

size_t WebRequestEventRouter::GetListenerCountForTesting(
    content::BrowserContext* browser_context,
    const std::string& event_name) {
  return data_[GetBrowserContextID(browser_context)]
      .active_listeners[event_name]
      .size();
}

size_t WebRequestEventRouter::GetInactiveListenerCountForTesting(
    content::BrowserContext* browser_context,
    const std::string& event_name) {
  return data_[GetBrowserContextID(browser_context)]
      .inactive_listeners[event_name]
      .size();
}

bool WebRequestEventRouter::HasAnyExtraHeadersListenerImpl(
    content::BrowserContext* browser_context) {
  auto iter = data_.find(GetBrowserContextID(browser_context));
  return iter != data_.end() && iter->second.extra_headers_count > 0;
}

WebRequestEventRouter::BlockedRequestMap&
WebRequestEventRouter::GetBlockedRequestMap(
    content::BrowserContext* browser_context) {
  // Blocked requests are stored in the data for the regular context.
  // TODO(crbug.com/40279375): Blocked requests should be isolated to
  // a particular BrowserContext and not shared between the main and
  // OTR contexts.
  if (browser_context->IsOffTheRecord()) {
    browser_context = GetCrossBrowserContext(browser_context);
  }
  return data_[GetBrowserContextID(browser_context)].blocked_requests;
}

void WebRequestEventRouter::ClearBlockedRequest(
    content::BrowserContext* browser_context,
    uint64_t id) {
  GetBlockedRequestMap(browser_context).erase(id);
}

WebRequestEventRouter::BlockedRequest&
WebRequestEventRouter::GetOrAddBlockedRequest(
    content::BrowserContext* browser_context,
    uint64_t id) {
  return GetBlockedRequestMap(browser_context)[id];
}

WebRequestEventRouter::BlockedRequest* WebRequestEventRouter::GetBlockedRequest(
    content::BrowserContext* browser_context,
    uint64_t id) {
  BlockedRequestMap& blocked_requests = GetBlockedRequestMap(browser_context);
  auto it = blocked_requests.find(id);
  return it == blocked_requests.end() ? nullptr : &it->second;
}

bool WebRequestEventRouter::IsPageLoad(const WebRequestInfo& request) const {
  return request.web_request_type == WebRequestResourceType::MAIN_FRAME;
}

void WebRequestEventRouter::NotifyPageLoad() {
  for (auto& callback : GetCallbacksForPageLoad()) {
    std::move(callback).Run();
  }
  GetCallbacksForPageLoad().clear();
}

// static
content::BrowserContext* WebRequestEventRouter::GetCrossBrowserContext(
    content::BrowserContext* browser_context) {
  return CrossContextData::Get().GetCrossBrowserContext(browser_context);
}

bool WebRequestEventRouter::WasSignaled(
    content::BrowserContext* browser_context,
    uint64_t request_id) const {
  const SignaledRequestIDTracker* const tracker =
      GetSignaledRequestIDTracker(browser_context);
  return !tracker ? false : tracker->WasSignaled(request_id);
}

WebRequestEventRouter::RawListeners WebRequestEventRouter::GetMatchingListeners(
    content::BrowserContext* browser_context,
    const std::string& event_name,
    const WebRequestInfo* request,
    int* extra_info_spec) {
  *extra_info_spec = 0;

  bool is_request_from_extension =
      IsRequestFromExtension(*request, browser_context);

  std::string web_request_event_name(event_name);
  if (request->is_web_view) {
    web_request_event_name.replace(0, kWebRequestEventPrefixLen,
                                   kWebViewEventPrefix);
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

// static
bool WebRequestEventRouter::ListenerMatchesRequest(
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

    if (listener.id.web_view_instance_id != request.web_view_instance_id) {
      return false;
    }
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

  // We do not want to notify extensions about XHR requests that are
  // triggered by themselves. This is a workaround to prevent deadlocks
  // in case of synchronous XHR requests that block the extension renderer
  // and therefore prevent the extension from processing the request
  // handler. This is only a problem for blocking listeners.
  // http://crbug.com/105656
  bool synchronous_xhr_from_extension =
      !request.is_async && is_request_from_extension &&
      request.web_request_type == WebRequestResourceType::XHR;
  return !listener.IsBlocking() || !synchronous_xhr_from_extension;
}

// static
void WebRequestEventRouter::GetMatchingListenersForRequest(
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

void WebRequestEventRouter::DecrementBlockCount(
    content::BrowserContext* browser_context,
    const ExtensionId& extension_id,
    const std::string& event_name,
    uint64_t request_id,
    std::unique_ptr<EventResponse> response,
    int extra_info_spec) {
  // It's possible that this request was deleted, or cancelled by a previous
  // event handler or handled by Declarative Net Request API. If so, ignore this
  // response.
  BlockedRequest* blocked_request =
      GetBlockedRequest(browser_context, request_id);
  if (!blocked_request) {
    return;
  }

  // Ensure that the response is for the event we are blocked on.
  DCHECK_EQ(blocked_request->event, GetEventTypeFromEventName(event_name));
  // Cache the event type; we use it below.
  EventTypes request_event = blocked_request->event;

  int num_handlers_blocking = --blocked_request->num_handlers_blocking;
  CHECK_GE(num_handlers_blocking, 0);

  if (response) {
    helpers::EventResponseDelta delta = CalculateDelta(
        browser_context, blocked_request, response.get(), extra_info_spec);

    activity_monitor::OnWebRequestApiUsed(
        static_cast<content::BrowserContext*>(browser_context), extension_id,
        blocked_request->request->url, blocked_request->is_incognito,
        event_name, SummarizeResponseDelta(event_name, delta));

    blocked_request->response_deltas.push_back(std::move(delta));
  }

  if (num_handlers_blocking == 0) {
    ExecuteDeltas(browser_context, blocked_request->request, true);
    // Note: `blocked_request` can be deleted here, depending on the outcome
    // of ExecuteDeltas(). Use the cached `request_event` and `request_id`
    // instead of using `blocked_request`.
    if (request_event == kOnBeforeRequest) {
      GetExtensionWebRequestTimeTracker().LogBeforeRequestCompletionTime(
          request_id, base::TimeTicks::Now());
    }
  }
}

void WebRequestEventRouter::SendMessages(
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

int WebRequestEventRouter::ExecuteDeltas(
    content::BrowserContext* browser_context,
    const WebRequestInfo* request,
    bool call_callback) {
  BlockedRequest& blocked_request =
      GetOrAddBlockedRequest(browser_context, request->id);
  CHECK_EQ(0, blocked_request.num_handlers_blocking);
  helpers::EventResponseDeltas& deltas = blocked_request.response_deltas;
  base::TimeDelta block_time =
      base::Time::Now() - blocked_request.blocking_time;
  GetExtensionWebRequestTimeTracker().IncrementTotalBlockTime(request->id,
                                                              block_time);

  bool request_headers_modified = false;
  bool response_headers_modified = false;
  bool credentials_set = false;
  // The set of request headers which were removed or set to new values.
  std::set<std::string> request_headers_removed;
  std::set<std::string> request_headers_set;

  deltas.sort(&helpers::InDecreasingExtensionInstallationTimeOrder);

  std::optional<ExtensionId> canceled_by_extension;
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
    NOTREACHED_IN_MIGRATION();
  }

  SendMessages(browser_context, blocked_request);

  if (!ignored_actions.empty()) {
    NotifyIgnoredActionsOnUI(browser_context, request->id,
                             std::move(ignored_actions));
  }

  for (const DNRRequestAction* action : matched_dnr_actions) {
    OnDNRActionMatched(browser_context, *request, *action);
  }

  const bool redirected =
      blocked_request.new_url && !blocked_request.new_url->is_empty();

  if (canceled_by_extension) {
    GetExtensionWebRequestTimeTracker().SetRequestCanceled(request->id);
  } else if (redirected) {
    GetExtensionWebRequestTimeTracker().SetRequestRedirected(request->id);
  }

  // Log UMA metrics. Note: We are not necessarily concerned with the final
  // action taken. Instead we are interested in how frequently the different
  // actions are used by extensions. Hence multiple actions may be logged for a
  // single delta execution.
  if (canceled_by_extension) {
    LogRequestAction(RequestAction::CANCEL);
  }
  if (redirected) {
    LogRequestAction(RequestAction::REDIRECT);
  }
  if (request_headers_modified) {
    LogRequestAction(RequestAction::MODIFY_REQUEST_HEADERS);
  }
  if (response_headers_modified) {
    LogRequestAction(RequestAction::MODIFY_RESPONSE_HEADERS);
  }
  if (credentials_set) {
    LogRequestAction(RequestAction::SET_AUTH_CREDENTIALS);
  }

  // This triggers onErrorOccurred if canceled is true.
  int rv = net::OK;
  if (canceled_by_extension) {
    rv = net::ERR_BLOCKED_BY_CLIENT;
    TRACE_EVENT2("extensions", "NetworkRequestBlockedByClient", "extension",
                 canceled_by_extension.value(), "id", request->id);
  }

  if (!blocked_request.callback.is_null()) {
    net::CompletionOnceCallback callback = std::move(blocked_request.callback);
    // Ensure that request is removed before callback because the callback
    // might trigger the next event.
    ClearBlockedRequest(browser_context, request->id);
    if (call_callback) {
      std::move(callback).Run(rv);
    }
  } else if (!blocked_request.before_send_headers_callback.is_null()) {
    auto callback = std::move(blocked_request.before_send_headers_callback);
    // Ensure that request is removed before callback because the callback
    // might trigger the next event.
    ClearBlockedRequest(browser_context, request->id);
    if (call_callback) {
      std::move(callback).Run(request_headers_removed, request_headers_set, rv);
    }
  } else if (!blocked_request.auth_callback.is_null()) {
    WebRequestEventRouter::AuthRequiredResponse response;
    if (canceled_by_extension) {
      response = AuthRequiredResponse::AUTH_REQUIRED_RESPONSE_CANCEL_AUTH;
    } else if (credentials_set) {
      response = AuthRequiredResponse::AUTH_REQUIRED_RESPONSE_SET_AUTH;
    } else {
      response = AuthRequiredResponse::AUTH_REQUIRED_RESPONSE_NO_ACTION;
    }

    AuthCallback callback = std::move(blocked_request.auth_callback);
    ClearBlockedRequest(browser_context, request->id);
    if (call_callback) {
      std::move(callback).Run(response);
    }
  } else {
    ClearBlockedRequest(browser_context, request->id);
  }
  return rv;
}

bool WebRequestEventRouter::ProcessDeclarativeRules(
    content::BrowserContext* browser_context,
    const std::string& event_name,
    const WebRequestInfo* request,
    RequestStage request_stage,
    const net::HttpResponseHeaders* original_response_headers) {
  // If this check fails, check that the active stages are up to date in
  // extensions/browser/api/declarative_webrequest/request_stage.h.
  DCHECK(request_stage & kActiveStages);

  int rules_registry_id = request->is_web_view
                              ? request->web_view_rules_registry_id
                              : RulesRegistryService::kDefaultRulesRegistryID;

  // First parameter identifies the registry, the second indicates whether the
  // registry belongs to the cross browser_context.
  using RelevantRegistry = std::pair<WebRequestRulesRegistry*, bool>;
  std::vector<RelevantRegistry> relevant_registries;

  // Get the WebRequestRulesRegistry for the current BrowserContext, if any.
  {
    BrowserContextData& data = data_[GetBrowserContextID(browser_context)];
    auto rules_key_it = data.rules_registries.find(rules_registry_id);
    if (rules_key_it != data.rules_registries.end()) {
      relevant_registries.emplace_back(rules_key_it->second.get(), false);
    }
  }

  // Rules of the current `browser_context` may apply but we need to check also
  // whether there are applicable rules from extensions whose background page
  // spans from regular to incognito mode.
  content::BrowserContext* cross_browser_context =
      GetCrossBrowserContext(browser_context);
  if (cross_browser_context) {
    BrowserContextData& cross_context_data =
        data_[GetBrowserContextID(cross_browser_context)];
    auto cross_rules_key_it =
        cross_context_data.rules_registries.find(rules_registry_id);
    if (cross_rules_key_it != cross_context_data.rules_registries.end()) {
      relevant_registries.emplace_back(cross_rules_key_it->second.get(), true);
    }
  }

  for (auto it : relevant_registries) {
    WebRequestRulesRegistry* rules_registry = it.first;
    if (rules_registry->ready().is_signaled()) {
      continue;
    }

    // The rules registry is still loading. Block this request until it
    // finishes.
    rules_registry->ready().Post(
        FROM_HERE, base::BindOnce(&WebRequestEventRouter::OnRulesRegistryReady,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  static_cast<void*>(browser_context),
                                  event_name, request->id, request_stage));
    BlockedRequest& blocked_request =
        GetOrAddBlockedRequest(browser_context, request->id);
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
          GetOrAddBlockedRequest(browser_context, request->id).response_deltas;
      deltas.insert(deltas.end(), std::make_move_iterator(result.begin()),
                    std::make_move_iterator(result.end()));
      deltas_created = true;
    }
  }

  return deltas_created;
}

void WebRequestEventRouter::OnRulesRegistryReady(void* browser_context_id,
                                                 const std::string& event_name,
                                                 uint64_t request_id,
                                                 RequestStage request_stage) {
  // TODO(crbug.com/40264286): We should be able to remove this once we roll
  // out the per-BrowserContext event router, since the WeakPtr that was bound
  // to the callback will be invalidated when the BrowserContext shuts down.
  // Some additional special handling will be needed since this might be a
  // pointer to an off-the-record instance.
  if (!ExtensionsBrowserClient::Get()->IsValidContext(browser_context_id)) {
    return;
  }

  content::BrowserContext* browser_context =
      reinterpret_cast<content::BrowserContext*>(browser_context_id);

  // It's possible that this request was deleted, or cancelled by a previous
  // event handler. If so, ignore this response.
  BlockedRequest* blocked_request =
      GetBlockedRequest(browser_context, request_id);
  if (!blocked_request) {
    return;
  }

  ProcessDeclarativeRules(browser_context, event_name, blocked_request->request,
                          request_stage,
                          blocked_request->original_response_headers.get());
  DecrementBlockCount(browser_context, std::string(), event_name, request_id,
                      nullptr, 0 /* extra_info_spec */);
}

bool WebRequestEventRouter::GetAndSetSignaled(
    content::BrowserContext* browser_context,
    uint64_t request_id,
    EventTypes event_type) {
  return GetSignaledRequestIDTracker(browser_context)
      .GetAndSet(request_id, event_type);
}

void WebRequestEventRouter::ClearSignaled(
    content::BrowserContext* browser_context,
    uint64_t request_id,
    EventTypes event_type) {
  GetSignaledRequestIDTracker(browser_context)
      .ClearEventType(request_id, event_type);
}

}  // namespace extensions
