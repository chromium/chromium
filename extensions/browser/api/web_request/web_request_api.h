// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_API_H_
#define EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_API_H_

#include <stdint.h>

#include <list>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/unique_ptr_adapters.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_request_id.h"
#include "extensions/browser/api/declarative/rules_registry.h"
#include "extensions/browser/api/declarative_webrequest/request_stage.h"
#include "extensions/browser/api/web_request/web_request_api_helpers.h"
#include "extensions/browser/api/web_request/web_request_permissions.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/url_pattern_set.h"
#include "ipc/ipc_sender.h"
#include "net/base/auth.h"
#include "net/base/completion_once_callback.h"
#include "net/base/network_delegate.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/websocket.mojom.h"

class ExtensionWebRequestTimeTracker;
class GURL;

namespace base {
class DictionaryValue;
}

namespace content {
class BrowserContext;
class RenderFrameHost;
}

namespace net {
class AuthChallengeInfo;
class AuthCredentials;
class HttpRequestHeaders;
class HttpResponseHeaders;
}

namespace extensions {

enum class WebRequestResourceType : uint8_t;

class InfoMap;
class WebRequestEventDetails;
struct WebRequestInfo;
class WebRequestRulesRegistry;

// Support class for the WebRequest API. Lives on the UI thread. Most of the
// work is done by ExtensionWebRequestEventRouter below. This class observes
// extensions::EventRouter to deal with event listeners. There is one instance
// per BrowserContext which is shared with incognito.
class WebRequestAPI : public BrowserContextKeyedAPI,
                      public EventRouter::Observer,
                      public ExtensionRegistryObserver {
 public:
  // A callback used to asynchronously respond to an intercepted authentication
  // request when the Network Service is enabled. If |should_cancel| is true
  // the request will be cancelled. Otherwise any supplied |credentials| will be
  // used. If no credentials are supplied, default browser behavior will follow
  // (e.g. UI prompt for login).
  using AuthRequestCallback = base::OnceCallback<void(
      const base::Optional<net::AuthCredentials>& credentials,
      bool should_cancel)>;

  // An interface which is held by ProxySet defined below.
  class Proxy {
   public:
    virtual ~Proxy() {}

    // Asks the Proxy to handle an auth request on behalf of one of its known
    // in-progress network requests. If the request will *not* be handled by
    // the proxy, |callback| should be invoked with |base::nullopt|.
    virtual void HandleAuthRequest(
        net::AuthChallengeInfo* auth_info,
        scoped_refptr<net::HttpResponseHeaders> response_headers,
        int32_t request_id,
        AuthRequestCallback callback);
  };

  // A ProxySet is a set of proxies used by WebRequestAPI: It holds Proxy
  // instances, and removes all proxies when the ResourceContext it is bound to
  // is destroyed.
  class ProxySet : public base::SupportsUserData::Data {
   public:
    ProxySet();
    ~ProxySet() override;

    // Gets or creates a ProxySet from the given ResourceContext.
    static ProxySet* GetFromResourceContext(
        content::ResourceContext* resource_context);

    // Add a Proxy.
    void AddProxy(std::unique_ptr<Proxy> proxy);
    // Remove a Proxy. The removed proxy is deleted upon this call.
    void RemoveProxy(Proxy* proxy);

    // Associates |proxy| with |id|. |proxy| must already be registered within
    // this ProxySet.
    //
    // Each Proxy may be responsible for multiple requests, but any given
    // request identified by |id| must be associated with only a single proxy.
    void AssociateProxyWithRequestId(Proxy* proxy,
                                     const content::GlobalRequestID& id);

    // Disassociates |proxy| with |id|. |proxy| must already be registered
    // within this ProxySet.
    void DisassociateProxyWithRequestId(Proxy* proxy,
                                        const content::GlobalRequestID& id);

    Proxy* GetProxyFromRequestId(const content::GlobalRequestID& id);

    void MaybeProxyAuthRequest(
        net::AuthChallengeInfo* auth_info,
        scoped_refptr<net::HttpResponseHeaders> response_headers,
        const content::GlobalRequestID& request_id,
        AuthRequestCallback callback);

   private:
    // Although these members are initialized on the UI thread, we expect at
    // least one memory barrier before actually calling Generate in the IO
    // thread, so we don't protect them with a lock.
    std::set<std::unique_ptr<Proxy>, base::UniquePtrComparator> proxies_;

    // Bi-directional mapping between request ID and Proxy for faster lookup.
    std::map<content::GlobalRequestID, Proxy*> request_id_to_proxy_map_;
    std::map<Proxy*, std::set<content::GlobalRequestID>>
        proxy_to_request_id_map_;

    DISALLOW_COPY_AND_ASSIGN(ProxySet);
  };

  class RequestIDGenerator
      : public base::RefCountedThreadSafe<RequestIDGenerator> {
   public:
    RequestIDGenerator() = default;
    int64_t Generate() {
      DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
      return ++id_;
    }

   private:
    friend class base::RefCountedThreadSafe<RequestIDGenerator>;
    ~RequestIDGenerator() {}

    // Although this initialization can be done in a thread other than the IO
    // thread, we expect at least one memory barrier before actually calling
    // Generate in the IO thread, so we don't protect the variable with a lock.
    int64_t id_ = 0;
    DISALLOW_COPY_AND_ASSIGN(RequestIDGenerator);
  };

  explicit WebRequestAPI(content::BrowserContext* context);
  ~WebRequestAPI() override;

  // BrowserContextKeyedAPI support:
  static BrowserContextKeyedAPIFactory<WebRequestAPI>* GetFactoryInstance();
  void Shutdown() override;

  // EventRouter::Observer overrides:
  void OnListenerRemoved(const EventListenerInfo& details) override;

  // If any WebRequest event listeners are currently active for this
  // BrowserContext, |*factory_request| is swapped out for a new request which
  // proxies through an internal URLLoaderFactory. This supports lifetime
  // observation and control on behalf of the WebRequest API.
  //
  // Returns |true| if the URLLoaderFactory will be proxied; |false| otherwise.
  // Only used when the Network Service is enabled.
  bool MaybeProxyURLLoaderFactory(
      content::RenderFrameHost* frame,
      bool is_navigation,
      network::mojom::URLLoaderFactoryRequest* factory_request);

  // Any request which requires authentication to complete will be bounced
  // through this method iff Network Service is enabled.
  //
  // If this returns |true|, |callback| will eventually be invoked on the UI
  // thread.
  bool MaybeProxyAuthRequest(
      net::AuthChallengeInfo* auth_info,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      const content::GlobalRequestID& request_id,
      bool is_main_frame,
      AuthRequestCallback callback);

  // If any WebRequest event listeners are currently active for this
  // BrowserContext, |*request| is swapped out for a new request which proxies
  // through an internal WebSocket implementation. This supports lifetime
  // observation and control on behalf of the WebRequest API.
  //
  // Only used when the Network Service is enabled.
  void MaybeProxyWebSocket(
      content::RenderFrameHost* frame,
      network::mojom::WebSocketRequest* request,
      network::mojom::AuthenticationHandlerPtr* auth_handler);

  void ForceProxyForTesting();

 private:
  friend class BrowserContextKeyedAPIFactory<WebRequestAPI>;

  // BrowserContextKeyedAPI support:
  static const char* service_name() { return "WebRequestAPI"; }
  static const bool kServiceRedirectedInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

  // Indicates whether or not the WebRequestAPI may have one or more proxies
  // installed to support the API with Network Service enabled.
  bool MayHaveProxies() const;

  // Checks if |MayHaveProxies()| has changed from false to true, and resets
  // URLLoaderFactories if so.
  void UpdateMayHaveProxies();

  // ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // A count of active extensions for this BrowserContext that use web request
  // permissions.
  int web_request_extension_count_ = 0;

  content::BrowserContext* const browser_context_;
  InfoMap* const info_map_;

  scoped_refptr<RequestIDGenerator> request_id_generator_;

  // Stores the last result of |MayHaveProxies()|, so it can be used in
  // |UpdateMayHaveProxies()|.
  bool may_have_proxies_;

  DISALLOW_COPY_AND_ASSIGN(WebRequestAPI);
};

// This class observes network events and routes them to the appropriate
// extensions listening to those events. All methods must be called on the IO
// thread unless otherwise specified.
class ExtensionWebRequestEventRouter {
 public:
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

