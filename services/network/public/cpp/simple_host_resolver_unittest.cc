// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/simple_host_resolver.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/host_port_pair.h"
#include "net/dns/host_resolver.h"
#include "net/dns/mock_host_resolver.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

net::IPEndPoint CreateExpectedEndPoint(base::StringPiece address,
                                       uint16_t port) {
  net::IPAddress ip_address;
  CHECK(ip_address.AssignFromIPLiteral(address));
  return net::IPEndPoint(ip_address, port);
}

std::string CreateMappingRules(
    std::vector<std::pair<base::StringPiece, base::StringPiece>>
        host_ip_address_pairs) {
  std::vector<std::string> map_rules;
  for (auto [host, ip_address] : host_ip_address_pairs) {
    map_rules.push_back(
        base::StringPrintf("MAP %s %s", host.data(), ip_address.data()));
  }
  return base::JoinString(map_rules, ",");
}

class MockHostResolver : public network::mojom::HostResolver {
 public:
  explicit MockHostResolver(
      mojo::PendingReceiver<network::mojom::HostResolver> resolver_receiver,
      std::unique_ptr<net::HostResolver> internal_resolver)
      : receiver_(this), internal_resolver_(std::move(internal_resolver)) {
    receiver_.Bind(std::move(resolver_receiver));
  }

  static std::unique_ptr<MockHostResolver> CreateHostResolver(
      base::StringPiece host_mapping_rules,
      mojo::PendingReceiver<network::mojom::HostResolver> receiver) {
    return std::make_unique<MockHostResolver>(
        std::move(receiver),
        net::HostResolver::CreateStandaloneResolver(
            net::NetLog::Get(), /*options=*/absl::nullopt, host_mapping_rules,
            /*enable_caching=*/false));
  }

  // Resets ResolveHostClient when matching |host| is supplied in ResolveHost().
  void SetResetClientFor(absl::optional<net::HostPortPair> reset_client_for) {
    reset_client_for_ = std::move(reset_client_for);
  }

  void ResolveHost(
      network::mojom::HostResolverHostPtr host,
      const ::net::NetworkAnonymizationKey& network_anonymization_key,
      network::mojom::ResolveHostParametersPtr optional_parameters,
      ::mojo::PendingRemote<network::mojom::ResolveHostClient>
          pending_response_client) override {
    DCHECK(host->is_host_port_pair());

    mojo::Remote<network::mojom::ResolveHostClient> response_client;
    response_client.Bind(std::move(pending_response_client));

    if (reset_client_for_ == host->get_host_port_pair()) {
      response_client.reset();
      return;
    }

    auto internal_request = internal_resolver_->CreateRequest(
        host->get_host_port_pair(), network_anonymization_key,
        net::NetLogWithSource::Make(net::NetLog::Get(),
                                    net::NetLogSourceType::NONE),
        absl::nullopt);

    auto* ptr = internal_request.get();
    auto [async_callback, sync_calback] =
        base::SplitOnceCallback(base::BindOnce(
            &MockHostResolver::OnComplete, base::Unretained(this),
            std::move(response_client), std::move(internal_request)));

    // See ResolveHostRequest::Start() for an explanation why only one callback
    // will be invoked.
    int rv = ptr->Start(std::move(async_callback));
    if (rv != net::ERR_IO_PENDING) {
      std::move(sync_calback).Run(rv);
    }
  }

  void MdnsListen(
      const ::net::HostPortPair& host,
      ::net::DnsQueryType query_type,
      ::mojo::PendingRemote<network::mojom::MdnsListenClient> response_client,
      MdnsListenCallback callback) override {
    NOTREACHED();
  }

 protected:
  void OnComplete(
      mojo::Remote<network::mojom::ResolveHostClient> response_client,
      std::unique_ptr<net::HostResolver::ResolveHostRequest> internal_request,
      int error) {
    response_client->OnComplete(
        error, internal_request->GetResolveErrorInfo(),
        base::OptionalFromPtr(internal_request->GetAddressResults()),
        /*endpoint_results_with_metadata=*/absl::nullopt);
    response_client.reset();
  }

  absl::optional<net::HostPortPair> reset_client_for_;

  mojo::Receiver<network::mojom::HostResolver> receiver_;
  std::unique_ptr<net::HostResolver> internal_resolver_;
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
  absl::optional<net::IPEndPoint> resolved_address;
};

struct HostResolverRequest {
  net::HostPortPair host_port_pair;

  // Simulates mojo pipe break for a specific request.
  bool reset_client = false;
};

using ResolveHostFuture = base::test::TestFuture<
    int,
    const net::ResolveErrorInfo&,
    const absl::optional<net::AddressList>&,
    const absl::optional<net::HostResolverEndpointResults>&>;

TEST_F(SimpleHostResolverTest, ResolveFourAddresses) {
  mojo::PendingReceiver<network::mojom::HostResolver> resolver_receiver;
  mojo::PendingRemote<network::mojom::HostResolver> resolver_remote =
      resolver_receiver.InitWithNewPipeAndPassRemote();

  auto mock_resolver = MockHostResolver::CreateHostResolver(
      CreateMappingRules({{"example.test", "98.76.54.32"},
                          {"another-example.test", "11.22.33.44"}}),
      std::move(resolver_receiver));
  auto simple_resolver =
      SimpleHostResolver::CreateForTesting(std::move(resolver_remote));

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
      mock_resolver->SetResetClientFor(request.host_port_pair);
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
