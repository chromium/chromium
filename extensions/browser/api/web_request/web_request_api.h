// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_API_H_
#define EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_API_H_

#include <stdint.h>

#include <list>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/unique_ptr_adapters.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_request_id.h"
#include "extensions/browser/api/declarative_webrequest/request_stage.h"
#include "extensions/browser/api/web_request/extension_web_request_event_router.h"
#include "extensions/browser/api/web_request/web_request_permissions.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension_id.h"
#include "ipc/ipc_sender.h"
#include "net/base/auth.h"
#include "net/base/completion_once_callback.h"
#include "net/http/http_request_headers.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/websocket.mojom.h"

class GURL;

namespace content {
class BrowserContext;
class RenderFrameHost;
}  // namespace content

namespace net {
class AuthCredentials;
class HttpResponseHeaders;
class SiteForCookies;
}  // namespace net

namespace network {
class URLLoaderFactoryBuilder;
}  // namespace network

namespace extensions {
class WebViewGuest;

// Support class for the WebRequest API. Lives on the UI thread. Most of the
// work is done by ExtensionWebRequestEventRouter below. This class observes
// extensions::EventRouter to deal with event listeners. There is one instance
// per BrowserContext which is shared with incognito.
class WebRequestAPI : public BrowserContextKeyedAPI,
                      public EventRouter::Observer,
                      public ExtensionRegistryObserver {
 public:
  // A callback used to asynchronously respond to an intercepted authentication
  // request. If |should_cancel| is true the request will be cancelled.
  // Otherwise any supplied |credentials| will be used. If no credentials are
  // supplied, default browser behavior will follow (e.g. UI prompt for login).
  using AuthRequestCallback = base::OnceCallback<void(
      const std::optional<net::AuthCredentials>& credentials,
      bool should_cancel)>;

  // An interface which is held by ProxySet defined below.
  class Proxy {
   public:
    virtual ~Proxy() = default;

    // Asks the Proxy to handle an auth request on behalf of one of its known
    // in-progress network requests. If the request will *not* be handled by
    // the proxy, |callback| should be invoked with |std::nullopt|.
    virtual void HandleAuthRequest(
        const net::AuthChallengeInfo& auth_info,
        scoped_refptr<net::HttpResponseHeaders> response_headers,
        int32_t request_id,
        AuthRequestCallback callback);

    // Called when an extension that can execute declarativeNetRequest actions
    // is unloaded, so orphaned DNR actions on current requests can be cleaned
    // up.
    virtual void OnDNRExtensionUnloaded(const Extension* extension) = 0;
  };

  // A ProxySet is a set of proxies used by WebRequestAPI: It holds Proxy
  // instances, and removes all proxies when it is destroyed.
  class ProxySet {
   public:
    ProxySet();

    ProxySet(const ProxySet&) = delete;
    ProxySet& operator=(const ProxySet&) = delete;

    ~ProxySet();

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
        const net::AuthChallengeInfo& auth_info,
        scoped_refptr<net::HttpResponseHeaders> response_headers,
        const content::GlobalRequestID& request_id,
        AuthRequestCallback callback);

    void OnDNRExtensionUnloaded(const Extension* extension);

   private:
    // Although these members are initialized on the UI thread, we expect at
    // least one memory barrier before actually calling Generate in the IO
    // thread, so we don't protect them with a lock.
    std::set<std::unique_ptr<Proxy>, base::UniquePtrComparator> proxies_;

    // Bi-directional mapping between request ID and Proxy for faster lookup.
    std::map<content::GlobalRequestID, raw_ptr<Proxy, CtnExperimental>>
        request_id_to_proxy_map_;
    std::map<Proxy*, std::set<content::GlobalRequestID>>
        proxy_to_request_id_map_;
  };

  class RequestIDGenerator {
   public:
    RequestIDGenerator();

    RequestIDGenerator(const RequestIDGenerator&) = delete;
    RequestIDGenerator& operator=(const RequestIDGenerator&) = delete;

    ~RequestIDGenerator();

    // Generates a WebRequest ID. If the same (routing_id,
    // network_service_request_id) pair is passed to this as was previously
    // passed to SaveID(), the |request_id| passed to SaveID() will be returned.
    int64_t Generate(int32_t routing_id, int32_t network_service_request_id);

    // This saves a WebRequest ID mapped to the (routing_id,
    // network_service_request_id) pair. Clients must call Generate() with the
    // same ID pair to retrieve the |request_id|, or else there may be a memory
    // leak.
    void SaveID(int32_t routing_id,
                int32_t network_service_request_id,
                uint64_t request_id);

   private:
    int64_t id_ = 0;
    std::map<std::pair<int32_t, int32_t>, uint64_t> saved_id_map_;
  };

  explicit WebRequestAPI(content::BrowserContext* context);

  WebRequestAPI(const WebRequestAPI&) = delete;
  WebRequestAPI& operator=(const WebRequestAPI&) = delete;

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
  // |frame| and |render_process_id| are the frame and render process id in
  // which the URLLoaderFactory will be used. |frame| can be nullptr for
  // factories proxied for service worker.
  //
  // |navigation_response_task_runner| is a task runner that may be non-null for
  // navigation requests and can be used to run navigation request blocking
  // tasks.
  //
  // Returns |true| if the URLLoaderFactory will be proxied; |false| otherwise.
  bool MaybeProxyURLLoaderFactory(
      content::BrowserContext* browser_context,
      content::RenderFrameHost* frame,
      int render_process_id,
      content::ContentBrowserClient::URLLoaderFactoryType type,
      std::optional<int64_t> navigation_id,
      ukm::SourceIdObj ukm_source_id,
      network::URLLoaderFactoryBuilder& factory_builder,
      mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
          header_client,
      scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner,
      const url::Origin& request_initiator = url::Origin());