  // Internal representation of the webRequest.RequestFilter type, used to
  // filter what network events an extension cares about.
  struct RequestFilter {
    RequestFilter();
    RequestFilter(const RequestFilter& other);
    ~RequestFilter();

    // Returns false if there was an error initializing. If it is a user error,
    // an error message is provided, otherwise the error is internal (and
    // unexpected).
    bool InitFromValue(const base::DictionaryValue& value, std::string* error);

    extensions::URLPatternSet urls;
    std::vector<WebRequestResourceType> types;
    int tab_id;
    int window_id;
  };

  // Contains an extension's response to a blocking event.
  struct EventResponse {
    EventResponse(const std::string& extension_id,
                  const base::Time& extension_install_time);
    ~EventResponse();

    // ID of the extension that sent this response.
    std::string extension_id;

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

    std::unique_ptr<net::AuthCredentials> auth_credentials;

   private:
    DISALLOW_COPY_AND_ASSIGN(EventResponse);
  };

  static ExtensionWebRequestEventRouter* GetInstance();

  // Registers a rule registry. Pass null for |rules_registry| to unregister
  // the rule registry for |browser_context|.
  void RegisterRulesRegistry(
      void* browser_context,
      int rules_registry_id,
      scoped_refptr<extensions::WebRequestRulesRegistry> rules_registry);

