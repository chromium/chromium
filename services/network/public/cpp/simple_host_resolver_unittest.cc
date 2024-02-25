// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/simple_host_resolver.h"

#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/host_port_pair.h"
#include "net/dns/host_resolver.h"
#include "services/network/test/test_network_context_with_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

net::IPEndPoint CreateExpectedEndPoint(std::string_view address,
                                       uint16_t port) {
  net::IPAddress ip_address;
  CHECK(ip_address.AssignFromIPLiteral(address));
  return net::IPEndPoint(ip_address, port);
}

std::string CreateMappingRules(
    std::vector<std::pair<std::string_view, std::string_view>>
        host_ip_address_pairs) {
  std::vector<std::string> map_rules;
  for (auto [host, ip_address] : host_ip_address_pairs) {
    map_rules.push_back(
        base::StringPrintf("MAP %s %s", host.data(), ip_address.data()));
  }
  return base::JoinString(map_rules, ",");
}

class MockNetworkContext : public TestNetworkContextWithHostResolver {
 public:
  explicit MockNetworkContext(std::unique_ptr<net::HostResolver> host_resolver)
      : TestNetworkContextWithHostResolver(std::move(host_resolver)) {}

  static std::unique_ptr<MockNetworkContext> CreateNetworkContext(
      std::string_view host_mapping_rules) {
    return std::make_unique<MockNetworkContext>(
        net::HostResolver::CreateStandaloneResolver(
            net::NetLog::Get(), /*options=*/std::nullopt, host_mapping_rules,
            /*enable_caching=*/false));
  }

  // Resets ResolveHostClient when matching |host| is supplied in ResolveHost().
  void SetResetClientFor(std::optional<net::HostPortPair> reset_client_for) {
    reset_client_for_ = std::move(reset_client_for);
  }

  void ResolveHost(
      network::mojom::HostResolverHostPtr host,
      const ::net::NetworkAnonymizationKey& network_anonymization_key,
      network::mojom::ResolveHostParametersPtr optional_parameters,
      ::mojo::PendingRemote<network::mojom::ResolveHostClient>
          pending_response_client) override {
    DCHECK(host->is_host_port_pair());
    if (reset_client_for_ == host->get_host_port_pair()) {
      pending_response_client.reset();
      return;
    }
    ResolveHostImpl(std::move(host), network_anonymization_key,
                    std::move(optional_parameters),
                    std::move(pending_response_client));
  }

  std::optional<net::HostPortPair> reset_client_for_;
};

class SimpleHostResolverTest : public testing::Test {
 public:
  SimpleHostResolverTest() = default;

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::MainThreadType::IO};
};

struct HostResolverResult {
  int result;
  std::optional<net::IPEndPoint> resolved_address;
};

struct HostResolverRequest {
  net::HostPortPair host_port_pair;

  // Simulates mojo pipe break for a specific request.
  bool reset_client = false;
};

using ResolveHostFuture = base::test::TestFuture<
    int,
    const net::ResolveErrorInfo&,
    const std::optional<net::AddressList>&,
    const std::optional<net::HostResolverEndpointResults>&>;

TEST_F(SimpleHostResolverTest, ResolveFourAddresses) {
  auto network_context = MockNetworkContext::CreateNetworkContext(
      CreateMappingRules({{"example.test", "98.76.54.32"},
                          {"another-example.test", "11.22.33.44"}}));
  auto simple_resolver = SimpleHostResolver::Create(network_context.get());

  // Send four ResolveHost requests:
  //   * #1 passes
  //   * #2 gets ERR_NAME_NOT_RESOLVED
  //   * #3 passes
  //   * #4 receives a pipe break and responds with ERR_FAILED
  std::vector<std::pair<HostResolverRequest, HostResolverResult>> test_cases = {
      {{.host_port_pair = net::HostPortPair("example.test", 160)},
       {.result = net::OK,
        .resolved_address = CreateExpectedEndPoint("98.76.54.32", 160)}},
      {{.host_port_pair = net::HostPortPair("unknown", 15)},
       {.result = net::ERR_NAME_NOT_RESOLVED}},
      {{.host_port_pair = net::HostPortPair("another-example.test", 85)},
       {.result = net::OK,
        .resolved_address = CreateExpectedEndPoint("11.22.33.44", 85)}},
      {{.host_port_pair = net::HostPortPair("example.test", 3800),
        .reset_client = true},
       {.result = net::ERR_FAILED}}};

  EXPECT_EQ(simple_resolver->GetNumOutstandingRequestsForTesting(), 0U);

  std::vector<std::pair<std::unique_ptr<ResolveHostFuture>, HostResolverResult>>
      futures;
  for (auto [request, result] : test_cases) {
    auto future = std::make_unique<ResolveHostFuture>();
    if (request.reset_client) {
      network_context->SetResetClientFor(request.host_port_pair);
    }
    simple_resolver->ResolveHost(
        network::mojom::HostResolverHost::NewHostPortPair(
            request.host_port_pair),
        net::NetworkAnonymizationKey(),
        /*optional_parameters=*/nullptr, future->GetCallback());
    futures.emplace_back(std::move(future), std::move(result));
  }

  EXPECT_EQ(simple_resolver->GetNumOutstandingRequestsForTesting(), 4U);

  for (const auto& [future, resolver_result] : futures) {
    const auto& [result, resolve_error_info, resolved_addresses,
                 endpoint_results_with_metadata] = future->Get();
    EXPECT_EQ(result, resolver_result.result);
    if (!resolver_result.resolved_address) {
      EXPECT_FALSE(resolved_addresses);
    } else {
      EXPECT_THAT(resolved_addresses->endpoints(),
                  testing::ElementsAre(*resolver_result.resolved_address));
    }
  }

  // Verify that all receivers have been erased.
  EXPECT_EQ(simple_resolver->GetNumOutstandingRequestsForTesting(), 0U);
}

}  // namespace
}  // namespace network
