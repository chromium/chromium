// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/target_network_test_util.h"

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
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TargetNetworkCheckingSocketFactory::TargetNetworkCheckingSocketFactory(
    std::vector<handles::NetworkHandle> expected_networks)
    : expected_networks_(std::move(expected_networks)) {}

TargetNetworkCheckingSocketFactory::~TargetNetworkCheckingSocketFactory() {
  EXPECT_EQ(current_socket_request_, expected_networks_.size());
}

std::unique_ptr<TransportClientSocket>
TargetNetworkCheckingSocketFactory::CreateTransportClientSocket(
    const AddressList& addresses,
    handles::NetworkHandle target_network,
    std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
    NetworkQualityEstimator* network_quality_estimator,
    NetLog* net_log,
    const NetLogSource& source) {
  if (current_socket_request_ >= expected_networks_.size()) {
    ADD_FAILURE() << "Unexpected CreateTransportClientSocket call";
    return nullptr;
  }
  EXPECT_EQ(target_network, expected_networks_[current_socket_request_]);
  tcp_socket_count_++;
  current_socket_request_++;
  return MockClientSocketFactory::CreateTransportClientSocket(
      addresses, target_network, std::move(socket_performance_watcher),
      network_quality_estimator, net_log, source);
}

std::unique_ptr<DatagramClientSocket>
TargetNetworkCheckingSocketFactory::CreateDatagramClientSocket(
    DatagramSocket::BindType bind_type,
    handles::NetworkHandle target_network,
    NetLog* net_log,
    const NetLogSource& source) {
  if (current_socket_request_ >= expected_networks_.size()) {
    ADD_FAILURE() << "Unexpected CreateDatagramClientSocket call";
    return nullptr;
  }
  EXPECT_EQ(target_network, expected_networks_[current_socket_request_]);
  udp_socket_count_++;
  current_socket_request_++;
  return MockClientSocketFactory::CreateDatagramClientSocket(
      bind_type, target_network, net_log, source);
}

TargetNetworkCheckingHostResolver::TargetNetworkCheckingHostResolver(
    std::vector<handles::NetworkHandle> expected_networks)
    : MockHostResolver(
          MockHostResolverBase::RuleResolver::GetLocalhostResult()),
      expected_networks_(std::move(expected_networks)) {}

TargetNetworkCheckingHostResolver::~TargetNetworkCheckingHostResolver() {
  EXPECT_EQ(current_dns_request_, expected_networks_.size());
}

std::unique_ptr<HostResolver::ResolveHostRequest>
TargetNetworkCheckingHostResolver::CreateRequest(
    url::SchemeHostPort host,
    NetworkAnonymizationKey network_anonymization_key,
    handles::NetworkHandle target_network,
    NetLogWithSource net_log,
    std::optional<ResolveHostParameters> optional_parameters) {
  if (current_dns_request_ >= expected_networks_.size()) {
    ADD_FAILURE() << "Unexpected CreateRequest call";
    return nullptr;
  }
  EXPECT_EQ(target_network, expected_networks_[current_dns_request_]);
  current_dns_request_++;
  return MockHostResolver::CreateRequest(
      std::move(host), std::move(network_anonymization_key), target_network,
      std::move(net_log), std::move(optional_parameters));
}

std::unique_ptr<HostResolver::ResolveHostRequest>
TargetNetworkCheckingHostResolver::CreateRequest(
    const HostPortPair& host,
    const NetworkAnonymizationKey& network_anonymization_key,
    handles::NetworkHandle target_network,
    const NetLogWithSource& net_log,
    const std::optional<ResolveHostParameters>& optional_parameters) {
  if (current_dns_request_ >= expected_networks_.size()) {
    ADD_FAILURE() << "Unexpected CreateRequest call";
    return nullptr;
  }
  EXPECT_EQ(target_network, expected_networks_[current_dns_request_]);
  current_dns_request_++;
  return MockHostResolver::CreateRequest(host, network_anonymization_key,
                                         target_network, net_log,
                                         optional_parameters);
}

std::unique_ptr<HostResolver::ServiceEndpointRequest>
TargetNetworkCheckingHostResolver::CreateServiceEndpointRequest(
    Host host,
    NetworkAnonymizationKey network_anonymization_key,
    handles::NetworkHandle target_network,
    NetLogWithSource net_log,
    ResolveHostParameters parameters) {
  if (current_dns_request_ >= expected_networks_.size()) {
    ADD_FAILURE() << "Unexpected CreateServiceEndpointRequest call";
    return nullptr;
  }
  EXPECT_EQ(target_network, expected_networks_[current_dns_request_]);
  current_dns_request_++;
  return MockHostResolver::CreateServiceEndpointRequest(
      std::move(host), std::move(network_anonymization_key), target_network,
      std::move(net_log), std::move(parameters));
}

TargetNetworkCheckingProxyResolutionService::
    TargetNetworkCheckingProxyResolutionService(
        std::unique_ptr<ProxyResolutionService> delegate,
        std::vector<handles::NetworkHandle> expected_networks)
    : delegate_(std::move(delegate)),
      expected_networks_(std::move(expected_networks)) {}

TargetNetworkCheckingProxyResolutionService::
    ~TargetNetworkCheckingProxyResolutionService() {
  EXPECT_EQ(current_proxy_request_, expected_networks_.size());
}

int TargetNetworkCheckingProxyResolutionService::ResolveProxy(
    const GURL& url,
    const std::string& method,
    const NetworkAnonymizationKey& network_anonymization_key,
    handles::NetworkHandle target_network,
    ProxyInfo* results,
    CompletionOnceCallback callback,
    std::unique_ptr<ProxyResolutionRequest>* request,
    const NetLogWithSource& net_log,
    RequestPriority priority) {
  if (current_proxy_request_ >= expected_networks_.size()) {
    ADD_FAILURE() << "Unexpected ResolveProxy call";
    return ERR_UNEXPECTED;
  }
  EXPECT_EQ(target_network, expected_networks_[current_proxy_request_]);
  current_proxy_request_++;
  return delegate_->ResolveProxy(url, method, network_anonymization_key,
                                 target_network, results, std::move(callback),
                                 request, net_log, priority);
}

void TargetNetworkCheckingProxyResolutionService::ReportSuccess(
    const ProxyInfo& proxy_info) {
  delegate_->ReportSuccess(proxy_info);
}

void TargetNetworkCheckingProxyResolutionService::SetProxyDelegate(
    ProxyDelegate* delegate) {
  delegate_->SetProxyDelegate(delegate);
}

void TargetNetworkCheckingProxyResolutionService::OnShutdown() {
  delegate_->OnShutdown();
}

void TargetNetworkCheckingProxyResolutionService::ClearBadProxiesCache() {
  delegate_->ClearBadProxiesCache();
}

const ProxyRetryInfoMap&
TargetNetworkCheckingProxyResolutionService::proxy_retry_info() const {
  return delegate_->proxy_retry_info();
}

base::DictValue
TargetNetworkCheckingProxyResolutionService::GetProxyNetLogValues() {
  return delegate_->GetProxyNetLogValues();
}

bool TargetNetworkCheckingProxyResolutionService::
    CastToConfiguredProxyResolutionService(
        ConfiguredProxyResolutionService**
            configured_proxy_resolution_service) {
  *configured_proxy_resolution_service = nullptr;
  return false;
}

}  // namespace net