  // Dispatches the OnBeforeRequest event to any extensions whose filters match
  // the given request. Returns net::ERR_IO_PENDING if an extension is
  // intercepting the request and OK if the request should proceed normally.
  // net::ERR_BLOCKED_BY_CLIENT is returned if the request should be blocked. In
  // this case, |should_collapse_initiator| might be set to true indicating
  // whether the DOM element which initiated the request should be blocked.
  int OnBeforeRequest(void* browser_context,
                      const extensions::InfoMap* extension_info_map,
                      WebRequestInfo* request,
                      net::CompletionOnceCallback callback,
                      GURL* new_url,
                      bool* should_collapse_initiator);

  // Dispatches the onBeforeSendHeaders event. This is fired for HTTP(s)
  // requests only, and allows modification of the outgoing request headers.
  // Returns net::ERR_IO_PENDING if an extension is intercepting the request, OK
  // otherwise.
  int OnBeforeSendHeaders(void* browser_context,
                          const extensions::InfoMap* extension_info_map,
                          const WebRequestInfo* request,
                          net::CompletionOnceCallback callback,
                          net::HttpRequestHeaders* headers);

  // Dispatches the onSendHeaders event. This is fired for HTTP(s) requests
  // only.
  void OnSendHeaders(void* browser_context,
                     const extensions::InfoMap* extension_info_map,
                     const WebRequestInfo* request,
                     const net::HttpRequestHeaders& headers);

  // Dispatches the onHeadersReceived event. This is fired for HTTP(s)
  // requests only, and allows modification of incoming response headers.
  // Returns net::ERR_IO_PENDING if an extension is intercepting the request,
  // OK otherwise. |original_response_headers| is reference counted. |callback|
  // |override_response_headers| and |allowed_unsafe_redirect_url| are not owned
  // but are guaranteed to be valid until |callback| is called or
  // OnRequestWillBeDestroyed is called (whatever comes first).
  // Do not modify |original_response_headers| directly but write new ones
  // into |override_response_headers|.
  int OnHeadersReceived(
      void* browser_context,
      const extensions::InfoMap* extension_info_map,
      const WebRequestInfo* request,
      net::CompletionOnceCallback callback,
      const net::HttpResponseHeaders* original_response_headers,
      scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
      GURL* allowed_unsafe_redirect_url);

