// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_NETWORK_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_NETWORK_HANDLER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/network.h"
#include "content/browser/devtools/protocol/protocol.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/net_errors.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/filter/source_stream_type.h"
#include "net/net_buildflags.h"
#include "services/network/public/mojom/devtools_observer.mojom-forward.h"
#include "services/network/public/mojom/http_raw_headers.mojom-forward.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"

#if BUILDFLAG(ENABLE_REPORTING)
#include "services/network/public/mojom/reporting_service.mojom.h"
#endif  // BUILDFLAG(ENABLE_REPORTING)

namespace net {
class HttpRequestHeaders;
class SSLInfo;
class X509Certificate;
}  // namespace net

namespace network {
struct CorsErrorStatus;
struct ResourceRequest;
struct URLLoaderCompletionStatus;
namespace mojom {
class URLLoaderFactoryOverride;
}
}  // namespace network

namespace content {
class BrowserContext;
class DevToolsAgentHostClient;
class DevToolsAgentHostImpl;
class DevToolsIOContext;
class DevToolsURLLoaderInterceptor;
class RenderFrameHostImpl;
class NavigationRequest;
class SignedExchangeEnvelope;
class StoragePartition;
struct InterceptedRequestInfo;
struct SignedExchangeError;

namespace protocol {
class BackgroundSyncRestorer;
class DevToolsNetworkResourceLoader;

class NetworkHandler : public DevToolsDomainHandler,
#if BUILDFLAG(ENABLE_REPORTING)
                       public network::mojom::ReportingApiObserver,
#endif  // BUILDFLAG(ENABLE_REPORTING)
                       public Network::Backend {
 public:
  NetworkHandler(const std::string& host_id,
                 const base::UnguessableToken& devtools_token,
                 DevToolsIOContext* io_context,
                 StoragePartition* maybe_storage_partition,
                 base::RepeatingClosure update_loader_factories_callback,
                 DevToolsAgentHostClient* client,
                 base::OnceClosure cleanup_after_modifications_callback =
                     base::OnceClosure());

  NetworkHandler(const NetworkHandler&) = delete;
  NetworkHandler& operator=(const NetworkHandler&) = delete;

  ~NetworkHandler() override;

  static std::vector<NetworkHandler*> ForAgentHost(DevToolsAgentHostImpl* host);

  // static helpers used by other agents that depend on types defined
  // in network domain.
  static net::Error NetErrorFromString(const std::string& error, bool* ok);
  static std::string NetErrorToString(int net_error);
  static const char* ResourceTypeToString(
      blink::mojom::ResourceType resource_type);
  static bool AddInterceptedResourceType(
      const std::string& resource_type,
      base::flat_set<blink::mojom::ResourceType>* intercepted_resource_types);
  static std::unique_ptr<Array<Network::Cookie>> BuildCookieArray(
      const std::vector<net::CanonicalCookie>& cookie_list);
  static void SetCookies(
      StoragePartition* storage_partition,
      std::unique_ptr<protocol::Array<Network::CookieParam>> cookies,
      base::OnceCallback<void(bool)> callback);

  void Wire(UberDispatcher* dispatcher) override;
  void SetRenderer(int render_process_id,
                   RenderFrameHostImpl* frame_host) override;

  Response Enable(std::optional<int> max_total_size,
                  std::optional<int> max_resource_size,
                  std::optional<int> max_post_data_size,
                  std::optional<bool> report_direct_socket_traffic,
                  std::optional<bool> enable_durable_messages) override;
  Response Disable() override;

#if BUILDFLAG(ENABLE_REPORTING)
  void OnReportAdded(const net::ReportingReport& report) override;
  void OnReportUpdated(const net::ReportingReport& report) override;
  void OnEndpointsUpdatedForOrigin(
      const std::vector<net::ReportingEndpoint>& endpoints) override;
  std::unique_ptr<protocol::Network::ReportingApiReport> BuildProtocolReport(
      const net::ReportingReport& report);
  std::unique_ptr<protocol::Network::ReportingApiEndpoint>
  BuildProtocolEndpoint(const net::ReportingEndpoint& endpoint);
#endif  // BUILDFLAG(ENABLE_REPORTING)

  Response EnableReportingApi(bool enable) override;

  Response SetCacheDisabled(bool cache_disabled) override;

  Response SetAcceptedEncodings(
      std::unique_ptr<Array<Network::ContentEncoding>> encodings) override;

  Response ClearAcceptedEncodingsOverride() override;

  void ClearBrowserCache(
      std::unique_ptr<ClearBrowserCacheCallback> callback) override;

  void ClearBrowserCookies(
      std::unique_ptr<ClearBrowserCookiesCallback> callback) override;

  void GetCookies(std::unique_ptr<protocol::Array<String>> urls,
                  std::unique_ptr<GetCookiesCallback> callback) override;
  void GetAllCookies(std::unique_ptr<GetAllCookiesCallback> callback) override;
  void DeleteCookies(const std::string& name,
                     std::optional<std::string> url,
                     std::optional<std::string> domain,
                     std::optional<std::string> path,
                     std::unique_ptr<Network::CookiePartitionKey> partition_key,
                     std::unique_ptr<DeleteCookiesCallback> callback) override;
  void SetCookie(const std::string& name,
                 const std::string& value,
                 std::optional<std::string> url,
                 std::optional<std::string> domain,
                 std::optional<std::string> path,
                 std::optional<bool> secure,
                 std::optional<bool> http_only,
                 std::optional<std::string> same_site,
                 std::optional<double> expires,
                 std::optional<std::string> priority,
                 std::optional<bool> same_party,
                 std::optional<std::string> source_scheme,
                 std::optional<int> source_port,
                 std::unique_ptr<Network::CookiePartitionKey> partition_key,
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
      std::optional<protocol::Network::ConnectionType> connection_type,
      std::optional<double> packet_loss,
      std::optional<int> packet_queue_length,
      std::optional<bool> packet_reordering) override;
  Response EmulateNetworkConditionsByRule(
      bool offline,
      std::unique_ptr<protocol::Array<protocol::Network::NetworkConditions>>
          matched_network_conditions,
      std::unique_ptr<protocol::Array<String>>* rule_ids_result) override;
  Response SetBypassServiceWorker(bool bypass) override;

  DispatchResponse SetRequestInterception(
      std::unique_ptr<protocol::Array<protocol::Network::RequestPattern>>
          patterns) override;
  void ContinueInterceptedRequest(
      const std::string& request_id,
      std::optional<std::string> error_reason,
      std::optional<protocol::Binary> raw_response,
      std::optional<std::string> url,
      std::optional<std::string> method,
      std::optional<std::string> post_data,
      std::unique_ptr<protocol::Network::Headers> headers,
      std::unique_ptr<protocol::Network::AuthChallengeResponse>
          auth_challenge_response,
      std::unique_ptr<ContinueInterceptedRequestCallback> callback) override;

  void GetResponseBodyForInterception(
      const String& interception_id,
      std::unique_ptr<GetResponseBodyForInterceptionCallback> callback)
      override;
  void GetResponseBody(
      const String& request_id,
      std::unique_ptr<GetResponseBodyCallback> callback) override;
  void TakeResponseBodyForInterceptionAsStream(
      const String& interception_id,
      std::unique_ptr<TakeResponseBodyForInterceptionAsStreamCallback> callback)
      override;

  // Note that |frame_token| below is for the frame that is associated with the
  // factory being created, and is therefore not necessarily the same as one
  // associated with the NetworkHandler itself (which is the token of the local
  // root frame).
  bool MaybeCreateProxyForInterception(
      int process_id,
      StoragePartition* storage_partition,
      const base::UnguessableToken& frame_token,
      bool is_navigation,
      bool is_download,
      network::mojom::URLLoaderFactoryOverride* intercepting_factory);

  void ApplyOverrides(
      net::HttpRequestHeaders* headers,
      bool* skip_service_worker,
      bool* disable_cache,
      std::optional<std::vector<net::SourceStreamType>>* accepted_stream_types,
      GURL* referrer_override);
  void ApplyCookieControlsOverrides(net::CookieSettingOverrides& overrides);
  void PrefetchRequestWillBeSent(
      const std::string& request_id,
      const network::ResourceRequest& request,
      const GURL& initiator_url,
      std::optional<std::string> frame_token,
      base::TimeTicks timestamp,
      std::optional<
          std::pair<const GURL&,
                    const network::mojom::URLResponseHeadDevToolsInfo&>>
          redirect_info);

  void NavigationRequestWillBeSent(const NavigationRequest& nav_request,
                                   base::TimeTicks timestamp);
  void FencedFrameReportRequestSent(const std::string& request_id,
                                    const network::ResourceRequest& request,
                                    const std::string& event_data,
                                    base::TimeTicks timestamp);
  void RequestSent(const std::string& request_id,
                   const std::string& loader_id,
                   const net::HttpRequestHeaders& request_headers,
                   const network::mojom::URLRequestDevToolsInfo& request_info,
                   const char* initiator_type,
                   const std::optional<GURL>& initiator_url,
                   const std::string& initiator_devtools_request_id,
                   std::optional<base::UnguessableToken> frame_token,
                   base::TimeTicks timestamp);
  void ResponseReceived(const std::string& request_id,
                        const std::string& loader_id,
                        const GURL& url,
                        const char* resource_type,
                        const network::mojom::URLResponseHeadDevToolsInfo& head,
                        std::optional<std::string> frame_id);
  void LoadingComplete(
      const std::string& request_id,
      const char* resource_type,
      const network::URLLoaderCompletionStatus& completion_status);

  void FetchKeepAliveRequestWillBeSent(
      const std::string& request_id,
      const network::ResourceRequest& request,
      const GURL& initiator_url,
      std::optional<std::string> frame_token,
      base::TimeTicks timestamp,
      std::optional<
          std::pair<const GURL&,
                    const network::mojom::URLResponseHeadDevToolsInfo&>>
          redirect_info);

  void OnSignedExchangeReceived(
      std::optional<const base::UnguessableToken> devtools_navigation_token,
      const GURL& outer_request_url,
      const network::mojom::URLResponseHead& outer_response,
      const std::optional<SignedExchangeEnvelope>& header,
      const scoped_refptr<net::X509Certificate>& certificate,
      const std::optional<net::SSLInfo>& ssl_info,
      const std::vector<SignedExchangeError>& errors);

  DispatchResponse GetSecurityIsolationStatus(
      std::optional<String> in_frameId,
      std::unique_ptr<protocol::Network::SecurityIsolationStatus>* out_info)
      override;

  void OnRequestWillBeSentExtraInfo(
      const std::string& devtools_request_id,
      const net::CookieAccessResultList& request_cookie_list,
      const std::vector<network::mojom::HttpRawHeaderPairPtr>& request_headers,
      const base::TimeTicks timestamp,
      const network::mojom::ClientSecurityStatePtr& security_state,
      const network::mojom::OtherPartitionInfoPtr& other_partition_info,
      std::optional<base::UnguessableToken> applied_network_conditions_id);
  void OnResponseReceivedExtraInfo(
      const std::string& devtools_request_id,
      const net::CookieAndLineAccessResultList& response_cookie_list,
      const std::vector<network::mojom::HttpRawHeaderPairPtr>& response_headers,
      const std::optional<std::string>& response_headers_text,
      network::mojom::IPAddressSpace resource_address_space,
      int32_t http_status_code,
      const std::optional<net::CookiePartitionKey>& cookie_partition_key);
  void OnResponseReceivedEarlyHints(
      const std::string& devtools_request_id,
      const std::vector<network::mojom::HttpRawHeaderPairPtr>&
          response_headers);
  void OnTrustTokenOperationDone(
      const std::string& devtools_request_id,
      const network::mojom::TrustTokenOperationResult& result);

  void OnPolicyContainerHostUpdated();
  bool enabled() const { return enabled_; }

  Network::Frontend* frontend() const { return frontend_.get(); }

  static std::string ExtractFragment(const GURL& url, std::string* fragment);
  static std::unique_ptr<Network::Request> CreateRequestFromResourceRequest(
      const network::ResourceRequest& request,
      const std::string& cookie_line,
      std::vector<base::expected<std::vector<uint8_t>, std::string>>
          request_bodies);

  void LoadNetworkResource(
      std::optional<content::protocol::String> frameId,
      const String& url,
      std::unique_ptr<protocol::Network::LoadNetworkResourceOptions> options,
      std::unique_ptr<LoadNetworkResourceCallback> callback) override;

  DispatchResponse SetCookieControls(
      bool enable_third_party_cookie_restriction,
      bool disable_third_party_cookie_metadata,
      bool disable_third_party_cookie_heuristics) override;

  // Protocol builders.
  static String BuildPrivateNetworkRequestPolicy(
      network::mojom::PrivateNetworkRequestPolicy policy);
  static protocol::Network::IPAddressSpace BuildIpAddressSpace(
      network::mojom::IPAddressSpace space);
  static std::unique_ptr<protocol::Network::ClientSecurityState>
  MaybeBuildClientSecurityState(
      const network::mojom::ClientSecurityStatePtr& state);
  static std::unique_ptr<protocol::Network::CorsErrorStatus>
  BuildCorsErrorStatus(const network::CorsErrorStatus& status);

  void BodyDataReceived(const String& request_id,
                        const String& body,
                        bool is_base64_encoded);

  void FedCmRequestWillBeSent(
      const std::string& request_id,
      const std::string& loader_id,
      const network::ResourceRequest& request,
      const std::optional<std::string>& request_body,
      const GURL& initiator_url,
      const std::optional<base::UnguessableToken>& frame_token,
      base::TimeTicks timestamp);

 private:
  void OnLoadNetworkResourceFinished(DevToolsNetworkResourceLoader* loader,
                                     const net::HttpResponseHeaders* rh,
                                     bool success,
                                     int net_error,
                                     std::string content);
  void RequestIntercepted(std::unique_ptr<InterceptedRequestInfo> request_info);
  void SetNetworkConditions(
      std::vector<network::mojom::MatchedNetworkConditionsPtr>
          matched_conditions,
      bool offline);
  void ConfigureDurableMessageCollector(
      network::mojom::NetworkDurableMessageConfigPtr config);
  void ProcessDurableMessageOrGetLocalData(
      const String& request_id,
      std::unique_ptr<GetResponseBodyCallback> callback,
      std::optional<mojo_base::BigBuffer> durable_message);
  void OnResponseBodyPipeTaken(
      std::unique_ptr<TakeResponseBodyForInterceptionAsStreamCallback> callback,
      Response response,
      mojo::ScopedDataPipeConsumerHandle pipe,
      const std::string& mime_type);

  void GotAllCookies(std::unique_ptr<GetAllCookiesCallback> callback,
                     const std::vector<net::CanonicalCookie>& cookies);
  void MaybeEnableDurableMessages();

  // TODO(dgozman): Remove this.
  const std::string host_id_;

  const base::UnguessableToken devtools_token_;
  const raw_ptr<DevToolsIOContext> io_context_;
  raw_ptr<DevToolsAgentHostClient> client_;

  std::unique_ptr<Network::Frontend> frontend_;
  raw_ptr<BrowserContext> browser_context_;
  raw_ptr<StoragePartition> storage_partition_;
  raw_ptr<RenderFrameHostImpl> host_;
  bool enabled_ = false;
  bool enable_third_party_cookie_restriction_ = false;
  bool disable_third_party_cookie_metadata_ = false;
  bool disable_third_party_cookie_heuristics_ = false;
  bool enable_durable_messages_ = false;
  int durable_message_max_total_size_ = 0;

#if BUILDFLAG(ENABLE_REPORTING)
  mojo::Receiver<network::mojom::ReportingApiObserver> reporting_receiver_;
#endif  // BUILDFLAG(ENABLE_REPORTING)
  std::vector<std::pair<std::string, std::string>> extra_headers_;
  std::unique_ptr<DevToolsURLLoaderInterceptor> url_loader_interceptor_;
  bool bypass_service_worker_;
  bool cache_disabled_;
  std::unique_ptr<BackgroundSyncRestorer> background_sync_restorer_;
  base::RepeatingClosure update_loader_factories_callback_;
  std::map<std::unique_ptr<DevToolsNetworkResourceLoader>,
           std::unique_ptr<LoadNetworkResourceCallback>,
           base::UniquePtrComparator>
      loaders_;
  std::optional<std::set<net::SourceStreamType>> accepted_stream_types_;
  std::unordered_map<String, std::pair<String, bool>> received_body_data_;
  bool did_modifications_ = false;
  base::OnceClosure cleanup_after_modifications_callback_;
  mojo::Remote<network::mojom::DurableMessageCollector>
      durable_message_collector_;
  base::WeakPtrFactory<NetworkHandler> weak_factory_{this};
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_NETWORK_HANDLER_H_
