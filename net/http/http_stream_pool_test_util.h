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

#include "base/memory/weak_ptr.h"
#include "base/test/test_future.h"
#include "net/base/completion_once_callback.h"
#include "net/base/load_timing_internal_info.h"
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

// Provides fake service endpoint resolution results for testing.
class FakeServiceEndpointResolution {
 public:
  FakeServiceEndpointResolution();
  ~FakeServiceEndpointResolution();

  FakeServiceEndpointResolution(const FakeServiceEndpointResolution&);
  FakeServiceEndpointResolution& operator=(
      const FakeServiceEndpointResolution&);

  int start_result() const { return start_result_; }
  const std::vector<ServiceEndpoint>& endpoints() const { return endpoints_; }
  const std::set<std::string>& aliases() const { return aliases_; }
  bool endpoints_crypto_ready() const { return endpoints_crypto_ready_; }
  ResolveErrorInfo resolve_error_info() const { return resolve_error_info_; }
  RequestPriority priority() const { return priority_; }

  // These setters return `this&` to allow chaining.
  FakeServiceEndpointResolution& CompleteStartSynchronously(int rv);
  FakeServiceEndpointResolution& set_start_result(int start_result);
  FakeServiceEndpointResolution& set_endpoints(
      std::vector<ServiceEndpoint> endpoints);
  FakeServiceEndpointResolution& add_endpoint(ServiceEndpoint endpoint);
  FakeServiceEndpointResolution& set_aliases(std::set<std::string> aliases);
  FakeServiceEndpointResolution& set_crypto_ready(bool endpoints_crypto_ready);
  FakeServiceEndpointResolution& set_resolve_error_info(
      ResolveErrorInfo resolve_error_info);
  FakeServiceEndpointResolution& set_priority(RequestPriority priority);

 private:
  int start_result_ = ERR_IO_PENDING;
  std::vector<ServiceEndpoint> endpoints_;
  std::set<std::string> aliases_;
  bool endpoints_crypto_ready_ = false;
  ResolveErrorInfo resolve_error_info_;
  RequestPriority priority_ = RequestPriority::IDLE;
};

// A fake ServiceEndpointRequest implementation that provides testing
// harnesses. See the comment of HostResolver::ServiceEndpointRequest for
// details.
class FakeServiceEndpointRequest : public HostResolver::ServiceEndpointRequest {
 public:
  FakeServiceEndpointRequest();
  ~FakeServiceEndpointRequest() override;

  // Following setter methods return `this&` to allow chaining.
  FakeServiceEndpointRequest& set_endpoints(
      std::vector<ServiceEndpoint> endpoints);
  FakeServiceEndpointRequest& add_endpoint(ServiceEndpoint endpoint);
  FakeServiceEndpointRequest& set_aliases(std::set<std::string> aliases);
  FakeServiceEndpointRequest& set_crypto_ready(bool endpoints_crypto_ready);
  FakeServiceEndpointRequest& set_resolve_error_info(
      ResolveErrorInfo resolve_error_info);
  FakeServiceEndpointRequest& set_priority(RequestPriority priority);

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

  RequestPriority priority() const { return resolution_.priority(); }

  // HostResolver::ServiceEndpointRequest methods:
  int Start(Delegate* delegate) override;
  base::span<const ServiceEndpoint> GetEndpointResults() override;
  const std::set<std::string>& GetDnsAliasResults() override;
  bool EndpointsCryptoReady() override;
  ResolveErrorInfo GetResolveErrorInfo() override;
  const HostCache::EntryStaleness* GetStaleInfo() const override;
  bool IsStaleWhileRefresing() const override;
  void ChangeRequestPriority(RequestPriority priority) override;

 private:
  friend class FakeServiceEndpointResolver;

  raw_ptr<Delegate> delegate_;

  FakeServiceEndpointResolution resolution_;

  base::WeakPtrFactory<FakeServiceEndpointRequest> weak_ptr_factory_{this};
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

  // Creates a FakeServiceEndpointRequest that will be used for the next
  // CreateServiceEndpointRequest() call. CreateServiceEndpointRequest()
  // consumes the request. If you expect multiple CreateServiceEndpointRequest()
  // calls, you need to do either:
  // - Call this method as many times as you expect
  //   CreateServiceEndpointRequest()
  // - Configure the default resolution result using
  //   ConfigureDefaultResolution().
  base::WeakPtr<FakeServiceEndpointRequest> AddFakeRequest();

  // Configures the default resolution result. It will be used when there are
  // no requests in the request queue. Overrides the previous default result if
  // existed.
  FakeServiceEndpointResolution& ConfigureDefaultResolution();

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
  bool IsHappyEyeballsV3Enabled() const override;

 private:
  std::list<std::unique_ptr<FakeServiceEndpointRequest>> requests_;
  std::optional<FakeServiceEndpointResolution> default_resolution_;
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

  ServiceEndpointBuilder& set_trust_anchor_ids(
      std::vector<std::vector<uint8_t>> trust_anchor_ids);

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

  // Simulates a situation where a connected socket disconnects after
  // IsConnected() is called `count` times. Such situation could happen in the
  // real world.
  void DisconnectAfterIsConnectedCall(int count = 1);

 private:
  bool is_idle_ = true;
  bool was_ever_used_ = false;
  // When set to a positive value, every IsConnected() call decrements this
  // counter. After this counter reached zero, IsConnected() uses
  // `is_connected_override_`.
  mutable int disconnect_after_is_connected_call_count_ = -1;
  mutable std::optional<bool> is_connected_override_;
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
                     NextProto negotiated_protocol,
                     std::optional<SessionSource> session_source) override;
  RequestPriority priority() const override;
  HttpStreamPool::RespectLimits respect_limits() const override;
  const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs()
      const override;
  bool enable_ip_based_pooling_for_h2() const override;
  bool enable_alternative_services() const override;
  NextProtoSet allowed_alpns() const override;
  const ProxyInfo& proxy_info() const override;
  const NetLogWithSource& net_log() const override;
  const perfetto::Flow& flow() const override;
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
  perfetto::Flow flow_;

  std::unique_ptr<HttpStreamPool::Job> job_;

  base::test::TestFuture<int> result_future_;
  NextProto negotiated_protocol_ = NextProto::kProtoUnknown;
};

// Convert a ClientSocketPool::GroupId to an HttpStreamKey.
HttpStreamKey GroupIdToHttpStreamKey(const ClientSocketPool::GroupId& group_id);

// Wait for the `attempt_manager`'s completion.
void WaitForAttemptManagerComplete(
    HttpStreamPool::AttemptManager* attempt_manager);

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_POOL_TEST_UTIL_H_