  // Dispatches the OnAuthRequired event to any extensions whose filters match
  // the given request. If the listener is not registered as "blocking", then
  // AUTH_REQUIRED_RESPONSE_OK is returned. Otherwise,
  // AUTH_REQUIRED_RESPONSE_IO_PENDING is returned and |callback| will be
  // invoked later.
  net::NetworkDelegate::AuthRequiredResponse OnAuthRequired(
      void* browser_context,
      const extensions::InfoMap* extension_info_map,
      const WebRequestInfo* request,
      const net::AuthChallengeInfo& auth_info,
      net::NetworkDelegate::AuthCallback callback,
      net::AuthCredentials* credentials);

  // Dispatches the onBeforeRedirect event. This is fired for HTTP(s) requests
  // only.
  void OnBeforeRedirect(void* browser_context,
                        const extensions::InfoMap* extension_info_map,
                        const WebRequestInfo* request,
                        const GURL& new_location);

  // Dispatches the onResponseStarted event indicating that the first bytes of
  // the response have arrived.
  void OnResponseStarted(void* browser_context,
                         const extensions::InfoMap* extension_info_map,
                         const WebRequestInfo* request,
                         int net_error);

  // Dispatches the onComplete event.
  void OnCompleted(void* browser_context,
                   const extensions::InfoMap* extension_info_map,
                   const WebRequestInfo* request,
                   int net_error);

  // Dispatches an onErrorOccurred event.
  void OnErrorOccurred(void* browser_context,
                       const extensions::InfoMap* extension_info_map,
                       const WebRequestInfo* request,
                       bool started,
                       int net_error);

  // Notificaties when |request| is no longer being processed, regardless of
  // whether it has gone to completion or merely been cancelled. This is
  // guaranteed to be called eventually for any request observed by this object,
  // and |*request| will be immintently destroyed after this returns.
  void OnRequestWillBeDestroyed(void* browser_context,
                                const WebRequestInfo* request);

  // Called when an event listener handles a blocking event and responds.
  void OnEventHandled(void* browser_context,
                      const std::string& extension_id,
                      const std::string& event_name,
                      const std::string& sub_event_name,
                      uint64_t request_id,
                      EventResponse* response);

  // Adds a listener to the given event. |event_name| specifies the event being
  // listened to. |sub_event_name| is an internal event uniquely generated in
  // the extension process to correspond to the given filter and
  // extra_info_spec. It returns true on success, false on failure.
  bool AddEventListener(void* browser_context,
                        const std::string& extension_id,
                        const std::string& extension_name,
                        events::HistogramValue histogram_value,
                        const std::string& event_name,
                        const std::string& sub_event_name,
                        const RequestFilter& filter,
                        int extra_info_spec,
                        int embedder_process_id,
                        int web_view_instance_id,
                        base::WeakPtr<IPC::Sender> ipc_sender);

  // Removes the listeners for a given <webview>.
  void RemoveWebViewEventListeners(
      void* browser_context,
      int embedder_process_id,
      int web_view_instance_id);

  // Called when an incognito browser_context is created or destroyed.
  void OnOTRBrowserContextCreated(void* original_browser_context,
                                  void* otr_browser_context);
  void OnOTRBrowserContextDestroyed(void* original_browser_context,
                                    void* otr_browser_context);

  // Registers a |callback| that is executed when the next page load happens.
  // The callback is then deleted.
  void AddCallbackForPageLoad(const base::Closure& callback);

