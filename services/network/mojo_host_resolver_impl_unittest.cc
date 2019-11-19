// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/mojo_host_resolver_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/address_family.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using net::test::IsError;
using net::test::IsOk;

namespace network {

namespace {

class TestRequestClient
    : public proxy_resolver::mojom::HostResolverRequestClient {
 public:
  explicit TestRequestClient(
      mojo::PendingReceiver<proxy_resolver::mojom::HostResolverRequestClient>
          receiver)
      : done_(false), receiver_(this, std::move(receiver)) {
    receiver_.set_disconnect_handler(base::Bind(
        &TestRequestClient::OnMojoDisconnect, base::Unretained(this)));
  }

  void WaitForResult();
  void WaitForConnectionError();

  int32_t error_;
  std::vector<net::IPAddress> results_;

 private:
  // Overridden from proxy_resolver::mojom::HostResolverRequestClient.
  void ReportResult(int32_t error,
                    const std::vector<net::IPAddress>& results) override;

  // Mojo disconnect handler.
  void OnMojoDisconnect();

  bool done_;
  base::Closure run_loop_quit_closure_;
  base::Closure connection_error_quit_closure_;

  mojo::Receiver<proxy_resolver::mojom::HostResolverRequestClient> receiver_;
};

void TestRequestClient::WaitForResult() {
  if (done_)
    return;

  base::RunLoop run_loop;
  run_loop_quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
  ASSERT_TRUE(done_);
}

void TestRequestClient::WaitForConnectionError() {
  base::RunLoop run_loop;
  connection_error_quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

void TestRequestClient::ReportResult(
    int32_t error,
    const std::vector<net::IPAddress>& results) {
  if (!run_loop_quit_closure_.is_null()) {
    run_loop_quit_closure_.Run();
  }
  ASSERT_FALSE(done_);
  error_ = error;
  results_ = results;
  done_ = true;
}

void TestRequestClient::OnMojoDisconnect() {
  if (!connection_error_quit_closure_.is_null())
    connection_error_quit_closure_.Run();
}

}  // namespace

class MojoHostResolverImplTest : public testing::Test {
 protected:
  const net::IPAddress kExampleComAddress{1, 2, 3, 4};
  const net::IPAddress kExampleComAddressIpv6{1, 2,  3,  4,  5,  6,  7,  8,
                                              9, 10, 11, 12, 13, 14, 15, 16};
  const net::IPAddress kChromiumOrgAddress{8, 8, 8, 8};

  void SetUp() override {
    mock_host_resolver_.rules()->AddRuleForAddressFamily(
        "example.com", net::ADDRESS_FAMILY_IPV4, kExampleComAddress.ToString());
    mock_host_resolver_.rules()->AddRule("example.com",
                                         kExampleComAddressIpv6.ToString());
    mock_host_resolver_.rules()->AddRule("chromium.org",
                                         kChromiumOrgAddress.ToString());
    mock_host_resolver_.rules()->AddSimulatedFailure("failure.fail");

    resolver_service_.reset(new MojoHostResolverImpl(&mock_host_resolver_,
                                                     net::NetLogWithSource()));
  }

  // Wait until the mock resolver has received |num| resolve requests.
  void WaitForRequests(size_t num) {
    while (mock_host_resolver_.num_resolve() < num) {
      base::RunLoop run_loop;
      run_loop.RunUntilIdle();
    }
  }

  base::test::TaskEnvironment task_environment_;

