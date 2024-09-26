// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_factory.h"

#include <cstddef>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "net/base/host_mapping_rules.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_isolation_key.h"
#include "net/base/parse_number.h"
#include "net/base/port_util.h"
#include "net/base/privacy_mode.h"
#include "net/base/upload_data_stream.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/http_network_session.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_stream_factory_job.h"
#include "net/http/http_stream_factory_job_controller.h"
#include "net/http/transport_security_state.h"
#include "net/quic/quic_http_utils.h"
#include "net/socket/socket_tag.h"
#include "net/spdy/bidirectional_stream_spdy_impl.h"
#include "net/spdy/spdy_http_stream.h"
#include "net/ssl/ssl_config.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_alt_svc_wire_format.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_packets.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_server_id.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace net {

namespace {
const char kAlternativeServiceHeader[] = "Alt-Svc";

}  // namespace

// static
SpdySessionKey HttpStreamFactory::GetSpdySessionKey(
    const ProxyChain& proxy_chain,
    const GURL& origin_url,
    const StreamRequestInfo& request_info) {
  // In the case that we'll be sending a GET request to the proxy, look for an
  // HTTP/2 proxy session *to* the proxy, instead of to the origin server. The
  // way HTTP over HTTPS proxies work is that the ConnectJob makes a SpdyProxy,
  // and then the HttpStreamFactory detects it when it's added to the
  // SpdySession pool, and uses it directly (completely ignoring the result of
  // the ConnectJob, and in fact cancelling it). So we need to create the same
  // key used by the HttpProxyConnectJob for the last proxy in the chain.
  if (IsGetToProxy(proxy_chain, origin_url)) {
    // For this to work as expected, the whole chain should be HTTPS.
    for (const auto& proxy_server : proxy_chain.proxy_servers()) {
      CHECK(proxy_server.is_https());
    }
    auto [last_proxy_partial_chain, last_proxy_server] =
        proxy_chain.SplitLast();
    const auto& last_proxy_host_port_pair = last_proxy_server.host_port_pair();
    // Note that `disable_cert_network_fetches` must be true for proxies to
    // avoid deadlock. See comment on
    // `SSLConfig::disable_cert_verification_network_fetches`.
    return SpdySessionKey(
        last_proxy_host_port_pair, PRIVACY_MODE_DISABLED,
        last_proxy_partial_chain, SessionUsage::kProxy, request_info.socket_tag,
        request_info.network_anonymization_key, request_info.secure_dns_policy,
        /*disable_cert_network_fetches=*/true);
  }
  return SpdySessionKey(
      HostPortPair::FromURL(origin_url), request_info.privacy_mode, proxy_chain,
      SessionUsage::kDestination, request_info.socket_tag,
      request_info.network_anonymization_key, request_info.secure_dns_policy,
      request_info.load_flags & LOAD_DISABLE_CERT_NETWORK_FETCHES);
}

// static
bool HttpStreamFactory::IsGetToProxy(const ProxyChain& proxy_chain,
                                     const GURL& origin_url) {
  // Sending proxied GET requests to the last proxy server in the chain is no
  // longer supported for QUIC.
  return proxy_chain.is_get_to_proxy_allowed() &&
         proxy_chain.Last().is_https() && origin_url.SchemeIs(url::kHttpScheme);
}

HttpStreamFactory::StreamRequestInfo::StreamRequestInfo() = default;

HttpStreamFactory::StreamRequestInfo::StreamRequestInfo(
    const HttpRequestInfo& http_request_info)
    : method(http_request_info.method),
      network_anonymization_key(http_request_info.network_anonymization_key),
      is_http1_allowed(!http_request_info.upload_data_stream ||
                       http_request_info.upload_data_stream->AllowHTTP1()),
      load_flags(http_request_info.load_flags),
      privacy_mode(http_request_info.privacy_mode),
      secure_dns_policy(http_request_info.secure_dns_policy),
      socket_tag(http_request_info.socket_tag) {}

HttpStreamFactory::StreamRequestInfo::StreamRequestInfo(
    const StreamRequestInfo& other) = default;
HttpStreamFactory::StreamRequestInfo&
HttpStreamFactory::StreamRequestInfo::operator=(
    const StreamRequestInfo& other) = default;
HttpStreamFactory::StreamRequestInfo::StreamRequestInfo(
    StreamRequestInfo&& other) = default;
HttpStreamFactory::StreamRequestInfo&
HttpStreamFactory::StreamRequestInfo::operator=(StreamRequestInfo&& other) =
    default;

HttpStreamFactory::StreamRequestInfo::~StreamRequestInfo() = default;

HttpStreamFactory::HttpStreamFactory(HttpNetworkSession* session)
    : session_(session), job_factory_(std::make_unique<JobFactory>()) {}

HttpStreamFactory::~HttpStreamFactory() = default;

void HttpStreamFactory::ProcessAlternativeServices(
    HttpNetworkSession* session,
    const NetworkAnonymizationKey& network_anonymization_key,
    const HttpResponseHeaders* headers,
    const url::SchemeHostPort& http_server) {
  if (!headers->HasHeader(kAlternativeServiceHeader))
    return;

  std::string alternative_service_str;
  headers->GetNormalizedHeader(kAlternativeServiceHeader,
                               &alternative_service_str);
  spdy::SpdyAltSvcWireFormat::AlternativeServiceVector
      alternative_service_vector;
  if (!spdy::SpdyAltSvcWireFormat::ParseHeaderFieldValue(
          alternative_service_str, &alternative_service_vector)) {
    return;
  }

  session->http_server_properties()->SetAlternativeServices(
      RewriteHost(http_server), network_anonymization_key,
      net::ProcessAlternativeServices(
          alternative_service_vector, session->params().enable_http2,
          session->params().enable_quic,
          session->context().quic_context->params()->supported_versions));
}

