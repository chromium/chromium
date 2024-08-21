// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_WEB_REQUEST_EXTENSION_WEB_REQUEST_EVENT_ROUTER_H_
#define EXTENSIONS_BROWSER_API_WEB_REQUEST_EXTENSION_WEB_REQUEST_EVENT_ROUTER_H_

#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/api/declarative_webrequest/request_stage.h"
#include "extensions/browser/api/web_request/web_request_api_helpers.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/url_pattern_set.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace net {
class AuthChallengeInfo;
class AuthCredentials;
class HttpRequestHeaders;
class HttpResponseHeaders;
}  // namespace net

namespace extensions {

enum class WebRequestResourceType : uint8_t;
class WebRequestRulesRegistry;
class WebRequestEventDetails;
struct WebRequestInfo;

class WebRequestEventRouter : public KeyedService {
 public:
  explicit WebRequestEventRouter(content::BrowserContext* browser_context);
  ~WebRequestEventRouter() override;
  WebRequestEventRouter(const WebRequestEventRouter&) = delete;
  WebRequestEventRouter& operator=(const WebRequestEventRouter&) = delete;

  // KeyedService overrides.
  void Shutdown() override;

  struct BlockedRequest;

  // The events denoting the lifecycle of a given network request.
  enum EventTypes {
    kInvalidEvent = 0,
    kOnBeforeRequest = 1 << 0,
    kOnBeforeSendHeaders = 1 << 1,
    kOnSendHeaders = 1 << 2,
    kOnHeadersReceived = 1 << 3,
    kOnBeforeRedirect = 1 << 4,
    kOnAuthRequired = 1 << 5,
    kOnResponseStarted = 1 << 6,
    kOnErrorOccurred = 1 << 7,
    kOnCompleted = 1 << 8,
  };

  // Get the instance of the WebRequestEventRouter for |browser_context|.
  static WebRequestEventRouter* Get(content::BrowserContext* browser_context);

  static std::vector<std::string> GetEventNames();

  // Internal representation of the webRequest.RequestFilter type, used to
  // filter what network events an extension cares about.
  struct RequestFilter {
    RequestFilter();
    ~RequestFilter();

    RequestFilter(const RequestFilter&) = delete;
    RequestFilter& operator=(const RequestFilter&) = delete;

    RequestFilter(RequestFilter&& other);
    RequestFilter& operator=(RequestFilter&& other);

    // Returns false if there was an error initializing. If it is a user error,
    // an error message is provided, otherwise the error is internal (and
    // unexpected).
    bool InitFromValue(const base::Value::Dict& value, std::string* error);

    extensions::URLPatternSet urls;
    std::vector<WebRequestResourceType> types;
    int tab_id;
    int window_id;
  };

  // Contains an extension's response to a blocking event.
  struct EventResponse {
    EventResponse(const ExtensionId& extension_id,
                  const base::Time& extension_install_time);

    EventResponse(const EventResponse&) = delete;
    EventResponse& operator=(const EventResponse&) = delete;

    ~EventResponse();

    // ID of the extension that sent this response.
    ExtensionId extension_id;

    // The time that the extension was installed. Used for deciding order of
    // precedence in case multiple extensions respond with conflicting
    // decisions.
    base::Time extension_install_time;

    // Response values. These are mutually exclusive.
    bool cancel;
    GURL new_url;
    std::unique_ptr<net::HttpRequestHeaders> request_headers;
    std::unique_ptr<extension_web_request_api_helpers::ResponseHeaders>
        response_headers;

    std::optional<net::AuthCredentials> auth_credentials;
  };

  // AuthRequiredResponse indicates how an OnAuthRequired call is handled.
  enum class AuthRequiredResponse {
    // No credenitals were provided.
    AUTH_REQUIRED_RESPONSE_NO_ACTION,
    // AuthCredentials is filled in with a username and password, which should
    // be used in a response to the provided auth challenge.
    AUTH_REQUIRED_RESPONSE_SET_AUTH,
    // The request should be canceled.
    AUTH_REQUIRED_RESPONSE_CANCEL_AUTH,
    // The action will be decided asynchronously. |callback| will be invoked
    // when the decision is made, and one of the other AuthRequiredResponse
    // values will be passed in with the same semantics as described above.
    AUTH_REQUIRED_RESPONSE_IO_PENDING,
  };
  using AuthCallback = base::OnceCallback<void(AuthRequiredResponse)>;

