// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/delegation/dns_request.h"

#include "base/functional/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "services/network/test/test_network_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content::webid {

namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::WithArgs;

class MockHostResolver : public network::mojom::HostResolver {
 public:
  MOCK_METHOD(
      void,
      ResolveHost,
      (network::mojom::HostResolverHostPtr host,
       const net::NetworkAnonymizationKey& network_anonymization_key,
       network::mojom::ResolveHostParametersPtr optional_parameters,
       mojo::PendingRemote<network::mojom::ResolveHostClient> response_client),
      (override));
  MOCK_METHOD(
      void,
      MdnsListen,
      (const net::HostPortPair& host,
       net::DnsQueryType query_type,
       mojo::PendingRemote<network::mojom::MdnsListenClient> response_client,
       MdnsListenCallback callback),
      (override));
};

class MockNetworkContext : public network::TestNetworkContext {
 public:
  MOCK_METHOD(void,
              CreateHostResolver,
              (const std::optional<net::DnsConfigOverrides>& config_overrides,
               mojo::PendingReceiver<network::mojom::HostResolver> receiver),
              (override));
};

}  // namespace

class DnsRequestTest : public testing::Test {
 public:
  DnsRequestTest() = default;

 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(DnsRequestTest, Success) {
  MockHostResolver mock_host_resolver;
  MockNetworkContext mock_network_context;
  mojo::Receiver<network::mojom::HostResolver> receiver(&mock_host_resolver);
  EXPECT_CALL(mock_network_context, CreateHostResolver(_, _))
      .WillOnce([&](const std::optional<net::DnsConfigOverrides>&,
                    mojo::PendingReceiver<network::mojom::HostResolver>
                        pending_receiver) {
        receiver.Bind(std::move(pending_receiver));
      });

  EXPECT_CALL(mock_host_resolver, ResolveHost(_, _, _, _))
      .WillOnce(
          WithArgs<3>([](mojo::PendingRemote<network::mojom::ResolveHostClient>
                             response_client) {
            mojo::Remote<network::mojom::ResolveHostClient> client(
                std::move(response_client));
            client->OnTextResults({"iss=record1"});
            client->OnComplete(net::OK, net::ResolveErrorInfo(net::OK),
                               net::AddressList(), {});
          }));

  DnsRequest dns_request(base::BindRepeating(
      [](network::mojom::NetworkContext* network_context) {
        return network_context;
      },
      &mock_network_context));

  base::RunLoop run_loop;
  base::MockCallback<DnsRequest::DnsRequestCallback> callback;
  EXPECT_CALL(callback,
              Run(testing::Optional(std::vector<std::string>{"iss=record1"})))
      .WillOnce([&]() { run_loop.Quit(); });

  dns_request.SendRequest("hostname", callback.Get());
  run_loop.Run();
}

TEST_F(DnsRequestTest, NetError) {
  MockHostResolver mock_host_resolver;
  MockNetworkContext mock_network_context;
  mojo::Receiver<network::mojom::HostResolver> receiver(&mock_host_resolver);
  EXPECT_CALL(mock_network_context, CreateHostResolver(_, _))
      .WillOnce([&](const std::optional<net::DnsConfigOverrides>&,
                    mojo::PendingReceiver<network::mojom::HostResolver>
                        pending_receiver) {
        receiver.Bind(std::move(pending_receiver));
      });

  EXPECT_CALL(mock_host_resolver, ResolveHost(_, _, _, _))
      .WillOnce(
          WithArgs<3>([](mojo::PendingRemote<network::mojom::ResolveHostClient>
                             response_client) {
            mojo::Remote<network::mojom::ResolveHostClient> client(
                std::move(response_client));
            client->OnComplete(
                net::ERR_NAME_NOT_RESOLVED,
                net::ResolveErrorInfo(net::ERR_NAME_NOT_RESOLVED),
                net::AddressList(), {});
          }));

  DnsRequest dns_request(base::BindRepeating(
      [](network::mojom::NetworkContext* network_context) {
        return network_context;
      },
      &mock_network_context));

  base::RunLoop run_loop;
  base::MockCallback<DnsRequest::DnsRequestCallback> callback;
  EXPECT_CALL(callback, Run(testing::Eq(std::nullopt))).WillOnce([&]() {
    run_loop.Quit();
  });

  dns_request.SendRequest("hostname", callback.Get());
  run_loop.Run();
}

TEST_F(DnsRequestTest, NetworkContextGetterReturnsNull) {
  DnsRequest dns_request(base::BindRepeating(
      []() -> network::mojom::NetworkContext* { return nullptr; }));

  base::RunLoop run_loop;
  base::MockCallback<DnsRequest::DnsRequestCallback> callback;
  EXPECT_CALL(callback, Run(testing::Eq(std::nullopt))).WillOnce([&]() {
    run_loop.Quit();
  });

  dns_request.SendRequest("hostname", callback.Get());
  run_loop.Run();
}

TEST_F(DnsRequestTest, MultipleTxtRecords) {
  MockHostResolver mock_host_resolver;
  MockNetworkContext mock_network_context;
  mojo::Receiver<network::mojom::HostResolver> receiver(&mock_host_resolver);
  EXPECT_CALL(mock_network_context, CreateHostResolver(_, _))
      .WillOnce([&](const std::optional<net::DnsConfigOverrides>&,
                    mojo::PendingReceiver<network::mojom::HostResolver>
                        pending_receiver) {
        receiver.Bind(std::move(pending_receiver));
      });

  EXPECT_CALL(mock_host_resolver, ResolveHost(_, _, _, _))
      .WillOnce(
          WithArgs<3>([](mojo::PendingRemote<network::mojom::ResolveHostClient>
                             response_client) {
            mojo::Remote<network::mojom::ResolveHostClient> client(
                std::move(response_client));
            client->OnTextResults({"iss=hello.coop", "iss=foo.com"});
            client->OnComplete(net::OK, net::ResolveErrorInfo(net::OK),
                               net::AddressList(), {});
          }));

  DnsRequest dns_request(base::BindRepeating(
      [](network::mojom::NetworkContext* network_context) {
        return network_context;
      },
      &mock_network_context));

  base::RunLoop run_loop;
  base::MockCallback<DnsRequest::DnsRequestCallback> callback;
  EXPECT_CALL(callback, Run(testing::Optional(std::vector<std::string>{
                            "iss=hello.coop", "iss=foo.com"})))
      .WillOnce([&]() { run_loop.Quit(); });

  dns_request.SendRequest("hostname", callback.Get());
  run_loop.Run();
}

}  // namespace content::webid
