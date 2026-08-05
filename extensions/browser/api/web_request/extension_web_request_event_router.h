// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_WEB_REQUEST_EXTENSION_WEB_REQUEST_EVENT_ROUTER_H_
#define EXTENSIONS_BROWSER_API_WEB_REQUEST_EXTENSION_WEB_REQUEST_EVENT_ROUTER_H_

#include <stdint.h>

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/functional/function_ref.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/common/child_process_id.h"
#include "extensions/browser/api/declarative_webrequest/request_stage.h"
#include "extensions/browser/api/web_request/web_request_api_helpers.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_observer.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/api/web_request/web_request_filter.h"
#include "extensions/common/api/web_request/web_request_resource_type.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/url_pattern_set.h"
#include "net/base/completion_once_callback.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_database.mojom.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

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

class WebRequestRulesRegistry;
class WebRequestEventDetails;
struct WebRequestInfo;
struct WorkerId;

class WebRequestEventRouter : public KeyedService,
                              public ProcessManagerObserver,
                              public content::RenderProcessHostObserver {
 public:
  struct BlockedRequest;

  // The events denoting the lifecycle of a given network request.
  enum class EventTypes {
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

  explicit WebRequestEventRouter(content::BrowserContext* browser_context);
  ~WebRequestEventRouter() override;
  WebRequestEventRouter(const WebRequestEventRouter&) = delete;
  WebRequestEventRouter& operator=(const WebRequestEventRouter&) = delete;

  // KeyedService overrides.
  void Shutdown() override;

  // Get the instance of the WebRequestEventRouter for `browser_context`.
  static WebRequestEventRouter* Get(content::BrowserContext* browser_context);

  static std::vector<std::string> GetEventNames();

  // Internal representation of the webRequest.RequestFilter type, used to
  // filter what network events an extension cares about.
  struct RequestFilter : public WebRequestParsedFilter {
    // Serializes the filter to a dictionary value suitable for persistence.
    base::DictValue ToValue() const;
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
    // No credentials were provided.
    AUTH_REQUIRED_RESPONSE_NO_ACTION,
    // AuthCredentials is filled in with a username and password, which should
    // be used in a response to the provided auth challenge.
    AUTH_REQUIRED_RESPONSE_SET_AUTH,
    // The request should be canceled.
    AUTH_REQUIRED_RESPONSE_CANCEL_AUTH,
    // The action will be decided asynchronously. `callback` will be invoked
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

  // Registers a rule registry. Pass null for `rules_registry` to unregister
  // the rule registry for `browser_context`.
  void RegisterRulesRegistry(
      content::BrowserContext* browser_context,
      int rules_registry_id,
      scoped_refptr<WebRequestRulesRegistry> rules_registry);

  // Dispatches the OnBeforeRequest event to any extensions whose filters match
  // the given request. Returns net::ERR_IO_PENDING if an extension is
  // intercepting the request and OK if the request should proceed normally.
  // net::ERR_BLOCKED_BY_CLIENT is returned if the request should be blocked. In
  // this case, `should_collapse_initiator` might be set to true indicating
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
  // OK otherwise. `original_response_headers` is reference counted. `callback`
  // `override_response_headers` and `preserve_fragment_on_redirect_url` are not
  // owned but are guaranteed to be valid until `callback` is called or
  // OnRequestWillBeDestroyed is called (whatever comes first).
  // Do not modify `original_response_headers` directly but write new ones
  // into `override_response_headers`.
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
  // AUTH_REQUIRED_RESPONSE_IO_PENDING is returned and `callback` will be
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

  // Notificaties when `request` is no longer being processed, regardless of
  // whether it has gone to completion or merely been cancelled. This is
  // guaranteed to be called eventually for any request observed by this object,
  // and |*request| will be imminently destroyed after this returns.
  void OnRequestWillBeDestroyed(content::BrowserContext* browser_context,
                                const WebRequestInfo* request);

  // Called when an event listener handles a blocking event and responds.
  // `browser_context` is the responding listener's context; the blocked
  // request may belong to the cross browser context (a spanning-mode
  // extension responding to an off-the-record request).
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

  // Called when a blocking listener for a given target context and
  // parent event name (not a sub-event name) responds to a dispatched event.
  // It does NOT resolve the target: resolution is signaled separately by
  // `OnEventHandlingDone()`, because the context may have multiple listeners
  // for the same parent event name. `extra_info_spec` holds the options the
  // responding listener was registered with. `browser_context` is the
  // responding target's context; the blocked request may belong to the
  // cross browser context.
  void OnEventHandledForTarget(content::BrowserContext* browser_context,
                               const ExtensionId& extension_id,
                               const std::string& event_name,
                               uint64_t request_id,
                               content::ChildProcessId render_process_id,
                               int web_view_instance_id,
                               int worker_thread_id,
                               int64_t service_worker_version_id,
                               int extra_info_spec,
                               std::unique_ptr<EventResponse> response);

  // Called when a renderer context has finished handling a blocking event
  // for `request_id`, after all of its matching listeners have settled.
  // Resolves the pending dispatch target identified by the context.
  // `browser_context` is the responding target's context; the blocked
  // request may belong to the cross browser context.
  void OnEventHandlingDone(content::BrowserContext* browser_context,
                           const ExtensionId& extension_id,
                           const std::string& event_name,
                           uint64_t request_id,
                           content::ChildProcessId render_process_id,
                           int web_view_instance_id,
                           int worker_thread_id,
                           int64_t service_worker_version_id);

  // Adds a listener to the given event. `event_name` specifies the event being
  // listened to. `sub_event_name` is an internal event uniquely generated in
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
                        int64_t service_worker_version_id,
                        bool is_lazy);

  // Removes the listeners for a given <webview>.
  void RemoveWebViewEventListeners(content::BrowserContext* browser_context,
                                   content::ChildProcessId render_process_id,
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

  // Registers a `callback` that is executed when the next page load happens.
  // The callback is then deleted.
  static void AddCallbackForPageLoad(base::OnceClosure callback);

  // Whether there is a listener matching the request that has
  // ExtraInfoSpec::EXTRA_HEADERS set.
  bool HasExtraHeadersListenerForRequest(
      content::BrowserContext* browser_context,
      const WebRequestInfo* request);

  // Whether there is a listener matching the request that has
  // ExtraInfoSpec::SECURITY_INFO set.
  bool HasSecurityInfoListenerForRequest(
      content::BrowserContext* browser_context,
      const WebRequestInfo* request);

  // Whether there are any listeners for this context that have
  // ExtraInfoSpec::EXTRA_HEADERS set.
  bool HasAnyExtraHeadersListener(content::BrowserContext* browser_context);

  // Whether there are any listeners for this context that have
  // ExtraInfoSpec::SECURITY_INFO set.
  bool HasAnySecurityInfoListener(content::BrowserContext* browser_context);

  // Called when a BrowserContext is being destroyed.
  void OnBrowserContextShutdown(content::BrowserContext* browser_context);

  // Get the number of listeners - for testing only.
  size_t GetListenerCountForTesting(content::BrowserContext* browser_context,
                                    const std::string& event_name);

  // Get the number of blocked requests owned by `browser_context` - for
  // testing only.
  size_t GetBlockedRequestCountForTesting(
      content::BrowserContext* browser_context) const;

  size_t GetInactiveListenerCount(content::BrowserContext* browser_context,
                                  const std::string& event_name);

  // Get details of an inactive listener given event name - for testing only.
  bool GetInactiveListenerDetailsForTesting(
      content::BrowserContext* browser_context,
      const ExtensionId& extension_id,
      const std::string& event_name,
      RequestFilter** filter,
      int* extra_info_spec);

  bool HasAnyExtraHeadersListenerForTesting(
      content::BrowserContext* browser_context) {
    return HasAnyExtraHeadersListenerImpl(browser_context);
  }
  bool HasAnySecurityInfoListenerForTesting(
      content::BrowserContext* browser_context) {
    return HasAnySecurityInfoListenerImpl(browser_context);
  }

  // Updates active listeners in tests that do not need process-specific
  // matching.
  void UpdateActiveListenerForTesting(content::BrowserContext* browser_context,
                                      ListenerUpdateType update_type,
                                      const ExtensionId& extension_id,
                                      const std::string& sub_event_name,
                                      int worker_thread_id,
                                      int64_t service_worker_version_id) {
    UpdateActiveListener(browser_context, update_type, extension_id,
                         sub_event_name, std::nullopt, worker_thread_id,
                         service_worker_version_id);
  }

  // Updates only the active listener in tests whose render process matches
  // `render_process_id`. `filter`, `extra_info_spec`, and
  // `web_view_instance_id` narrow the update for per-context (parent event
  // named) registrations.
  void UpdateActiveListenerForTesting(
      content::BrowserContext* browser_context,
      ListenerUpdateType update_type,
      const ExtensionId& extension_id,
      const std::string& sub_event_name,
      content::ChildProcessId render_process_id,
      int worker_thread_id,
      int64_t service_worker_version_id,
      const RequestFilter* filter = nullptr,
      std::optional<int> extra_info_spec = std::nullopt,
      std::optional<int> web_view_instance_id = std::nullopt) {
    UpdateActiveListener(browser_context, update_type, extension_id,
                         sub_event_name, render_process_id, worker_thread_id,
                         service_worker_version_id, filter, extra_info_spec,
                         web_view_instance_id);
  }

  // Removes a lazy or worker listener in tests. `filter` and
  // `extra_info_spec` narrow the removal for per-context registrations.
  void RemoveLazyListenerForTesting(
      content::BrowserContext* original_context,
      const ExtensionId& extension_id,
      const std::string& sub_event_name,
      const RequestFilter* filter = nullptr,
      std::optional<int> extra_info_spec = std::nullopt) {
    RemoveLazyListener(original_context, extension_id, sub_event_name, filter,
                       extra_info_spec);
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(ExtensionWebRequestTest, BrowserContextShutdown);

  // Identifier for a `BrowserContext` to scope the lifetime for references.
  // `BrowserContextID` is derived from `BrowserContext*`, used in comparison
  // only, and are never dereferenced.
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
         content::ChildProcessId render_process_id,
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
      content::ChildProcessId render_process_id;
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

    bool HasSecurityInfo() const {
      using extension_web_request_api_helpers::ExtraInfoSpec;
      return extra_info_spec & ExtraInfoSpec::SECURITY_INFO;
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
    absl::flat_hash_set<uint64_t> blocked_requests;
  };

  using RawListeners = std::vector<EventListener*>;
  using ListenerIDs = std::vector<EventListener::ID>;
  using Listeners = std::vector<std::unique_ptr<EventListener>>;
  using ListenerMap = std::map<std::string, Listeners>;
  using BlockedRequestMap = std::map<uint64_t, BlockedRequest>;

  // Identifies a renderer-side dispatch target for per-context event dispatch.
  //
  // Because stopped lazy contexts lack assigned process or worker IDs, a
  // logical target can use two keys during a request:
  // - A placeholder lazy key (`IsLazy()`) for event dispatch and startup.
  // - A concrete key with actual renderer IDs for actions like responding or
  //   unregistering.
  struct DispatchTargetKey {
    // Returns the key of the dispatch target that `listener` belongs to. For
    // a stopped lazy context's registration this is the target's lazy key.
    static DispatchTargetKey ForListener(const EventListener& listener);

    // Constructs a key from a renderer response's routing IDs. The returned key
    // is always concrete (non-lazy) because event responses originate from
    // active renderer contexts.
    static DispatchTargetKey ForResponse(
        content::BrowserContext& browser_context,
        const ExtensionId& extension_id,
        content::ChildProcessId render_process_id,
        int web_view_instance_id,
        int worker_thread_id,
        int64_t service_worker_version_id);

    BrowserContextID listener_context_id = 0;
    ExtensionId extension_id;
    content::ChildProcessId render_process_id;
    int worker_thread_id = kMainThreadId;
    int64_t service_worker_version_id =
        blink::mojom::kInvalidServiceWorkerVersionId;
    int web_view_instance_id = 0;

    bool IsLazy() const { return render_process_id.is_null(); }

    friend auto operator<=>(const DispatchTargetKey&,
                            const DispatchTargetKey&) = default;
  };

  // Map of pending targets that still owe the browser a completion signal
  // (`OnEventHandlingDone`) for a single stage of a blocked request.
  // Entries are keyed by the target's initial dispatch identity, and the mapped
  // integer stores the union of the matched blocking listeners'
  // `extra_info_spec`. Lazy keys remain fixed throughout the request's lifetime
  // even when the underlying context starts up and acquires a concrete
  // identity.
  using PendingTargetMap = base::flat_map<DispatchTargetKey, int>;

  // A matched pending target for a renderer event response. `key` is the
  // matched map key; note that for a woken lazy context it remains lazy,
  // differing from the responding context's concrete identity.
  // `blocking_union_spec` is the union of the target's matched blocking
  // listeners' `extra_info_spec`.
  struct RespondingTarget {
    raw_ptr<BlockedRequest> blocked_request;
    DispatchTargetKey key;
    int blocking_union_spec;
  };

  enum class ListenerCountUpdate {
    kIncrement,
    kAlreadyCounted,
  };

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
    int extra_headers_listeners_count = 0;
    // The number of listeners that request security info be included with their
    // events. Modified through `IncrementSecurityInfoListenerCount()` and
    // `DecrementSecurityInfoListenerCount()`.
    int security_info_listeners_count = 0;
    // Maps each BrowserContext using the webview key to its respective rules
    // registry. For non-webview contexts, the default value defined by
    // `RulesRegistryService::kDefaultRulesRegistryID` is used.
    std::map<int, scoped_refptr<WebRequestRulesRegistry>> rules_registries;

    SignaledRequestIDTracker signaled_request_id_tracker;
  };

  using DataMap = std::map<BrowserContextID, BrowserContextData>;

  // Returns the active EventListener with the given `id`, or nullptr. See
  // `FindEventListenerInContainer()` for the role of `filter` and
  // `extra_info_spec`.
  EventListener* FindEventListener(
      const EventListener::ID& id,
      const RequestFilter* filter = nullptr,
      std::optional<int> extra_info_spec = std::nullopt);

  // Returns the active EventListener corresponding to the provided
  // `browser_context_id`, `extension_id`, `event_name`, and `sub_event_name`,
  // or nullptr if not found. See `FindEventListenerInContainer()` for the role
  // of `filter` and `extra_info_spec`.
  EventListener* FindEventListenerBySubEventName(
      BrowserContextID browser_context_id,
      const ExtensionId& extension_id,
      const std::string& event_name,
      const std::string& sub_event_name,
      const RequestFilter* filter = nullptr,
      std::optional<int> extra_info_spec = std::nullopt);

  // Returns the EventListener with the given `id` from `listeners`. For
  // per-context registrations (whose sub-event name equals the event name and
  // is shared by all of an extension's listeners), `filter` and
  // `extra_info_spec` narrow the lookup to a single registration.
  // TODO(crbug.com/494684626): Consider folding the registration identity
  // (filter, extra_info_spec) into `EventListener::ID` so that an ID alone
  // uniquely identifies a listener again.
  EventListener* FindEventListenerInContainer(
      const EventListener::ID& id,
      const Listeners& listeners,
      const RequestFilter* filter = nullptr,
      std::optional<int> extra_info_spec = std::nullopt);

  // Updates the active listener registration indicated by the given criteria.
  // `update_type` indicates whether the listener is fully removed or if it's
  // a lazy listener that had its context shut down. For per-context
  // registrations (whose sub-event name equals the event name and is shared
  // by all of an extension's listeners), `filter`, `extra_info_spec`, and
  // `web_view_instance_id` narrow the update to a single registration.
  void UpdateActiveListener(
      content::BrowserContext* browser_context,
      ListenerUpdateType update_type,
      const ExtensionId& extension_id,
      const std::string& sub_event_name,
      std::optional<content::ChildProcessId> render_process_id,
      int worker_thread_id,
      int64_t service_worker_version_id,
      const RequestFilter* filter = nullptr,
      std::optional<int> extra_info_spec = std::nullopt,
      std::optional<int> web_view_instance_id = std::nullopt);

  // Adds `listener` to `listeners` and updates listener counts if needed.
  void AddListenerToList(content::BrowserContext* browser_context,
                         Listeners& listeners,
                         std::unique_ptr<EventListener> listener,
                         ListenerCountUpdate count_update);

  // Removes a lazy listener registration. This affects both the provided
  // `original_context` and any incognito context associated with it. See
  // `UpdateActiveListener()` for the role of `filter` and `extra_info_spec`.
  void RemoveLazyListener(content::BrowserContext* original_context,
                          const ExtensionId& extension_id,
                          const std::string& sub_event_name,
                          const RequestFilter* filter = nullptr,
                          std::optional<int> extra_info_spec = std::nullopt);

  // Removes all listeners from `listeners` that matches the given criteria.
  // Optional criteria are ignored if not provided. Removes the matching
  // listeners, if any.
  static std::vector<std::unique_ptr<EventListener>> RemoveMatchingListeners(
      Listeners& listeners,
      const ExtensionId& extension_id,
      const std::string& sub_event_name,
      std::optional<content::ChildProcessId> render_process_id,
      std::optional<int> worker_thread_id,
      std::optional<int64_t> service_worker_version_id,
      BrowserContextID browser_context_id,
      const RequestFilter* filter = nullptr,
      std::optional<int> extra_info_spec = std::nullopt,
      std::optional<int> web_view_instance_id = std::nullopt);

  // Replaces inactive listeners for the same extension id and sub-event name.
  // Returns the number of exact registration matches preserved.
  size_t ReplaceInactiveListeners(Listeners& inactive_listeners,
                                  const ExtensionId& extension_id,
                                  const std::string& sub_event_name,
                                  BrowserContextID browser_context_id,
                                  EventListener& replacement_listener);

  // Cleans up for a listener being removed, unblocking any requests and
  // updating counts as appropriate.
  void CleanUpForListener(EventListener& listener,
                          ListenerUpdateType removal_type);

  // Ensures that future callbacks for `request` are ignored so that it can be
  // destroyed safely.
  void ClearPendingCallbacks(const WebRequestInfo& request);

  bool DispatchEvent(content::BrowserContext* browser_context,
                     const WebRequestInfo* request,
                     const RawListeners& listener_ids,
                     std::unique_ptr<WebRequestEventDetails> event_details);

  void DispatchEventToListeners(
      content::BrowserContext* browser_context,
      std::unique_ptr<ListenerIDs> listener_ids,
      uint64_t request_id,
      std::unique_ptr<WebRequestEventDetails> event_details);

  // Dispatches one event per dispatch target to the per-context
  // (parent-event named) `listeners`, recording a pending target for each
  // target that contains blocking listeners. Returns the number of blocking
  // targets recorded.
  int DispatchEventToTargets(content::BrowserContext* browser_context,
                             const WebRequestInfo* request,
                             const RawListeners& listeners,
                             const WebRequestEventDetails& event_details);

  // Groups the matched `listeners` by their renderer dispatch target,
  // producing exactly one entry per context.
  //
  // Static member rather than anonymous namespace helper because the signature
  // names private nested types (`RawListeners`, `DispatchTargetKey`).
  static std::map<DispatchTargetKey, RawListeners>
  GroupListenersByDispatchTarget(const RawListeners& listeners);

  // Finds the pending target in `blocked_request` matching `key`, returning
  // std::nullopt if not found. If an exact match fails, falls back to
  // searching for a lazy entry with the same logical identity (extension ID,
  // browser context, and webview ID) to account for lazy contexts that woke up
  // under a concrete identity.
  std::optional<RespondingTarget> FindPendingTarget(
      BlockedRequest& blocked_request,
      const DispatchTargetKey& key);

  // Finds the pending target addressed by a renderer response matching the
  // given request, event stage, extension, and routing IDs. Returns
  // std::nullopt if no matching target exists.
  std::optional<RespondingTarget> FindTargetForResponse(
      content::BrowserContext& browser_context,
      const ExtensionId& extension_id,
      const std::string& event_name,
      uint64_t request_id,
      content::ChildProcessId render_process_id,
      int web_view_instance_id,
      int worker_thread_id,
      int64_t service_worker_version_id);

  // Resolves the pending target matching `dispatch_key` for `request_id`
  // without applying response deltas. Bound as an event's
  // `cannot_dispatch_callback` to unblock requests when a target is unreachable
  // at dispatch time (e.g., worker start failure).
  // `ResolvePendingTargetsForTeardown()` also calls this on target teardown.
  void OnTargetCannotDispatch(const std::string& event_name,
                              uint64_t request_id,
                              const DispatchTargetKey& dispatch_key);

  // Resolves a pending target by erasing it from `blocked_request` and
  // decrementing the blocking count (which resumes the request if no blocking
  // sources remain). Does nothing if no such target remains.
  void ResolvePendingTarget(BlockedRequest& blocked_request,
                            const DispatchTargetKey& target_key,
                            const std::string& event_name,
                            uint64_t request_id);

  // Converts `response` into an EventResponseDelta clamped by the permissions
  // in `extra_info_spec`, logs the API usage to the activity monitor, and
  // appends the delta to `blocked_request`.
  void AppendResponseDelta(BlockedRequest& blocked_request,
                           const ExtensionId& extension_id,
                           const std::string& event_name,
                           EventResponse& response,
                           int extra_info_spec);

  // ProcessManagerObserver:
  void OnStoppedTrackingServiceWorkerInstance(
      content::BrowserContext& browser_context,
      const WorkerId& worker_id) override;
  void OnProcessManagerShutdown(ProcessManager* manager) override;

  // content::RenderProcessHostObserver:
  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

  // Starts to observe the ProcessManager of `browser_context`.
  void ObserveProcessManager(content::BrowserContext* browser_context);

  // Starts to observe `host`, so the router resolves the host's pending
  // targets when the process goes away.
  void ObserveRenderProcessHost(content::RenderProcessHost* host);

  // Resolves (without responses) every pending target whose key satisfies
  // `matches`, across the blocked requests of every BrowserContext that
  // shares this router.
  void ResolvePendingTargetsForTeardown(
      base::FunctionRef<bool(const DispatchTargetKey&)> matches);

  // Returns a list of event listeners that care about the given event, based
  // on their filter parameters. `extra_info_spec` will contain the combined
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
  void DecrementBlockCount(const ExtensionId& extension_id,
                           const std::string& event_name,
                           uint64_t request_id,
                           std::unique_ptr<EventResponse> response,
                           int extra_info_spec);

  // Processes the generated deltas from `blocked_requests_` on the specified
  // request. `browser_context` is the context that owns the blocked request. If
  // `call_callback` is true, the callback registered in `blocked_requests_` is
  // called. The function returns the error code for the network request. This
  // is mostly relevant in case the caller passes `call_callback` = false and
  // wants to return the correct network error code themself.
  int ExecuteDeltas(content::BrowserContext* browser_context,
                    const WebRequestInfo* request,
                    bool call_callback);

  // Evaluates the rules of the declarative webrequest API and stores
  // modifications to the request that result from WebRequestActions as
  // deltas in `blocked_requests_`. `filtered_response_headers` should only be
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

  // Sets the flag that `event_type` has been signaled for `request_id`.
  // Returns the value of the flag before setting it.
  bool GetAndSetSignaled(content::BrowserContext* browser_context,
                         uint64_t request_id,
                         EventTypes event_type);

  // Clears the flag that `event_type` has been signaled for `request_id`.
  void ClearSignaled(content::BrowserContext* browser_context,
                     uint64_t request_id,
                     EventTypes event_type);

  // Returns whether `request` represents a top level window navigation.
  bool IsPageLoad(const WebRequestInfo& request) const;

  // Called on a page load to process all registered callbacks.
  void NotifyPageLoad();

  // Returns the matching cross browser_context (the regular browser_context if
  // `browser_context` is OTR and vice versa).
  static content::BrowserContext* GetCrossBrowserContext(
      content::BrowserContext* browser_context);

  // Returns true if `request_id` was already signaled to some event handlers.
  bool WasSignaled(content::BrowserContext* browser_context,
                   uint64_t request_id) const;

  void IncrementExtraHeadersListenerCount(
      content::BrowserContext* browser_context);
  void DecrementExtraHeadersListenerCount(
      content::BrowserContext* browser_context);

  void IncrementSecurityInfoListenerCount(
      content::BrowserContext* browser_context);
  void DecrementSecurityInfoListenerCount(
      content::BrowserContext* browser_context);

  // Helper for |HasAnyExtraHeadersListener()|.
  bool HasAnyExtraHeadersListenerImpl(content::BrowserContext* browser_context);

  // Helper for |HasAnySecurityInfoListener()|.
  bool HasAnySecurityInfoListenerImpl(content::BrowserContext* browser_context);

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

  // Clears the entry in `blocked_requests_` with `id`, if any.
  void ClearBlockedRequest(uint64_t id);

  // Gets the entry in `blocked_requests_` with `id`. The entry is created if
  // it doesn't exist, owned by `browser_context` (the context the request
  // belongs to). An existing entry must be owned by `browser_context`.
  BlockedRequest& GetOrAddBlockedRequest(
      content::BrowserContext* browser_context,
      uint64_t id);

  // Gets the existing entry in `blocked_requests_` with `id`. The entry is
  // not created if it doesn't exist.
  BlockedRequest* GetBlockedRequest(uint64_t id);

  // Returns the blocked request identified by `request_id` if it is currently
  // waiting on the stage named `event_name`, or nullptr otherwise.
  BlockedRequest* GetBlockedRequestForEvent(uint64_t request_id,
                                            const std::string& event_name);

  // A map of data associated with given BrowserContexts.
  DataMap data_;

  // The network requests that are waiting for at least one event handler to
  // respond, keyed by request ID. IDs are unique across the BrowserContexts
  // that share this router, and a response can arrive from the cross browser
  // context, so entries record their owning context and lookup is by ID only.
  BlockedRequestMap blocked_requests_;

  const raw_ptr<content::BrowserContext> browser_context_;

  // Observes the ProcessManagers of the contexts that recorded pending
  // targets, for worker-stop signals.
  base::ScopedMultiSourceObservation<ProcessManager, ProcessManagerObserver>
      process_manager_observations_{this};

  // Observes every render process that hosts a concrete pending target.
  base::ScopedMultiSourceObservation<content::RenderProcessHost,
                                     content::RenderProcessHostObserver>
      render_process_host_observations_{this};

  base::WeakPtrFactory<WebRequestEventRouter> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_WEB_REQUEST_EXTENSION_WEB_REQUEST_EVENT_ROUTER_H_
