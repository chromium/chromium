// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_POOL_TEST_UTIL_H_
#define NET_HTTP_HTTP_STREAM_POOL_TEST_UTIL_H_

#include <list>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/test/test_future.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_stream_key.h"
#include "net/http/http_stream_pool.h"
#include "net/http/http_stream_pool_job.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/stream_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/scheme_host_port.h"

namespace net {

class IOBuffer;
class SSLInfo;

// A fake ServiceEndpointRequest implementation that provides testing harnesses.
// See the comment of HostResolver::ServiceEndpointRequest for details.
class FakeServiceEndpointRequest : public HostResolver::ServiceEndpointRequest {
 public:
  FakeServiceEndpointRequest();
  ~FakeServiceEndpointRequest() override;

  // Sets the current endpoints to `endpoints`. Previous endpoints are
  // discarded.
  FakeServiceEndpointRequest& set_endpoints(
      std::vector<ServiceEndpoint> endpoints) {
    endpoints_ = std::move(endpoints);
    return *this;
  }

  // Add `endpoint` to the current endpoints.
  FakeServiceEndpointRequest& add_endpoint(ServiceEndpoint endpoint) {
    endpoints_.emplace_back(std::move(endpoint));
    return *this;
  }

  // Sets the return value of GetDnsAliasResults().
  FakeServiceEndpointRequest& set_aliases(std::set<std::string> aliases) {
    aliases_ = std::move(aliases);
    return *this;
  }

  // Sets the return value of EndpointsCryptoReady().
  FakeServiceEndpointRequest& set_crypto_ready(bool endpoints_crypto_ready) {
    endpoints_crypto_ready_ = endpoints_crypto_ready;
    return *this;
  }

  // Sets the return value of GetResolveErrorInfo().
  FakeServiceEndpointRequest& set_resolve_error_info(
      ResolveErrorInfo resolve_error_info) {
    resolve_error_info_ = resolve_error_info;
    return *this;
  }

  RequestPriority priority() const { return priority_; }
  FakeServiceEndpointRequest& set_priority(RequestPriority priority) {
    priority_ = priority;
    return *this;
  }

  // Make `this` complete synchronously when ServiceEndpointRequest::Start()
  // is called.
  FakeServiceEndpointRequest& CompleteStartSynchronously(int rv);

  // Calls `delegate_->OnServiceEndpointsUpdated()`. Must not be used after
  // calling CompleteStartSynchronously() or
  // CallOnServiceEndpointRequestFinished()
  FakeServiceEndpointRequest& CallOnServiceEndpointsUpdated();

  // Calls `delegate_->OnServiceEndpointRequestFinished()`. Mut not be used
  // after calling CompleteStartSynchronously().
  FakeServiceEndpointRequest& CallOnServiceEndpointRequestFinished(int rv);

  // HostResolver::ServiceEndpointRequest methods:
  int Start(Delegate* delegate) override;
  const std::vector<ServiceEndpoint>& GetEndpointResults() override;
  const std::set<std::string>& GetDnsAliasResults() override;
  bool EndpointsCryptoReady() override;
  ResolveErrorInfo GetResolveErrorInfo() override;
  const HostCache::EntryStaleness* GetStaleInfo() const override;
  bool IsStaleWhileRefresing() const override;
  void ChangeRequestPriority(RequestPriority priority) override;

 private:
  raw_ptr<Delegate> delegate_;

  int start_result_ = ERR_IO_PENDING;
  std::vector<ServiceEndpoint> endpoints_;
  std::set<std::string> aliases_;
  bool endpoints_crypto_ready_ = false;
  ResolveErrorInfo resolve_error_info_;
  RequestPriority priority_ = RequestPriority::IDLE;
};

// A fake HostResolver that implements the ServiceEndpointRequest API using
// FakeServiceEndpointRequest.
class FakeServiceEndpointResolver : public HostResolver {
 public:
  FakeServiceEndpointResolver();