  // Any request which requires authentication to complete will be bounced
  // through this method.
  //
  // If this returns |true|, |callback| will eventually be invoked on the UI
  // thread.
  bool MaybeProxyAuthRequest(
      content::BrowserContext* browser_context,
      const net::AuthChallengeInfo& auth_info,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      const content::GlobalRequestID& request_id,
      bool is_request_for_navigation,
      AuthRequestCallback callback,
      WebViewGuest* web_view_guest);

  // Starts proxying the connection with |factory|. This function can be called
  // only when MayHaveProxies() returns true.
  void ProxyWebSocket(
      content::RenderFrameHost* frame,
      content::ContentBrowserClient::WebSocketFactory factory,
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      const std::optional<std::string>& user_agent,
      mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
          handshake_client);

  // Starts proxying WebTransport handshake.
  void ProxyWebTransport(
      content::RenderProcessHost& render_process_host,
      int frame_routing_id,
      const GURL& url,
      const url::Origin& initiator_origin,
      mojo::PendingRemote<network::mojom::WebTransportHandshakeClient>
          handshake_client,
      content::ContentBrowserClient::WillCreateWebTransportCallback callback);

  void ForceProxyForTesting();

  // Indicates whether or not the WebRequestAPI may have one or more proxies
  // installed to support the API.
  bool MayHaveProxies() const;

  // Indicates whether or not WebRequestAPI may have one or more proxies
  // installed to support intercepting websocket connections for extension
  // telemetry.
  // TODO(psarouthakis): This is here for the current implementation, but
  // will be refactored to live somewhere else so that we don't have to
  // create a full proxy just for telemetry.
  bool MayHaveWebsocketProxiesForExtensionTelemetry() const;

  // Indicates whether the WebRequestAPI is available to a RenderFrameHost
  // that embeds a WebView instance.
  bool IsAvailableToWebViewEmbedderFrame(
      content::RenderFrameHost* render_frame_host) const;

  bool HasExtraHeadersListenerForTesting();

 private:
  friend class BrowserContextKeyedAPIFactory<WebRequestAPI>;

  // BrowserContextKeyedAPI support:
  static const char* service_name() { return "WebRequestAPI"; }
  static const bool kServiceRedirectedInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

  // Checks if |MayHaveProxies()| has changed from false to true, and resets
  // URLLoaderFactories if so.
  void UpdateMayHaveProxies();

  // ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // This a proxy API for the tasks that are posted. It is either called
  // when the task is run and forwards to the corresponding member function
  // in ExtensionWebRequestEventRouter, or not, if the owning BrowserContext
  // goes away or the WeakPtr instance bound in the callback is invalidated.
  void UpdateActiveListener(
      void* browser_context_id,
      WebRequestEventRouter::ListenerUpdateType update_type,
      const ExtensionId& extension_id,
      const std::string& sub_event_name,
      int worker_thread_id,
      int64_t service_worker_version_id);

  // This a proxy API for the tasks that are posted. It is either called
  // when the task is run and forwards to the corresponding member function
  // in ExtensionWebRequestEventRouter, or not, if the owning BrowserContext
  // goes away or the WeakPtr instance bound in the callback is invalidated.
  void RemoveLazyListener(content::BrowserContext* browser_context,
                          const ExtensionId& extension_id,
                          const std::string& sub_event_name);

  // A count of active extensions for this BrowserContext that use web request
  // permissions.
  int web_request_extension_count_ = 0;

  const raw_ptr<content::BrowserContext, DanglingUntriaged> browser_context_;

  RequestIDGenerator request_id_generator_;
  std::unique_ptr<ProxySet> proxies_;

  // Stores the last result of |MayHaveProxies()|, so it can be used in
  // |UpdateMayHaveProxies()|.
  bool may_have_proxies_;

  base::WeakPtrFactory<WebRequestAPI> weak_factory_{this};
};

class WebRequestInternalFunction : public ExtensionFunction {
 public:
  WebRequestInternalFunction() = default;

 protected:
  ~WebRequestInternalFunction() override = default;

  const ExtensionId& extension_id_safe() const {
    return extension() ? extension_id() : base::EmptyString();
  }
};

class WebRequestInternalAddEventListenerFunction
    : public WebRequestInternalFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webRequestInternal.addEventListener",
                             WEBREQUESTINTERNAL_ADDEVENTLISTENER)

 protected:
  ~WebRequestInternalAddEventListenerFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class WebRequestInternalEventHandledFunction
    : public WebRequestInternalFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webRequestInternal.eventHandled",
                             WEBREQUESTINTERNAL_EVENTHANDLED)

 protected:
  ~WebRequestInternalEventHandledFunction() override = default;

 private:
  // Unblocks the network request. Use this function when handling incorrect
  // requests from the extension that cannot be detected by the schema
  // validator.
  void OnError(const std::string& event_name,
               const std::string& sub_event_name,
               uint64_t request_id,
               int render_process_id,
               int web_view_instance_id,
               std::unique_ptr<WebRequestEventRouter::EventResponse> response);

  // ExtensionFunction:
  ResponseAction Run() override;
};

class WebRequestHandlerBehaviorChangedFunction
    : public WebRequestInternalFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webRequest.handlerBehaviorChanged",
                             WEBREQUEST_HANDLERBEHAVIORCHANGED)

 protected:
  ~WebRequestHandlerBehaviorChangedFunction() override = default;

  // ExtensionFunction:
  void GetQuotaLimitHeuristics(
      extensions::QuotaLimitHeuristics* heuristics) const override;
  // Handle quota exceeded gracefully: Only warn the user but still execute the
  // function.
  void OnQuotaExceeded(std::string error) override;
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_API_H_