  // The type of listener removal.
  enum class ListenerUpdateType {
    // The listener was fully removed by the extension and the registration
    // should be removed here.
    kRemove,
    // This is for a lazy listener where the "active" listener's process is shut
    // down, but the listener should still be registered (and will be stored in
    // `BrowserContextData::inactive_listeners`).
    kDeactivate,
  };

  // Registers a rule registry. Pass null for |rules_registry| to unregister
  // the rule registry for |browser_context|.
  void RegisterRulesRegistry(
      content::BrowserContext* browser_context,
      int rules_registry_id,
      scoped_refptr<WebRequestRulesRegistry> rules_registry);

  // Dispatches the OnBeforeRequest event to any extensions whose filters match
  // the given request. Returns net::ERR_IO_PENDING if an extension is
  // intercepting the request and OK if the request should proceed normally.
  // net::ERR_BLOCKED_BY_CLIENT is returned if the request should be blocked. In
  // this case, |should_collapse_initiator| might be set to true indicating
  // whether the DOM element which initiated the request should be blocked.
  int OnBeforeRequest(content::BrowserContext* browser_context,
                      WebRequestInfo* request,
                      net::CompletionOnceCallback callback,
                      GURL* new_url,
                      bool* should_collapse_initiator);

  using BeforeSendHeadersCallback =
      base::OnceCallback<void(const std::set<std::string>& removed_headers,
                              const std::set<std::string>& set_headers,
                              int error_code)>;

  // Dispatches the onBeforeSendHeaders event. This is fired for HTTP(s)
  // requests only, and allows modification of the outgoing request headers.
  // Returns net::ERR_IO_PENDING if an extension is intercepting the request, OK
  // otherwise.
  int OnBeforeSendHeaders(content::BrowserContext* browser_context,
                          const WebRequestInfo* request,
                          BeforeSendHeadersCallback callback,
                          net::HttpRequestHeaders* headers);

  // Dispatches the onSendHeaders event. This is fired for HTTP(s) requests
  // only.
  void OnSendHeaders(content::BrowserContext* browser_context,
                     const WebRequestInfo* request,
                     const net::HttpRequestHeaders& headers);

  // Dispatches the onHeadersReceived event. This is fired for HTTP(s)
  // requests only, and allows modification of incoming response headers.
  // Returns net::ERR_IO_PENDING if an extension is intercepting the request,
  // OK otherwise. |original_response_headers| is reference counted. |callback|
  // |override_response_headers| and |preserve_fragment_on_redirect_url| are not
  // owned but are guaranteed to be valid until |callback| is called or
  // OnRequestWillBeDestroyed is called (whatever comes first).
  // Do not modify |original_response_headers| directly but write new ones
  // into |override_response_headers|.
  int OnHeadersReceived(
      content::BrowserContext* browser_context,
      WebRequestInfo* request,
      net::CompletionOnceCallback callback,
      const net::HttpResponseHeaders* original_response_headers,
      scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
      GURL* preserve_fragment_on_redirect_url,
      bool* should_collapse_initiator);

  // Dispatches the OnAuthRequired event to any extensions whose filters match
  // the given request. If the listener is not registered as "blocking", then
  // AUTH_REQUIRED_RESPONSE_NO_ACTION is returned. Otherwise,
  // AUTH_REQUIRED_RESPONSE_IO_PENDING is returned and |callback| will be
  // invoked later.
  AuthRequiredResponse OnAuthRequired(content::BrowserContext* browser_context,
                                      const WebRequestInfo* request,
                                      const net::AuthChallengeInfo& auth_info,
                                      AuthCallback callback,
                                      net::AuthCredentials* credentials);

  // Dispatches the onBeforeRedirect event. This is fired for HTTP(s) requests
  // only.
  void OnBeforeRedirect(content::BrowserContext* browser_context,
                        const WebRequestInfo* request,
                        const GURL& new_location);

  // Dispatches the onResponseStarted event indicating that the first bytes of
  // the response have arrived.
  void OnResponseStarted(content::BrowserContext* browser_context,
                         const WebRequestInfo* request,
                         int net_error);

  // Dispatches the onComplete event.
  void OnCompleted(content::BrowserContext* browser_context,
                   const WebRequestInfo* request,
                   int net_error);

  // Dispatches an onErrorOccurred event.
  void OnErrorOccurred(content::BrowserContext* browser_context,
                       const WebRequestInfo* request,
                       bool started,
                       int net_error);

