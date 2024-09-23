// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_pool_test_util.h"

#include "net/base/completion_once_callback.h"
#include "net/base/connection_endpoint_metadata.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/stream_socket.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

namespace {

IPEndPoint MakeIPEndPoint(std::string_view addr, uint16_t port = 80) {
  return IPEndPoint(*IPAddress::FromIPLiteral(addr), port);
}

}  // namespace

FakeServiceEndpointRequest::FakeServiceEndpointRequest() = default;

FakeServiceEndpointRequest::~FakeServiceEndpointRequest() = default;

FakeServiceEndpointRequest&
FakeServiceEndpointRequest::CompleteStartSynchronously(int rv) {
  start_result_ = rv;
  endpoints_crypto_ready_ = true;
  return *this;
}

FakeServiceEndpointRequest&
FakeServiceEndpointRequest::CallOnServiceEndpointsUpdated() {
  CHECK(delegate_);
  delegate_->OnServiceEndpointsUpdated();
  return *this;
}

FakeServiceEndpointRequest&
FakeServiceEndpointRequest::CallOnServiceEndpointRequestFinished(int rv) {
  CHECK(delegate_);
  endpoints_crypto_ready_ = true;
  delegate_->OnServiceEndpointRequestFinished(rv);
  return *this;
}

int FakeServiceEndpointRequest::Start(Delegate* delegate) {
  CHECK(!delegate_);
  CHECK(delegate);
  delegate_ = delegate;
  return start_result_;
}

const std::vector<ServiceEndpoint>&
FakeServiceEndpointRequest::GetEndpointResults() {
  return endpoints_;
}

const std::set<std::string>& FakeServiceEndpointRequest::GetDnsAliasResults() {
  return aliases_;
}

bool FakeServiceEndpointRequest::EndpointsCryptoReady() {
  return endpoints_crypto_ready_;
}

ResolveErrorInfo FakeServiceEndpointRequest::GetResolveErrorInfo() {
  return resolve_error_info_;
}

void FakeServiceEndpointRequest::ChangeRequestPriority(
    RequestPriority priority) {
  priority_ = priority;
}

FakeServiceEndpointResolver::FakeServiceEndpointResolver() = default;

FakeServiceEndpointResolver::~FakeServiceEndpointResolver() = default;

FakeServiceEndpointRequest* FakeServiceEndpointResolver::AddFakeRequest() {
  std::unique_ptr<FakeServiceEndpointRequest> request =
      std::make_unique<FakeServiceEndpointRequest>();
  FakeServiceEndpointRequest* raw_request = request.get();
  requests_.emplace_back(std::move(request));
  return raw_request;
}

void FakeServiceEndpointResolver::OnShutdown() {}

std::unique_ptr<HostResolver::ResolveHostRequest>
FakeServiceEndpointResolver::CreateRequest(
    url::SchemeHostPort host,
    NetworkAnonymizationKey network_anonymization_key,
    NetLogWithSource net_log,
    std::optional<ResolveHostParameters> optional_parameters) {
  NOTREACHED();
}

std::unique_ptr<HostResolver::ResolveHostRequest>
FakeServiceEndpointResolver::CreateRequest(
    const HostPortPair& host,
    const NetworkAnonymizationKey& network_anonymization_key,
    const NetLogWithSource& net_log,
    const std::optional<ResolveHostParameters>& optional_parameters) {
  NOTREACHED();
}

std::unique_ptr<HostResolver::ServiceEndpointRequest>
FakeServiceEndpointResolver::CreateServiceEndpointRequest(
    Host host,
    NetworkAnonymizationKey network_anonymization_key,
    NetLogWithSource net_log,
    ResolveHostParameters parameters) {
  CHECK(!requests_.empty());
  std::unique_ptr<FakeServiceEndpointRequest> request =
      std::move(requests_.front());
  requests_.pop_front();
  request->set_priority(parameters.initial_priority);
  return request;
}

ServiceEndpointBuilder::ServiceEndpointBuilder() = default;

ServiceEndpointBuilder::~ServiceEndpointBuilder() = default;

ServiceEndpointBuilder& ServiceEndpointBuilder::add_v4(std::string_view addr,
                                                       uint16_t port) {
  endpoint_.ipv4_endpoints.emplace_back(MakeIPEndPoint(addr));
  return *this;
}

ServiceEndpointBuilder& ServiceEndpointBuilder::add_v6(std::string_view addr,
                                                       uint16_t port) {
  endpoint_.ipv6_endpoints.emplace_back(MakeIPEndPoint(addr));
  return *this;
}

ServiceEndpointBuilder& ServiceEndpointBuilder::add_ip_endpoint(
    IPEndPoint ip_endpoint) {
  if (ip_endpoint.address().IsIPv4()) {
    endpoint_.ipv4_endpoints.emplace_back(ip_endpoint);
  } else {
    CHECK(ip_endpoint.address().IsIPv6());
    endpoint_.ipv6_endpoints.emplace_back(ip_endpoint);
  }
  return *this;
}

ServiceEndpointBuilder& ServiceEndpointBuilder::set_alpns(
    std::vector<std::string> alpns) {
  endpoint_.metadata.supported_protocol_alpns = std::move(alpns);
  return *this;
}

// static
std::unique_ptr<FakeStreamSocket> FakeStreamSocket::CreateForSpdy() {
  auto stream = std::make_unique<FakeStreamSocket>();
  SSLInfo ssl_info;
  SSLConnectionStatusSetVersion(SSL_CONNECTION_VERSION_TLS1_3,
                                &ssl_info.connection_status);
  SSLConnectionStatusSetCipherSuite(0x1301 /* TLS_CHACHA20_POLY1305_SHA256 */,
                                    &ssl_info.connection_status);
  ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  stream->set_ssl_info(ssl_info);
  return stream;
}

FakeStreamSocket::FakeStreamSocket() : MockClientSocket(NetLogWithSource()) {
  connected_ = true;
}

FakeStreamSocket::~FakeStreamSocket() = default;

int FakeStreamSocket::Read(IOBuffer* buf,
                           int buf_len,
                           CompletionOnceCallback callback) {
  return ERR_IO_PENDING;
}

int FakeStreamSocket::Write(
    IOBuffer* buf,
    int buf_len,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  return ERR_IO_PENDING;
}

int FakeStreamSocket::Connect(CompletionOnceCallback callback) {
  return OK;
}

bool FakeStreamSocket::IsConnected() const {
  return connected_;
}

bool FakeStreamSocket::IsConnectedAndIdle() const {
  return connected_ && is_idle_;
}

bool FakeStreamSocket::WasEverUsed() const {
  return was_ever_used_;
}

bool FakeStreamSocket::GetSSLInfo(SSLInfo* ssl_info) {
  if (ssl_info_.has_value()) {
    *ssl_info = *ssl_info_;
    return true;
  }

  return false;
}

HttpStreamKey GroupIdToHttpStreamKey(
    const ClientSocketPool::GroupId& group_id) {
  return HttpStreamKey(group_id.destination(), group_id.privacy_mode(),
                       SocketTag(), group_id.network_anonymization_key(),
                       group_id.secure_dns_policy(),
                       group_id.disable_cert_network_fetches());
}

}  // namespace net