url::SchemeHostPort HttpStreamFactory::RewriteHost(
    const url::SchemeHostPort& server) {
  HostPortPair host_port_pair(server.host(), server.port());
  const HostMappingRules* mapping_rules = GetHostMappingRules();
  if (mapping_rules)
    mapping_rules->RewriteHost(&host_port_pair);
  return url::SchemeHostPort(server.scheme(), host_port_pair.host(),
                             host_port_pair.port());
}

std::unique_ptr<HttpStreamRequest> HttpStreamFactory::RequestStream(
    const HttpRequestInfo& request_info,
    RequestPriority priority,
    const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs,
    HttpStreamRequest::Delegate* delegate,
    bool enable_ip_based_pooling,
    bool enable_alternative_services,
    const NetLogWithSource& net_log) {
  return RequestStreamInternal(request_info, priority, allowed_bad_certs,
                               delegate, nullptr,
                               HttpStreamRequest::HTTP_STREAM,
                               /*is_websocket=*/false, enable_ip_based_pooling,
                               enable_alternative_services, net_log);
}

std::unique_ptr<HttpStreamRequest>
HttpStreamFactory::RequestWebSocketHandshakeStream(
    const HttpRequestInfo& request_info,
    RequestPriority priority,
    const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs,
    HttpStreamRequest::Delegate* delegate,
    WebSocketHandshakeStreamBase::CreateHelper* create_helper,
    bool enable_ip_based_pooling,
    bool enable_alternative_services,
    const NetLogWithSource& net_log) {
  DCHECK(create_helper);
  return RequestStreamInternal(request_info, priority, allowed_bad_certs,
                               delegate, create_helper,
                               HttpStreamRequest::HTTP_STREAM,
                               /*is_websocket=*/true, enable_ip_based_pooling,
                               enable_alternative_services, net_log);
}

std::unique_ptr<HttpStreamRequest>
HttpStreamFactory::RequestBidirectionalStreamImpl(
    const HttpRequestInfo& request_info,
    RequestPriority priority,
    const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs,
    HttpStreamRequest::Delegate* delegate,
    bool enable_ip_based_pooling,
    bool enable_alternative_services,
    const NetLogWithSource& net_log) {
  DCHECK(request_info.url.SchemeIs(url::kHttpsScheme));

  return RequestStreamInternal(request_info, priority, allowed_bad_certs,
                               delegate, nullptr,
                               HttpStreamRequest::BIDIRECTIONAL_STREAM,
                               /*is_websocket=*/false, enable_ip_based_pooling,
                               enable_alternative_services, net_log);
}

std::unique_ptr<HttpStreamRequest> HttpStreamFactory::RequestStreamInternal(
    const HttpRequestInfo& request_info,
    RequestPriority priority,
    const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs,
    HttpStreamRequest::Delegate* delegate,
    WebSocketHandshakeStreamBase::CreateHelper*
        websocket_handshake_stream_create_helper,
    HttpStreamRequest::StreamType stream_type,
    bool is_websocket,
    bool enable_ip_based_pooling,
    bool enable_alternative_services,
    const NetLogWithSource& net_log) {
  // This is only needed in the non-preconnect path, as preconnects do not
  // require a NetworkIsolationKey.
  DCHECK(request_info.IsConsistent());

  auto job_controller = std::make_unique<JobController>(
      this, delegate, session_, job_factory_.get(), request_info,
      /* is_preconnect = */ false, is_websocket, enable_ip_based_pooling,
      enable_alternative_services,
      session_->context()
          .quic_context->params()
          ->delay_main_job_with_available_spdy_session,
      allowed_bad_certs);
  JobController* job_controller_raw_ptr = job_controller.get();
  job_controller_set_.insert(std::move(job_controller));
  return job_controller_raw_ptr->Start(delegate,
                                       websocket_handshake_stream_create_helper,
                                       net_log, stream_type, priority);
}

void HttpStreamFactory::PreconnectStreams(int num_streams,
                                          HttpRequestInfo& request_info) {
  // Ignore invalid URLs. This matches the behavior of
  // URLRequestJobFactory::CreateJob(). Passing very long valid GURLs over Mojo
  // can result in invalid URLs, so can't rely on callers sending only valid
  // URLs.
  if (!request_info.url.is_valid()) {
    OnPreconnectsCompleteInternal();
    return;
  }

  auto job_controller = std::make_unique<JobController>(
      this, nullptr, session_, job_factory_.get(), request_info,
      /*is_preconnect=*/true,
      /*is_websocket=*/false,
      /*enable_ip_based_pooling=*/true,
      /*enable_alternative_services=*/true,
      session_->context()
          .quic_context->params()
          ->delay_main_job_with_available_spdy_session,
      /*allowed_bad_certs=*/std::vector<SSLConfig::CertAndStatus>());
  JobController* job_controller_raw_ptr = job_controller.get();
  job_controller_set_.insert(std::move(job_controller));
  job_controller_raw_ptr->Preconnect(num_streams);
}

const HostMappingRules* HttpStreamFactory::GetHostMappingRules() const {
  return &session_->params().host_mapping_rules;
}

void HttpStreamFactory::OnJobControllerComplete(JobController* controller) {
  auto it = job_controller_set_.find(controller);
  if (it != job_controller_set_.end()) {
    job_controller_set_.erase(it);
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

}  // namespace net
