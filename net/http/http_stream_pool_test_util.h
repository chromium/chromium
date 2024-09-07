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

#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_stream_key.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/stream_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/scheme_host_port.h"

namespace net {

class IOBuffer;
class SSLInfo;

// A fake ServiceEndpointRequest implementation that provides testing harnesses.
class FakeServiceEndpointRequest : public HostResolver::ServiceEndpointRequest {
 public:
  FakeServiceEndpointRequest();
  ~FakeServiceEndpointRequest() override;

  void set_start_result(int start_result) { start_result_ = start_result; }

  void set_endpoints(std::vector<ServiceEndpoint> endpoints) {
    endpoints_ = std::move(endpoints);
  }

  FakeServiceEndpointRequest& add_endpoint(ServiceEndpoint endpoint) {
    endpoints_.emplace_back(std::move(endpoint));
    return *this;
  }

  FakeServiceEndpointRequest& set_aliases(std::set<std::string> aliases) {
    aliases_ = std::move(aliases);
    return *this;
  }

  FakeServiceEndpointRequest& set_crypto_ready(bool endpoints_crypto_ready) {
    endpoints_crypto_ready_ = endpoints_crypto_ready;
    return *this;
  }

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

  FakeServiceEndpointRequest& CompleteStartSynchronously(int rv);

  FakeServiceEndpointRequest& CallOnServiceEndpointsUpdated();

  FakeServiceEndpointRequest& CallOnServiceEndpointRequestFinished(int rv);

  // HostResolver::ServiceEndpointRequest methods:
  int Start(Delegate* delegate) override;
  const std::vector<ServiceEndpoint>& GetEndpointResults() override;
  const std::set<std::string>& GetDnsAliasResults() override;
  bool EndpointsCryptoReady() override;
  ResolveErrorInfo GetResolveErrorInfo() override;
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

// Convert a ClientSocketPool::GroupId to an HttpStreamKey.
HttpStreamKey GroupIdToHttpStreamKey(const ClientSocketPool::GroupId& group_id);

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_POOL_TEST_UTIL_H_