  FakeServiceEndpointResolver(const FakeServiceEndpointResolver&) = delete;
  FakeServiceEndpointResolver& operator=(const FakeServiceEndpointResolver&) =
      delete;

  ~FakeServiceEndpointResolver() override;

  // Create a FakeServiceEndpointRequest that will be used for the next
  // CreateServiceEndpointRequest() call. Note that
  // CreateServiceEndpointRequest() consumes the request. You will need to call
  // this method multiple times when you expect multiple
  // CreateServiceEndpointRequest() calls.
  FakeServiceEndpointRequest* AddFakeRequest();

  // HostResolver methods:
  void OnShutdown() override;
  std::unique_ptr<ResolveHostRequest> CreateRequest(
      url::SchemeHostPort host,
      NetworkAnonymizationKey network_anonymization_key,
      NetLogWithSource net_log,
      std::optional<ResolveHostParameters> optional_parameters) override;
  std::unique_ptr<ResolveHostRequest> CreateRequest(
      const HostPortPair& host,
      const NetworkAnonymizationKey& network_anonymization_key,
      const NetLogWithSource& net_log,
      const std::optional<ResolveHostParameters>& optional_parameters) override;
  std::unique_ptr<ServiceEndpointRequest> CreateServiceEndpointRequest(
      Host host,
      NetworkAnonymizationKey network_anonymization_key,
      NetLogWithSource net_log,
      ResolveHostParameters parameters) override;

 private:
  std::list<std::unique_ptr<FakeServiceEndpointRequest>> requests_;
};

// A helper to build a ServiceEndpoint.
class ServiceEndpointBuilder {
 public:
  ServiceEndpointBuilder();
  ~ServiceEndpointBuilder();

  ServiceEndpointBuilder& add_v4(std::string_view addr, uint16_t port = 80);

  ServiceEndpointBuilder& add_v6(std::string_view addr, uint16_t port = 80);

  ServiceEndpointBuilder& add_ip_endpoint(IPEndPoint ip_endpoint);

  ServiceEndpointBuilder& set_alpns(std::vector<std::string> alpns);

  ServiceEndpointBuilder& set_ech_config_list(
      std::vector<uint8_t> ech_config_list);

  ServiceEndpoint endpoint() const { return endpoint_; }

 private:
  ServiceEndpoint endpoint_;
};

class FakeStreamSocket : public MockClientSocket {
 public:
  static std::unique_ptr<FakeStreamSocket> CreateForSpdy();

  FakeStreamSocket();

  FakeStreamSocket(const FakeStreamSocket&) = delete;
  FakeStreamSocket& operator=(const FakeStreamSocket&) = delete;

  ~FakeStreamSocket() override;

  void set_is_connected(bool connected) { connected_ = connected; }

  void set_is_idle(bool is_idle) { is_idle_ = is_idle; }

  void set_was_ever_used(bool was_ever_used) { was_ever_used_ = was_ever_used; }

  void set_peer_addr(IPEndPoint peer_addr) { peer_addr_ = peer_addr; }

  void set_ssl_info(SSLInfo ssl_info) { ssl_info_ = std::move(ssl_info); }

  // StreamSocket implementation:
  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override;
  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation) override;
  int Connect(CompletionOnceCallback callback) override;
  bool IsConnected() const override;
  bool IsConnectedAndIdle() const override;
  bool WasEverUsed() const override;
  bool GetSSLInfo(SSLInfo* ssl_info) override;

 private:
  bool is_idle_ = true;
  bool was_ever_used_ = false;
  std::optional<SSLInfo> ssl_info_;
};

// A helper to create an HttpStreamKey.
class StreamKeyBuilder {
 public:
  explicit StreamKeyBuilder(std::string_view destination = "http://a.test")
      : destination_(url::SchemeHostPort(GURL(destination))) {}

  StreamKeyBuilder(const StreamKeyBuilder&) = delete;
  StreamKeyBuilder& operator=(const StreamKeyBuilder&) = delete;

  ~StreamKeyBuilder() = default;

  StreamKeyBuilder& from_key(const HttpStreamKey& key);