  net::MockHostResolver mock_host_resolver_;
  std::unique_ptr<MojoHostResolverImpl> resolver_service_;
};

TEST_F(MojoHostResolverImplTest, Resolve) {
  mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient>
      client_remote;
  TestRequestClient client(client_remote.InitWithNewPipeAndPassReceiver());

  resolver_service_->Resolve("example.com", net::NetworkIsolationKey(),
                             false /* is_ex */, std::move(client_remote));
  client.WaitForResult();

  EXPECT_THAT(client.error_, IsOk());
  EXPECT_THAT(client.results_, testing::ElementsAre(kExampleComAddress));
}

TEST_F(MojoHostResolverImplTest, ResolveSynchronous) {
  mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient>
      client_remote;
  TestRequestClient client(client_remote.InitWithNewPipeAndPassReceiver());

  mock_host_resolver_.set_synchronous_mode(true);

  resolver_service_->Resolve("example.com", net::NetworkIsolationKey(),
                             false /* is_ex */, std::move(client_remote));
  client.WaitForResult();

  EXPECT_THAT(client.error_, IsOk());
  EXPECT_THAT(client.results_, testing::ElementsAre(kExampleComAddress));
}

TEST_F(MojoHostResolverImplTest, ResolveMultiple) {
  mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient>
      client1_remote;
  TestRequestClient client1(client1_remote.InitWithNewPipeAndPassReceiver());
  mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient>
      client2_remote;
  TestRequestClient client2(client2_remote.InitWithNewPipeAndPassReceiver());

  mock_host_resolver_.set_ondemand_mode(true);

  resolver_service_->Resolve("example.com", net::NetworkIsolationKey(),
                             false /* is_ex */, std::move(client1_remote));
  resolver_service_->Resolve("chromium.org", net::NetworkIsolationKey(),
                             false /* is_ex */, std::move(client2_remote));
  WaitForRequests(2);
  mock_host_resolver_.ResolveAllPending();

  client1.WaitForResult();
  client2.WaitForResult();

  EXPECT_THAT(client1.error_, IsOk());
  EXPECT_THAT(client1.results_, testing::ElementsAre(kExampleComAddress));
  EXPECT_THAT(client2.error_, IsOk());
  EXPECT_THAT(client2.results_, testing::ElementsAre(kChromiumOrgAddress));
}

TEST_F(MojoHostResolverImplTest, ResolveDuplicate) {
  mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient>
      client1_remote;
  TestRequestClient client1(client1_remote.InitWithNewPipeAndPassReceiver());
  mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient>
      client2_remote;
  TestRequestClient client2(client2_remote.InitWithNewPipeAndPassReceiver());

  mock_host_resolver_.set_ondemand_mode(true);

  resolver_service_->Resolve("example.com", net::NetworkIsolationKey(),
                             false /* is_ex */, std::move(client1_remote));
  resolver_service_->Resolve("example.com", net::NetworkIsolationKey(),
                             false /* is_ex */, std::move(client2_remote));
  WaitForRequests(2);
  mock_host_resolver_.ResolveAllPending();

  client1.WaitForResult();
  client2.WaitForResult();

  EXPECT_THAT(client1.error_, IsOk());
  EXPECT_THAT(client1.results_, testing::ElementsAre(kExampleComAddress));
  EXPECT_THAT(client2.error_, IsOk());
  EXPECT_THAT(client2.results_, testing::ElementsAre(kExampleComAddress));
}

TEST_F(MojoHostResolverImplTest, ResolveFailure) {
  mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient>
      client_remote;
  TestRequestClient client(client_remote.InitWithNewPipeAndPassReceiver());

  resolver_service_->Resolve("failure.fail", net::NetworkIsolationKey(),
                             false /* is_ex */, std::move(client_remote));
  client.WaitForResult();

  EXPECT_THAT(client.error_, IsError(net::ERR_NAME_NOT_RESOLVED));
  EXPECT_TRUE(client.results_.empty());
}

TEST_F(MojoHostResolverImplTest, ResolveEx) {
  mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient>
      client_remote;
  TestRequestClient client(client_remote.InitWithNewPipeAndPassReceiver());

  resolver_service_->Resolve("example.com", net::NetworkIsolationKey(),
                             true /* is_ex */, std::move(client_remote));
  client.WaitForResult();

  EXPECT_THAT(client.error_, IsOk());
  EXPECT_THAT(client.results_, testing::ElementsAre(kExampleComAddressIpv6));
}

// Makes sure that the passed in NetworkIsolationKey is passed to the
// HostResolver.
TEST_F(MojoHostResolverImplTest, NetworkIsolationKeyUsed) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://foo.test/"));
  const net::NetworkIsolationKey kNetworkIsolationKey(kOrigin, kOrigin);

  mock_host_resolver_.set_ondemand_mode(true);

  mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient>
      client_remote;
  TestRequestClient client(client_remote.InitWithNewPipeAndPassReceiver());

  resolver_service_->Resolve("example.com", kNetworkIsolationKey,
                             false /* is_ex */, std::move(client_remote));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, mock_host_resolver_.num_resolve());
  EXPECT_EQ(kNetworkIsolationKey,
            mock_host_resolver_.last_request_network_isolation_key());
}

TEST_F(MojoHostResolverImplTest, DestroyClient) {
  mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient>
      client_remote;
  std::unique_ptr<TestRequestClient> client(
      new TestRequestClient(client_remote.InitWithNewPipeAndPassReceiver()));

  mock_host_resolver_.set_ondemand_mode(true);

  resolver_service_->Resolve("example.com", net::NetworkIsolationKey(),
                             false /* is_ex */, std::move(client_remote));
  WaitForRequests(1);

  client.reset();
  base::RunLoop().RunUntilIdle();

  mock_host_resolver_.ResolveAllPending();
  base::RunLoop().RunUntilIdle();
}

}  // namespace network