 private:
  friend class WebRequestAPI;
  friend class base::NoDestructor<ExtensionWebRequestEventRouter>;
  FRIEND_TEST_ALL_PREFIXES(ExtensionWebRequestTest,
                           BlockingEventPrecedenceRedirect);
  FRIEND_TEST_ALL_PREFIXES(ExtensionWebRequestTest,
                           BlockingEventPrecedenceCancel);
  FRIEND_TEST_ALL_PREFIXES(ExtensionWebRequestTest,
                           SimulateChancelWhileBlocked);
  FRIEND_TEST_ALL_PREFIXES(ExtensionWebRequestTest, AccessRequestBodyData);
  FRIEND_TEST_ALL_PREFIXES(ExtensionWebRequestTest,
                           MinimalAccessRequestBodyData);
  FRIEND_TEST_ALL_PREFIXES(ExtensionWebRequestTest,
                           ProperFilteringInPublicSession);
  FRIEND_TEST_ALL_PREFIXES(ExtensionWebRequestTest, NoAccessRequestBodyData);
  FRIEND_TEST_ALL_PREFIXES(ExtensionWebRequestTest, AddAndRemoveListeners);
  FRIEND_TEST_ALL_PREFIXES(ExtensionWebRequestTest, BlockedRequestsAreRemoved);
  FRIEND_TEST_ALL_PREFIXES(ExtensionWebRequestHeaderModificationTest,
                           TestModifications);

  struct EventListener {
    // An EventListener is uniquely defined by five properties.
    // TODO(rdevlin.cronin): There are two types of EventListeners - those
    // associated with WebViews and those that are not. The ones associated with
    // WebViews are always identified by all five properties. The other ones
    // will always have web_view_instance_id = 0. Unfortunately, the
    // callbacks/interfaces for these ones don't specify embedder_process_id.
    // This is why we need the LooselyMatches method, and the need for a
    // |strict| argument on RemoveEventListener.
    struct ID {
      ID(void* browser_context,
         const std::string& extension_id,
         const std::string& sub_event_name,
         int embedder_process_id,
         int web_view_instance_id);

      // If web_view_instance_id is 0, then ignore embedder_process_id.
      // TODO(rdevlin.cronin): In a more sane world, LooselyMatches wouldn't be
      // necessary.
      bool LooselyMatches(const ID& that) const;

      bool operator==(const ID& that) const;
      void* browser_context;
      std::string extension_id;
      std::string sub_event_name;
      int embedder_process_id;
      int web_view_instance_id;
    };

    EventListener(ID id);
    ~EventListener();

    const ID id;
    std::string extension_name;
    events::HistogramValue histogram_value = events::UNKNOWN;
    RequestFilter filter;
    int extra_info_spec = 0;
    base::WeakPtr<IPC::Sender> ipc_sender;
    std::unordered_set<uint64_t> blocked_requests;

   private:
    DISALLOW_COPY_AND_ASSIGN(EventListener);
  };

  using RawListeners = std::vector<EventListener*>;
  using ListenerIDs = std::vector<EventListener::ID>;
  using Listeners = std::vector<std::unique_ptr<EventListener>>;
  using ListenerMapForBrowserContext = std::map<std::string, Listeners>;
  using ListenerMap = std::map<void*, ListenerMapForBrowserContext>;
  using BlockedRequestMap = std::map<uint64_t, BlockedRequest>;
  // Map of request_id -> bit vector of EventTypes already signaled
  using SignaledRequestMap = std::map<uint64_t, int>;
  // For each browser_context: a bool indicating whether it is an incognito
  // browser_context, and a pointer to the corresponding (non-)incognito
  // browser_context.
  using CrossBrowserContextMap = std::map<void*, std::pair<bool, void*>>;
  using CallbacksForPageLoad = std::list<base::Closure>;

  ExtensionWebRequestEventRouter();

  // This instance is leaked.
  ~ExtensionWebRequestEventRouter() = delete;

  // Returns the EventListener with the given |id|, or nullptr. Must be called
  // from the IO thread.
  EventListener* FindEventListener(const EventListener::ID& id);

