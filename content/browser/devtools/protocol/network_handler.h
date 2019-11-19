// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_NETWORK_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_NETWORK_HANDLER_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/unguessable_token.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/network.h"
#include "content/public/common/resource_type.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/net_errors.h"
#include "net/cookies/canonical_cookie.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace net {
class HttpRequestHeaders;
class SSLInfo;
class X509Certificate;
}  // namespace net

namespace network {
struct ResourceResponseHead;
struct ResourceRequest;
struct URLLoaderCompletionStatus;
}  // namespace network

namespace content {
class BrowserContext;
class DevToolsAgentHostImpl;
class DevToolsIOContext;
class DevToolsURLLoaderInterceptor;
class RenderFrameHostImpl;
class RenderProcessHost;
class NavigationRequest;
class SignedExchangeEnvelope;
class StoragePartition;
struct InterceptedRequestInfo;
struct SignedExchangeError;

namespace protocol {
class BackgroundSyncRestorer;

class NetworkHandler : public DevToolsDomainHandler,
                       public Network::Backend {
 public:
  NetworkHandler(const std::string& host_id,
                 const base::UnguessableToken& devtools_token,
                 DevToolsIOContext* io_context,
                 base::RepeatingClosure update_loader_factories_callback);
  ~NetworkHandler() override;

  static std::vector<NetworkHandler*> ForAgentHost(DevToolsAgentHostImpl* host);

  // static helpers used by other agents that depend on types defined
  // in network domain.
  static net::Error NetErrorFromString(const std::string& error, bool* ok);
  static std::string NetErrorToString(int net_error);
  static const char* ResourceTypeToString(ResourceType resource_type);
  static bool AddInterceptedResourceType(
      const std::string& resource_type,
      base::flat_set<ResourceType>* intercepted_resource_types);

  void Wire(UberDispatcher* dispatcher) override;
  void SetRenderer(int render_process_id,
                   RenderFrameHostImpl* frame_host) override;

  Response Enable(Maybe<int> max_total_size,
                  Maybe<int> max_resource_size,
                  Maybe<int> max_post_data_size) override;
  Response Disable() override;

  Response SetCacheDisabled(bool cache_disabled) override;

  void ClearBrowserCache(
      std::unique_ptr<ClearBrowserCacheCallback> callback) override;

  void ClearBrowserCookies(
      std::unique_ptr<ClearBrowserCookiesCallback> callback) override;

  void GetCookies(Maybe<protocol::Array<String>> urls,
                  std::unique_ptr<GetCookiesCallback> callback) override;
  void GetAllCookies(std::unique_ptr<GetAllCookiesCallback> callback) override;
  void DeleteCookies(const std::string& name,
                     Maybe<std::string> url,
                     Maybe<std::string> domain,
                     Maybe<std::string> path,
                     std::unique_ptr<DeleteCookiesCallback> callback) override;
  void SetCookie(const std::string& name,
                 const std::string& value,
                 Maybe<std::string> url,
                 Maybe<std::string> domain,
                 Maybe<std::string> path,
                 Maybe<bool> secure,
                 Maybe<bool> http_only,
                 Maybe<std::string> same_site,
                 Maybe<double> expires,
                 std::unique_ptr<SetCookieCallback> callback) override;
  void SetCookies(
      std::unique_ptr<protocol::Array<Network::CookieParam>> cookies,
      std::unique_ptr<SetCookiesCallback> callback) override;

  Response SetExtraHTTPHeaders(
      std::unique_ptr<Network::Headers> headers) override;
  Response CanEmulateNetworkConditions(bool* result) override;
  Response EmulateNetworkConditions(
      bool offline,
      double latency,
      double download_throughput,
      double upload_throughput,
      Maybe<protocol::Network::ConnectionType> connection_type) override;
  Response SetBypassServiceWorker(bool bypass) override;

  DispatchResponse SetRequestInterception(
      std::unique_ptr<protocol::Array<protocol::Network::RequestPattern>>
          patterns) override;
  void ContinueInterceptedRequest(
      const std::string& request_id,
      Maybe<std::string> error_reason,
      Maybe<protocol::Binary> raw_response,
      Maybe<std::string> url,
      Maybe<std::string> method,
      Maybe<std::string> post_data,
      Maybe<protocol::Network::Headers> headers,
      Maybe<protocol::Network::AuthChallengeResponse> auth_challenge_response,
      std::unique_ptr<ContinueInterceptedRequestCallback> callback) override;

  void GetResponseBodyForInterception(
      const String& interception_id,
      std::unique_ptr<GetResponseBodyForInterceptionCallback> callback)
      override;
  void TakeResponseBodyForInterceptionAsStream(
      const String& interception_id,
      std::unique_ptr<TakeResponseBodyForInterceptionAsStreamCallback> callback)
      override;

  // Note that |frame_token| below is for the frame that is associated with the
  // factory being created, and is therefore not necessarily the same as one
  // associated with the NetworkHandler itself (which is the token of the local
  // root frame).
  bool MaybeCreateProxyForInterception(
      RenderProcessHost* rph,
      const base::UnguessableToken& frame_token,
      bool is_navigation,
      bool is_download,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>*
          target_factory_receiver);

  void ApplyOverrides(net::HttpRequestHeaders* headers,
                      bool* skip_service_worker,
                      bool* disable_cache);
  void NavigationRequestWillBeSent(const NavigationRequest& nav_request,
                                   base::TimeTicks timestamp);
  void RequestSent(const std::string& request_id,
                   const std::string& loader_id,
                   const network::ResourceRequest& request,
                   const char* initiator_type,
                   const base::Optional<GURL>& initiator_url,
                   base::TimeTicks timestamp);
  void ResponseReceived(const std::string& request_id,
                        const std::string& loader_id,
                        const GURL& url,
                        const char* resource_type,
                        const network::ResourceResponseHead& head,
                        Maybe<std::string> frame_id);
  void LoadingComplete(
      const std::string& request_id,
      const char* resource_type,
      const network::URLLoaderCompletionStatus& completion_status);

  void OnSignedExchangeReceived(
      base::Optional<const base::UnguessableToken> devtools_navigation_token,
      const GURL& outer_request_url,
      const network::ResourceResponseHead& outer_response,
      const base::Optional<SignedExchangeEnvelope>& header,
      const scoped_refptr<net::X509Certificate>& certificate,
      const base::Optional<net::SSLInfo>& ssl_info,
      const std::vector<SignedExchangeError>& errors);

  void OnRequestWillBeSentExtraInfo(
      const std::string& devtools_request_id,
      const net::CookieStatusList& request_cookie_list,
      const std::vector<network::mojom::HttpRawHeaderPairPtr>& request_headers);
  void OnResponseReceivedExtraInfo(
      const std::string& devtools_request_id,
      const net::CookieAndLineStatusList& response_cookie_list,
      const std::vector<network::mojom::HttpRawHeaderPairPtr>& response_headers,
      const base::Optional<std::string>& response_headers_text);

  bool enabled() const { return enabled_; }

  Network::Frontend* frontend() const { return frontend_.get(); }

  static std::string ExtractFragment(const GURL& url, std::string* fragment);
  static std::unique_ptr<Network::Request> CreateRequestFromResourceRequest(
      const network::ResourceRequest& request,
      const std::string& cookie_line);

 private:
  void RequestIntercepted(std::unique_ptr<InterceptedRequestInfo> request_info);
  void SetNetworkConditions(network::mojom::NetworkConditionsPtr conditions);

  void OnResponseBodyPipeTaken(
      std::unique_ptr<TakeResponseBodyForInterceptionAsStreamCallback> callback,
      Response response,
      mojo::ScopedDataPipeConsumerHandle pipe,
      const std::string& mime_type);

  // TODO(dgozman): Remove this.
  const std::string host_id_;

  const base::UnguessableToken devtools_token_;
  DevToolsIOContext* const io_context_;

  std::unique_ptr<Network::Frontend> frontend_;
  BrowserContext* browser_context_;
  StoragePartition* storage_partition_;
  RenderFrameHostImpl* host_;
  bool enabled_;
  std::vector<std::pair<std::string, std::string>> extra_headers_;
  std::unique_ptr<DevToolsURLLoaderInterceptor> url_loader_interceptor_;
  bool bypass_service_worker_;
  bool cache_disabled_;
  std::unique_ptr<BackgroundSyncRestorer> background_sync_restorer_;
  base::RepeatingClosure update_loader_factories_callback_;
  base::WeakPtrFactory<NetworkHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NetworkHandler);
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_NETWORK_HANDLER_H_
