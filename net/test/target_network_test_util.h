// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_TARGET_NETWORK_TEST_UTIL_H_
#define NET_TEST_TARGET_NETWORK_TEST_UTIL_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "net/base/net_errors.h"
#include "net/base/network_handle.h"
#include "net/dns/mock_host_resolver.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/socket/socket_test_util.h"

namespace net {

// Validates that each socket creation call received specifies the expected
// target network from a pre-configured sequence. In its destructor, it also
// verifies that all expected socket creation calls were made.
class TargetNetworkCheckingSocketFactory : public MockClientSocketFactory {
 public:
  explicit TargetNetworkCheckingSocketFactory(
      std::vector<handles::NetworkHandle> expected_networks);
  ~TargetNetworkCheckingSocketFactory() override;

  std::unique_ptr<TransportClientSocket> CreateTransportClientSocket(
      const AddressList& addresses,
      handles::NetworkHandle target_network,
      std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
      NetworkQualityEstimator* network_quality_estimator,
      NetLog* net_log,
      const NetLogSource& source) override;

  std::unique_ptr<DatagramClientSocket> CreateDatagramClientSocket(
      DatagramSocket::BindType bind_type,
      handles::NetworkHandle target_network,
      NetLog* net_log,
      const NetLogSource& source) override;

  size_t tcp_socket_count() const { return tcp_socket_count_; }
  size_t udp_socket_count() const { return udp_socket_count_; }

 private:
  std::vector<handles::NetworkHandle> expected_networks_;
  size_t current_socket_request_ = 0;
  size_t tcp_socket_count_ = 0;
  size_t udp_socket_count_ = 0;
};

// Validates that each host resolution request received specifies the expected
// target network from a pre-configured sequence. In its destructor, it also
// verifies that all expected DNS requests were made.
class TargetNetworkCheckingHostResolver : public MockHostResolver {
 public:
  explicit TargetNetworkCheckingHostResolver(
      std::vector<handles::NetworkHandle> expected_networks);
  ~TargetNetworkCheckingHostResolver() override;

  std::unique_ptr<HostResolver::ResolveHostRequest> CreateRequest(
      url::SchemeHostPort host,
      NetworkAnonymizationKey network_anonymization_key,
      handles::NetworkHandle target_network,
      NetLogWithSource net_log,
      std::optional<ResolveHostParameters> optional_parameters) override;

  std::unique_ptr<HostResolver::ResolveHostRequest> CreateRequest(
      const HostPortPair& host,
      const NetworkAnonymizationKey& network_anonymization_key,
      handles::NetworkHandle target_network,
      const NetLogWithSource& net_log,
      const std::optional<ResolveHostParameters>& optional_parameters) override;

  std::unique_ptr<HostResolver::ServiceEndpointRequest>
  CreateServiceEndpointRequest(
      Host host,
      NetworkAnonymizationKey network_anonymization_key,
      handles::NetworkHandle target_network,
      NetLogWithSource net_log,
      ResolveHostParameters parameters) override;

  size_t current_dns_request() const { return current_dns_request_; }

 private:
  std::vector<handles::NetworkHandle> expected_networks_;
  size_t current_dns_request_ = 0;
};

// Validates that each proxy resolution call received specifies the expected
// target network from a pre-configured sequence before forwarding to an
// underlying delegate. In its destructor, it also verifies that all expected
// proxy resolution requests were made.
class TargetNetworkCheckingProxyResolutionService
    : public ProxyResolutionService {
 public:
  TargetNetworkCheckingProxyResolutionService(
      std::unique_ptr<ProxyResolutionService> delegate,
      std::vector<handles::NetworkHandle> expected_networks);
  ~TargetNetworkCheckingProxyResolutionService() override;

  int ResolveProxy(const GURL& url,
                   const std::string& method,
                   const NetworkAnonymizationKey& network_anonymization_key,
                   handles::NetworkHandle target_network,
                   ProxyInfo* results,
                   CompletionOnceCallback callback,
                   std::unique_ptr<ProxyResolutionRequest>* request,
                   const NetLogWithSource& net_log,
                   RequestPriority priority) override;

  void ReportSuccess(const ProxyInfo& proxy_info) override;
  void SetProxyDelegate(ProxyDelegate* delegate) override;
  void OnShutdown() override;
  void ClearBadProxiesCache() override;
  const ProxyRetryInfoMap& proxy_retry_info() const override;
  base::DictValue GetProxyNetLogValues() override;
  bool CastToConfiguredProxyResolutionService(
      ConfiguredProxyResolutionService** configured_proxy_resolution_service)
      override;

  size_t current_proxy_request() const { return current_proxy_request_; }

 private:
  std::unique_ptr<ProxyResolutionService> delegate_;
  std::vector<handles::NetworkHandle> expected_networks_;
  size_t current_proxy_request_ = 0;
};

}  // namespace net

#endif  // NET_TEST_TARGET_NETWORK_TEST_UTIL_H_