  // Returns the EventListener with the given |id| from |listeners|.
  EventListener* FindEventListenerInContainer(const EventListener::ID& id,
                                              Listeners& listeners);

  // Removes the listener for the given sub-event. Must be called from the IO
  // thread.
  void RemoveEventListener(const EventListener::ID& id, bool strict);

  // Ensures that future callbacks for |request| are ignored so that it can be
  // destroyed safely.
  void ClearPendingCallbacks(const WebRequestInfo& request);

  bool DispatchEvent(void* browser_context,
                     const InfoMap* extension_info_map,
                     const WebRequestInfo* request,
                     const RawListeners& listener_ids,
                     std::unique_ptr<WebRequestEventDetails> event_details);

  void DispatchEventToListeners(
      void* browser_context,
      const InfoMap* extension_info_map,
      std::unique_ptr<ListenerIDs> listener_ids,
      std::unique_ptr<WebRequestEventDetails> event_details);

  // Returns a list of event listeners that care about the given event, based
  // on their filter parameters. |extra_info_spec| will contain the combined
  // set of extra_info_spec flags that every matching listener asked for.
  RawListeners GetMatchingListeners(
      void* browser_context,
      const extensions::InfoMap* extension_info_map,
      const std::string& event_name,
      const WebRequestInfo* request,
      int* extra_info_spec);

  // Helper for the above functions. This is called twice: once for the
  // browser_context of the event, the next time for the "cross" browser_context
  // (i.e. the incognito browser_context if the event is originally for the
  // normal browser_context, or vice versa).
  void GetMatchingListenersImpl(void* browser_context,
                                const WebRequestInfo* request,
                                const extensions::InfoMap* extension_info_map,
                                bool crosses_incognito,
                                const std::string& event_name,
                                bool is_request_from_extension,
                                int* extra_info_spec,
                                RawListeners* matching_listeners);

  // Decrements the count of event handlers blocking the given request. When the
  // count reaches 0, we stop blocking the request and proceed it using the
  // method requested by the extension with the highest precedence. Precedence
  // is decided by extension install time. If |response| is non-NULL, this
  // method assumes ownership.
  void DecrementBlockCount(void* browser_context,
                           const std::string& extension_id,
                           const std::string& event_name,
                           uint64_t request_id,
                           EventResponse* response);

  // Processes the generated deltas from blocked_requests_ on the specified
  // request. If |call_back| is true, the callback registered in
  // |blocked_requests_| is called.
  // The function returns the error code for the network request. This is
  // mostly relevant in case the caller passes |call_callback| = false
  // and wants to return the correct network error code himself.
  int ExecuteDeltas(void* browser_context,
                    const WebRequestInfo* request,
                    bool call_callback);

  // Evaluates the rules of the declarative webrequest API and stores
  // modifications to the request that result from WebRequestActions as
  // deltas in |blocked_requests_|. |original_response_headers| should only be
  // set for the OnHeadersReceived stage and NULL otherwise. Returns whether any
  // deltas were generated.
  bool ProcessDeclarativeRules(
      void* browser_context,
      const extensions::InfoMap* extension_info_map,
      const std::string& event_name,
      const WebRequestInfo* request,
      extensions::RequestStage request_stage,
      const net::HttpResponseHeaders* original_response_headers);

  // If the BlockedRequest contains messages_to_extension entries in the event
  // deltas, we send them to subscribers of
  // chrome.declarativeWebRequest.onMessage.
  void SendMessages(void* browser_context,
                    const BlockedRequest& blocked_request);

  // Called when the RulesRegistry is ready to unblock a request that was
  // waiting for said event.
  void OnRulesRegistryReady(void* browser_context,
                            const std::string& event_name,
                            uint64_t request_id,
                            extensions::RequestStage request_stage);

  // Sets the flag that |event_type| has been signaled for |request_id|.
  // Returns the value of the flag before setting it.
  bool GetAndSetSignaled(uint64_t request_id, EventTypes event_type);