  // Notificaties when |request| is no longer being processed, regardless of
  // whether it has gone to completion or merely been cancelled. This is
  // guaranteed to be called eventually for any request observed by this object,
  // and |*request| will be immintently destroyed after this returns.
  void OnRequestWillBeDestroyed(content::BrowserContext* browser_context,
                                const WebRequestInfo* request);

  // Called when an event listener handles a blocking event and responds.
  void OnEventHandled(content::BrowserContext* browser_context,
                      const ExtensionId& extension_id,
                      const std::string& event_name,
                      const std::string& sub_event_name,
                      uint64_t request_id,
                      int render_process_id,
                      int web_view_instance_id,
                      int worker_thread_id,
                      int64_t service_worker_version_id,
                      std::unique_ptr<EventResponse> response);

  // Adds a listener to the given event. |event_name| specifies the event being
  // listened to. |sub_event_name| is an internal event uniquely generated in
  // the extension process to correspond to the given filter and
  // extra_info_spec. It returns true on success, false on failure.
  bool AddEventListener(content::BrowserContext* browser_context,
                        const ExtensionId& extension_id,
                        const std::string& extension_name,
                        const std::string& event_name,
                        const std::string& sub_event_name,
                        RequestFilter filter,
                        int extra_info_spec,
                        int render_process_id,
                        int web_view_instance_id,
                        int worker_thread_id,
                        int64_t service_worker_version_id);

  // Removes the listeners for a given <webview>.
  void RemoveWebViewEventListeners(content::BrowserContext* browser_context,
                                   int render_process_id,
                                   int web_view_instance_id);

  // Called when an incognito browser_context is created or destroyed. When
  // the OTR context is created, the original BrowserContext may not yet be
  // fully initialized, including its keyed services and factories, so this
  // must be static.
  static void OnOTRBrowserContextCreated(
      content::BrowserContext* original_browser_context,
      content::BrowserContext* otr_browser_context);
  static void OnOTRBrowserContextDestroyed(
      content::BrowserContext* original_browser_context,
      content::BrowserContext* otr_browser_context);

  // Registers a |callback| that is executed when the next page load happens.
  // The callback is then deleted.
  static void AddCallbackForPageLoad(base::OnceClosure callback);

  // Whether there is a listener matching the request that has
  // ExtraInfoSpec::EXTRA_HEADERS set.
  bool HasExtraHeadersListenerForRequest(
      content::BrowserContext* browser_context,
      const WebRequestInfo* request);

  // Whether there are any listeners for this context that have
  // ExtraInfoSpec::EXTRA_HEADERS set.
  bool HasAnyExtraHeadersListener(content::BrowserContext* browser_context);

  // Called when a BrowserContext is being destroyed.
  void OnBrowserContextShutdown(content::BrowserContext* browser_context);

  // Get the number of listeners - for testing only.
  size_t GetListenerCountForTesting(content::BrowserContext* browser_context,
                                    const std::string& event_name);
  size_t GetInactiveListenerCountForTesting(
      content::BrowserContext* browser_context,
      const std::string& event_name);

  bool HasAnyExtraHeadersListenerForTesting(
      content::BrowserContext* browser_context) {
    return HasAnyExtraHeadersListenerImpl(browser_context);
  }