  const url::SchemeHostPort& destination() const { return destination_; }

  StreamKeyBuilder& set_destination(std::string_view destination) {
    set_destination(url::SchemeHostPort(GURL(destination)));
    return *this;
  }

  StreamKeyBuilder& set_destination(url::SchemeHostPort destination) {
    destination_ = std::move(destination);
    return *this;
  }

  StreamKeyBuilder& set_privacy_mode(PrivacyMode privacy_mode) {
    privacy_mode_ = privacy_mode;
    return *this;
  }

  HttpStreamKey Build() const;

 private:
  url::SchemeHostPort destination_;
  PrivacyMode privacy_mode_ = PRIVACY_MODE_DISABLED;
  SecureDnsPolicy secure_dns_policy_ = SecureDnsPolicy::kAllow;
  bool disable_cert_network_fetches_ = true;
};

// An HttpStreamPool::Job::Delegate implementation for tests.
class TestJobDelegate : public HttpStreamPool::Job::Delegate {
 public:
  static inline constexpr std::string_view kDefaultDestination =
      "https://www.example.org";

  explicit TestJobDelegate(
      std::optional<HttpStreamKey> stream_key = std::nullopt);

  TestJobDelegate(const TestJobDelegate&) = delete;
  TestJobDelegate& operator=(const TestJobDelegate&) = delete;

  ~TestJobDelegate() override;

  TestJobDelegate& set_expected_protocol(NextProto expected_protocol) {
    expected_protocol_ = expected_protocol;
    return *this;
  }

  TestJobDelegate& set_quic_version(quic::ParsedQuicVersion quic_version) {
    quic_version_ = quic_version;
    return *this;
  }

  void CreateAndStartJob(HttpStreamPool& pool);

  void CancelJob() { job_.reset(); }

  int GetResult() { return result_future_.Get(); }

  // HttpStreamPool::Job::Delegate implementations:
  void OnStreamReady(HttpStreamPool::Job* job,
                     std::unique_ptr<HttpStream> stream,
                     NextProto negotiated_protocol) override;
  RequestPriority priority() const override;
  HttpStreamPool::RespectLimits respect_limits() const override;
  const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs()
      const override;
  bool enable_ip_based_pooling() const override;
  bool enable_alternative_services() const override;
  bool is_http1_allowed() const override;
  const ProxyInfo& proxy_info() const override;
  const NetLogWithSource& net_log() const override;
  void OnStreamFailed(HttpStreamPool::Job* job,
                      int status,
                      const NetErrorDetails& net_error_details,
                      ResolveErrorInfo resolve_error_info) override;
  void OnCertificateError(HttpStreamPool::Job* job,
                          int status,
                          const SSLInfo& ssl_info) override;
  void OnNeedsClientAuth(HttpStreamPool::Job* job,
                         SSLCertRequestInfo* cert_info) override;
  void OnPreconnectComplete(HttpStreamPool::Job* job, int status) override;

  HttpStreamKey GetStreamKey() const { return key_builder_.Build(); }

  NextProto negotiated_protocol() const { return negotiated_protocol_; }

 private:
  void SetResult(int result) { result_future_.SetValue(result); }

  StreamKeyBuilder key_builder_;

  NextProto expected_protocol_ = NextProto::kProtoUnknown;
  quic::ParsedQuicVersion quic_version_ =
      quic::ParsedQuicVersion::Unsupported();
  std::vector<SSLConfig::CertAndStatus> allowed_bad_certs_;
  ProxyInfo proxy_info_ = ProxyInfo::Direct();
  NetLogWithSource net_log_;

  std::unique_ptr<HttpStreamPool::Job> job_;

  base::test::TestFuture<int> result_future_;
  NextProto negotiated_protocol_ = NextProto::kProtoUnknown;
};

// Convert a ClientSocketPool::GroupId to an HttpStreamKey.
HttpStreamKey GroupIdToHttpStreamKey(const ClientSocketPool::GroupId& group_id);

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_POOL_TEST_UTIL_H_