  // Clears the flag that |event_type| has been signaled for |request_id|.
  void ClearSignaled(uint64_t request_id, EventTypes event_type);

  // Returns whether |request| represents a top level window navigation.
  bool IsPageLoad(const WebRequestInfo& request) const;

  // Called on a page load to process all registered callbacks.
  void NotifyPageLoad();

  // Returns the matching cross browser_context (the regular browser_context if
  // |browser_context| is OTR and vice versa).
  void* GetCrossBrowserContext(void* browser_context) const;

  // Determines whether the specified browser_context is an incognito
  // browser_context (based on the contents of the cross-browser_context table
  // and without dereferencing the browser_context pointer).
  bool IsIncognitoBrowserContext(void* browser_context) const;

  // Returns true if |request| was already signaled to some event handlers.
  bool WasSignaled(const WebRequestInfo& request) const;

  // Get the number of listeners - for testing only.
  size_t GetListenerCountForTesting(void* browser_context,
                                    const std::string& event_name);

  // A map for each browser_context that maps an event name to a set of
  // extensions that are listening to that event.
  ListenerMap listeners_;

  // A map of network requests that are waiting for at least one event handler
  // to respond.
  BlockedRequestMap blocked_requests_;

  // A map of request ids to a bitvector indicating which events have been
  // signaled and should not be sent again.
  SignaledRequestMap signaled_requests_;

  // A map of original browser_context -> corresponding incognito
  // browser_context (and vice versa).
  CrossBrowserContextMap cross_browser_context_map_;

  // Keeps track of time spent waiting on extensions using the blocking
  // webRequest API.
  std::unique_ptr<ExtensionWebRequestTimeTracker> request_time_tracker_;

  CallbacksForPageLoad callbacks_for_page_load_;

  typedef std::pair<void*, int> RulesRegistryKey;
  // Maps each browser_context (and OTRBrowserContext) and a webview key to its
  // respective rules registry.
  std::map<RulesRegistryKey,
      scoped_refptr<extensions::WebRequestRulesRegistry> > rules_registries_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionWebRequestEventRouter);
};

class WebRequestInternalFunction : public IOThreadExtensionFunction {
 public:
  WebRequestInternalFunction() {}

 protected:
  ~WebRequestInternalFunction() override {}

  const std::string& extension_id_safe() const {
    return extension() ? extension_id() : base::EmptyString();
  }
};

class WebRequestInternalAddEventListenerFunction
    : public WebRequestInternalFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webRequestInternal.addEventListener",
                             WEBREQUESTINTERNAL_ADDEVENTLISTENER)

 protected:
  ~WebRequestInternalAddEventListenerFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class WebRequestInternalEventHandledFunction
    : public WebRequestInternalFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webRequestInternal.eventHandled",
                             WEBREQUESTINTERNAL_EVENTHANDLED)

 protected:
  ~WebRequestInternalEventHandledFunction() override {}

 private:
  // Unblocks the network request. Use this function when handling incorrect
  // requests from the extension that cannot be detected by the schema
  // validator.
  void OnError(
      const std::string& event_name,
      const std::string& sub_event_name,
      uint64_t request_id,
      std::unique_ptr<ExtensionWebRequestEventRouter::EventResponse> response);

  // ExtensionFunction:
  ResponseAction Run() override;
};

class WebRequestHandlerBehaviorChangedFunction
    : public WebRequestInternalFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webRequest.handlerBehaviorChanged",
                             WEBREQUEST_HANDLERBEHAVIORCHANGED)

 protected:
  ~WebRequestHandlerBehaviorChangedFunction() override {}

  // ExtensionFunction:
  void GetQuotaLimitHeuristics(
      extensions::QuotaLimitHeuristics* heuristics) const override;
  // Handle quota exceeded gracefully: Only warn the user but still execute the
  // function.
  void OnQuotaExceeded(const std::string& error) override;
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_API_H_