  void UpdateActiveListenerForTesting(content::BrowserContext* browser_context,
                                      ListenerUpdateType update_type,
                                      const ExtensionId& extension_id,
                                      const std::string& sub_event_name,
                                      int worker_thread_id,
                                      int64_t service_worker_version_id) {
    UpdateActiveListener(browser_context, update_type, extension_id,
                         sub_event_name, worker_thread_id,
                         service_worker_version_id);
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(ExtensionWebRequestTest, BrowserContextShutdown);

  // Identifier for a `BrowserContext` to scope the lifetime for references.
  // `BrowserContextID` is derived from `BrowserContext*`, used in comparison
  // only, and are never deferenced.
  using BrowserContextID = std::uintptr_t;

  static BrowserContextID GetBrowserContextID(
      content::BrowserContext* browser_context) {
    return reinterpret_cast<BrowserContextID>(
        static_cast<void*>(browser_context));
  }

  friend class WebRequestAPI;

  struct EventListener {
    struct ID {
      ID(content::BrowserContext* browser_context,
         const ExtensionId& extension_id,
         const std::string& sub_event_name,
         int render_process_id,
         int web_view_instance_id,
         int worker_thread_id,
         int64_t service_worker_version_id);

      ID(const ID& source);
      ID(ID&& source);

      bool operator==(const ID& that) const;

      raw_ptr<content::BrowserContext> browser_context;
      ExtensionId extension_id;
      std::string sub_event_name;
      // In the case of a webview, this is the process ID of the embedder.
      int render_process_id;
      int web_view_instance_id;
      // The worker_thread_id and service_worker_version_id members are only
      // meaningful for event listeners for ServiceWorker events. Otherwise,
      // they are initialized to sentinel values.
      int worker_thread_id;
      int64_t service_worker_version_id;
    };

    explicit EventListener(ID id);

    EventListener(const EventListener&) = delete;
    EventListener& operator=(const EventListener&) = delete;

    ~EventListener();

    bool HasExtraHeaders() const {
      using extension_web_request_api_helpers::ExtraInfoSpec;
      return extra_info_spec & ExtraInfoSpec::EXTRA_HEADERS;
    }

    bool IsBlocking() const {
      using extension_web_request_api_helpers::ExtraInfoSpec;
      return extra_info_spec &
             (ExtraInfoSpec::BLOCKING | ExtraInfoSpec::ASYNC_BLOCKING);
    }

    ID id;
    std::string extension_name;
    events::HistogramValue histogram_value = events::UNKNOWN;
    RequestFilter filter;
    int extra_info_spec = 0;
    std::unordered_set<uint64_t> blocked_requests;
  };

  using RawListeners = std::vector<EventListener*>;
  using ListenerIDs = std::vector<EventListener::ID>;
  using Listeners = std::vector<std::unique_ptr<EventListener>>;
  using ListenerMap = std::map<std::string, Listeners>;
  using BlockedRequestMap = std::map<uint64_t, BlockedRequest>;

  class SignaledRequestIDTracker {
   public:
    SignaledRequestIDTracker();
    ~SignaledRequestIDTracker();
    SignaledRequestIDTracker(SignaledRequestIDTracker&&);

    SignaledRequestIDTracker(const SignaledRequestIDTracker&) = delete;
    SignaledRequestIDTracker& operator=(const SignaledRequestIDTracker&) =
        delete;

    // Clears the request.
    void ClearRequest(uint64_t request_id) {
      signaled_requests_.erase(request_id);
    }

    // Gets the previous state of the event and sets the flag for that event.
    bool GetAndSet(uint64_t request_id, EventTypes event_type);

    // Clears the flag that `event_type` has been signaled for `request_id`.
    void ClearEventType(uint64_t request_id, EventTypes event_type);

    // Returns true if `request_id` was already signaled to some event handlers.
    bool WasSignaled(uint64_t request_id) const {
      auto flag = signaled_requests_.find(request_id);
      return flag != signaled_requests_.end() && flag->second;
    }

   private:
    // Map of request_id -> bit vector of EventTypes already signaled
    using SignaledRequestMap = std::map<uint64_t, int>;

    // A map of request IDs to a bitvector indicating which events have been
    // signaled and should not be sent again.
    SignaledRequestMap signaled_requests_;
  };

  // A collection of data associated with a given BrowserContext.
  struct BrowserContextData {
    BrowserContextData();
    BrowserContextData(BrowserContextData&&);
    ~BrowserContextData();

    // The listeners that are currently active (i.e., have a corresponding
    // render process).
    ListenerMap active_listeners;
    // Listeners that are associated with currently-inactive lazy contexts.
    // These can still match events, but don't have an active renderer process.
    ListenerMap inactive_listeners;
    // The number of listeners that request extra headers be included with their
    // events. Modified through `IncrementExtraHeadersListenerCount()` and
    // `DecrementExtraHeadersListenerCount()`.
    int extra_headers_count = 0;
    // Maps each BrowserContext using the webview key to its respective rules
    // registry. For non-webview contexts, the default value defined by
    // `RulesRegistryService::kDefaultRulesRegistryID` is used.
    std::map<int, scoped_refptr<WebRequestRulesRegistry>> rules_registries;

    // A map of network requests that are waiting for at least one event handler
    // to respond. Blocked requests are stored on the regular BrowserContext for
    // both it and any off-the-record BrowserContext that exists.
    BlockedRequestMap blocked_requests;

    SignaledRequestIDTracker signaled_request_id_tracker;
  };

  using DataMap = std::map<BrowserContextID, BrowserContextData>;

  // Returns the EventListener with the given |id|, or nullptr.
  EventListener* FindEventListener(const EventListener::ID& id);

  // Returns the EventListener with the given |id| from |listeners|.
  EventListener* FindEventListenerInContainer(const EventListener::ID& id,
                                              const Listeners& listeners);

  // Updates the active listener registration indicated by the given criteria.
  // `update_type` indicates whether the listener is fully removed or if it's
  // a lazy listener that had its context shut down.
  void UpdateActiveListener(content::BrowserContext* browser_context,
                            ListenerUpdateType update_type,
                            const ExtensionId& extension_id,
                            const std::string& sub_event_name,
                            int worker_thread_id,
                            int64_t service_worker_version_id);

  // Removes a lazy listener registration. This affects both the provided
  // `original_context` and any incognito context associated with it.
  void RemoveLazyListener(content::BrowserContext* original_context,
                          const ExtensionId& extension_id,
                          const std::string& sub_event_name);

  // Removes the listener from `listeners` that matches the given criteria.
  // Optional criteria are ignored if not provided. Removes the matching
  // listener, if any. Expects a maximum of one listener to match.
  static std::unique_ptr<EventListener> RemoveMatchingListener(
      Listeners& listeners,
      const ExtensionId& extension_id,
      const std::string& sub_event_name,
      std::optional<int> worker_thread_id,
      std::optional<int64_t> service_worker_version_id,
      BrowserContextID browser_context_id);

  // Cleans up for a listener being removed, unblocking any requests and
  // updating counts as appropriate.
  void CleanUpForListener(const EventListener& listener,
                          ListenerUpdateType removal_type);

  // Ensures that future callbacks for |request| are ignored so that it can be
  // destroyed safely.
  void ClearPendingCallbacks(content::BrowserContext* browser_context,
                             const WebRequestInfo& request);

  bool DispatchEvent(content::BrowserContext* browser_context,
                     const WebRequestInfo* request,
                     const RawListeners& listener_ids,
                     std::unique_ptr<WebRequestEventDetails> event_details);

  void DispatchEventToListeners(
      content::BrowserContext* browser_context,
      std::unique_ptr<ListenerIDs> listener_ids,
      uint64_t request_id,
      std::unique_ptr<WebRequestEventDetails> event_details);

  // Returns a list of event listeners that care about the given event, based
  // on their filter parameters. |extra_info_spec| will contain the combined
  // set of extra_info_spec flags that every matching listener asked for.
  RawListeners GetMatchingListeners(content::BrowserContext* browser_context,
                                    const std::string& event_name,
                                    const WebRequestInfo* request,
                                    int* extra_info_spec);

  // Returns true if the given `listener` matches the `request`.
  // This needs to be a class method because `EventListener` is a private
  // struct.
  static bool ListenerMatchesRequest(const EventListener& listener,
                                     const WebRequestInfo& request,
                                     content::BrowserContext& browser_context,
                                     bool is_request_from_extension,
                                     bool crosses_incognito);

  // Adds all listeners that match `request` from `listeners` into
  // `listeners_out` and populates `extra_info_spec_out` with the set of all
  // options on the matches listeners.
  static void GetMatchingListenersForRequest(
      const Listeners& listeners,
      const WebRequestInfo& request,
      content::BrowserContext& browser_context,
      bool is_request_from_extension,
      bool crosses_incognito,
      RawListeners* listeners_out,
      int* extra_info_spec_out);

  // Decrements the count of event handlers blocking the given request. When the
  // count reaches 0, we stop blocking the request and proceed it using the
  // method requested by the extension with the highest precedence. Precedence
  // is decided by extension install time.
  void DecrementBlockCount(content::BrowserContext* browser_context,
                           const ExtensionId& extension_id,
                           const std::string& event_name,
                           uint64_t request_id,
                           std::unique_ptr<EventResponse> response,
                           int extra_info_spec);

  // Processes the generated deltas from blocked_requests_ on the specified
  // request. If |call_callback| is true, the callback registered in
  // |blocked_requests_| is called.
  // The function returns the error code for the network request. This is
  // mostly relevant in case the caller passes |call_callback| = false
  // and wants to return the correct network error code themself.
  int ExecuteDeltas(content::BrowserContext* browser_context,
                    const WebRequestInfo* request,
                    bool call_callback);

  // Evaluates the rules of the declarative webrequest API and stores
  // modifications to the request that result from WebRequestActions as
  // deltas in |blocked_requests_|. |filtered_response_headers| should only be
  // set for the OnHeadersReceived stage and NULL otherwise. Returns whether any
  // deltas were generated.
  bool ProcessDeclarativeRules(
      content::BrowserContext* browser_context,
      const std::string& event_name,
      const WebRequestInfo* request,
      RequestStage request_stage,
      const net::HttpResponseHeaders* filtered_response_headers);

  // If the BlockedRequest contains messages_to_extension entries in the event
  // deltas, we send them to subscribers of
  // chrome.declarativeWebRequest.onMessage.
  void SendMessages(content::BrowserContext* browser_context,
                    const BlockedRequest& blocked_request);

  // Called when the RulesRegistry is ready to unblock a request that was
  // waiting for said event.
  void OnRulesRegistryReady(void* browser_context_id,
                            const std::string& event_name,
                            uint64_t request_id,
                            RequestStage request_stage);

  // Sets the flag that |event_type| has been signaled for |request_id|.
  // Returns the value of the flag before setting it.
  bool GetAndSetSignaled(content::BrowserContext* browser_context,
                         uint64_t request_id,
                         EventTypes event_type);

  // Clears the flag that |event_type| has been signaled for |request_id|.
  void ClearSignaled(content::BrowserContext* browser_context,
                     uint64_t request_id,
                     EventTypes event_type);

  // Returns whether |request| represents a top level window navigation.
  bool IsPageLoad(const WebRequestInfo& request) const;

  // Called on a page load to process all registered callbacks.
  void NotifyPageLoad();

  // Returns the matching cross browser_context (the regular browser_context if
  // |browser_context| is OTR and vice versa).
  static content::BrowserContext* GetCrossBrowserContext(
      content::BrowserContext* browser_context);

  // Returns true if |request_id| was already signaled to some event handlers.
  bool WasSignaled(content::BrowserContext* browser_context,
                   uint64_t request_id) const;

  void IncrementExtraHeadersListenerCount(
      content::BrowserContext* browser_context);
  void DecrementExtraHeadersListenerCount(
      content::BrowserContext* browser_context);

  // Helper for |HasAnyExtraHeadersListener()|.
  bool HasAnyExtraHeadersListenerImpl(content::BrowserContext* browser_context);

  // Returns the instance of the BlockedRequestMap for `browser_context`.
  BlockedRequestMap& GetBlockedRequestMap(
      content::BrowserContext* browser_context);

  // Returns the instance of the SignaledRequestIDTracker for
  // `browser_context`, if the BrowserContext exists in the
  // BrowserContextData map. Otherwise, it returns nullptr.
  const SignaledRequestIDTracker* GetSignaledRequestIDTracker(
      content::BrowserContext* browser_context) const {
    auto it = data_.find(GetBrowserContextID(browser_context));
    return it == data_.end() ? nullptr
                             : &it->second.signaled_request_id_tracker;
  }

  // Returns the instance of the SignaledRequestIDTracker for
  // `browser_context`.
  SignaledRequestIDTracker& GetSignaledRequestIDTracker(
      content::BrowserContext* browser_context) {
    return data_[GetBrowserContextID(browser_context)]
        .signaled_request_id_tracker;
  }

  // Clears any entries in the BlockedRequestMap for `browser_context` with
  // `id`.
  void ClearBlockedRequest(content::BrowserContext* browser_context,
                           uint64_t id);

  // Gets the entry in the BlockedRequestMap for `browser_context` with `id`.
  // The entry is created if it doesn't exist.
  BlockedRequest& GetOrAddBlockedRequest(
      content::BrowserContext* browser_context,
      uint64_t id);

  // Gets the existing entry in the BlockedRequestMap for `browser_context`
  // with `id`. The entry is not created if it doesn't exist.
  BlockedRequest* GetBlockedRequest(content::BrowserContext* browser_context,
                                    uint64_t id);

  // A map of data associated with given BrowserContexts.
  DataMap data_;

  const raw_ptr<content::BrowserContext> browser_context_;

  base::WeakPtrFactory<WebRequestEventRouter> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_WEB_REQUEST_EXTENSION_WEB_REQUEST_EVENT_ROUTER_H_
