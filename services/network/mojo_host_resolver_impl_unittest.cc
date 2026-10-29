// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/mojo_host_resolver_impl.h"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/address_family.h"
#include "net/base/address_list.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
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
      : receiver_(this, std::move(receiver)) {
    receiver_.set_disconnect_handler(base::BindOnce(
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

  bool done_ = false;
  bool had_connection_error_ = false;
  base::OnceClosure run_loop_quit_closure_;
  base::OnceClosure connection_error_quit_closure_;

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
  if (had_connection_error_) {
    return;
  }
  base::RunLoop run_loop;
  connection_error_quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

void TestRequestClient::ReportResult(
    int32_t error,
    const std::vector<net::IPAddress>& results) {
  if (!run_loop_quit_closure_.is_null()) {
    std::move(run_loop_quit_closure_).Run();
  }
  ASSERT_FALSE(done_);
  error_ = error;
  results_ = results;
  done_ = true;
}

void TestRequestClient::OnMojoDisconnect() {
  had_connection_error_ = true;
  if (!connection_error_quit_closure_.is_null())
    std::move(connection_error_quit_closure_).Run();
}

// MockHostResolver that can run a callback when a request is destroyed, before
// the underlying request is destroyed. Used to simulate destroying one request
// synchronously completing another, which the network stack can do (e.g., when
// tearing down a shared DNS-over-HTTPS session).
class DestructionHookHostResolver : public net::MockHostResolver {
 public:
  DestructionHookHostResolver() = default;
  ~DestructionHookHostResolver() override = default;

  // `hook` will be invoked when the next request for `host` is destroyed, just
  // before the underlying request is destroyed.
  void set_destruction_hook(const std::string& host, base::OnceClosure hook) {
    destruction_hook_host_ = host;
    destruction_hook_ = std::move(hook);
  }

  // net::MockHostResolver implementation:
  using net::MockHostResolver::CreateRequest;
  std::unique_ptr<ResolveHostRequest> CreateRequest(
      const net::HostPortPair& host,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      net::handles::NetworkHandle target_network,
      const net::NetLogWithSource& net_log,
      const std::optional<ResolveHostParameters>& optional_parameters)
      override {
    return std::make_unique<RequestWrapper>(
        this, host.host(),
        net::MockHostResolver::CreateRequest(host, network_anonymization_key,
                                             target_network, net_log,
                                             optional_parameters));
  }

 private:
  class RequestWrapper : public ResolveHostRequest {
   public:
    RequestWrapper(DestructionHookHostResolver* resolver,
                   const std::string& host,
                   std::unique_ptr<ResolveHostRequest> request)
        : resolver_(resolver), host_(host), request_(std::move(request)) {}

    ~RequestWrapper() override {
      if (resolver_->destruction_hook_ &&
          resolver_->destruction_hook_host_ == host_) {
        std::move(resolver_->destruction_hook_).Run();
      }
    }

    int Start(net::CompletionOnceCallback callback) override {
      return request_->Start(std::move(callback));
    }
    const net::AddressList& GetAddressResults() const override {
      return request_->GetAddressResults();
    }
    base::span<const net::HostResolverEndpointResult> GetEndpointResults()
        const override {
      return request_->GetEndpointResults();
    }
    base::span<const std::string> GetTextResults() const override {
      return request_->GetTextResults();
    }
    base::span<const net::HostPortPair> GetHostnameResults() const override {
      return request_->GetHostnameResults();
    }
    const std::set<std::string>& GetDnsAliasResults() const override {
      return request_->GetDnsAliasResults();
    }
    net::ResolveErrorInfo GetResolveErrorInfo() const override {
      return request_->GetResolveErrorInfo();
    }
    const std::optional<net::HostCache::EntryStaleness>& GetStaleInfo()
        const override {
      return request_->GetStaleInfo();
    }
    std::optional<net::ResolutionDetails> GetResolutionDetails()
        const override {
      return request_->GetResolutionDetails();
    }

   private:
    const raw_ptr<DestructionHookHostResolver> resolver_;
    const std::string host_;
    const std::unique_ptr<ResolveHostRequest> request_;
  };

  std::string destruction_hook_host_;
  base::OnceClosure destruction_hook_;
};

}  // namespace

class MojoHostResolverImplTest : public testing::Test {
 protected:
  const net::IPAddress kExampleComAddress{1, 2, 3, 4};
  const net::IPAddress kExampleComAddressIpv6{1, 2,  3,  4,  5,  6,  7,  8,
                                              9, 10, 11, 12, 13, 14, 15, 16};
  const net::IPAddress kChromiumOrgAddress{8, 8, 8, 8};

  void SetUp() override {
    net::HostResolverEndpointResults example_com_endpoints(1);
    example_com_endpoints[0].ip_endpoints = {
        net::IPEndPoint(kExampleComAddress, 0),
        net::IPEndPoint(kExampleComAddressIpv6, 0)};
    mock_host_resolver_.rules()->AddRule(
        "example.com", net::MockHostResolverBase::RuleResolver::RuleResult(
                           std::move(example_com_endpoints)));
    mock_host_resolver_.rules()->AddRule("chromium.org",
                                         kChromiumOrgAddress.ToString());
    mock_host_resolver_.rules()->AddRule("failure.fail",
                                         net::ERR_NAME_NOT_RESOLVED);

    resolver_service_ = std::make_unique<MojoHostResolverImpl>(
        &mock_host_resolver_, net::NetLogWithSource());
  }

  // Wait until the mock resolver has received |num| resolve requests.
  void WaitForRequests(size_t num) {
    while (mock_host_resolver_.num_resolve() < num) {
      base::RunLoop run_loop;
      run_loop.RunUntilIdle();
    }
  }

  base::test::TaskEnvironment task_environment_;

  DestructionHookHostResolver mock_host_resolver_;
  std::unique_ptr<MojoHostResolverImpl> resolver_service_;
};

TEST_F(MojoHostResolverImplTest, Resolve) {
  mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient>
      client_remote;
  TestRequestClient client(client_remote.InitWithNewPipeAndPassReceiver());

  resolver_service_->Resolve("example.com", net::NetworkAnonymizationKey(),
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

  resolver_service_->Resolve("example.com", net::NetworkAnonymizationKey(),
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

  resolver_service_->Resolve("example.com", net::NetworkAnonymizationKey(),
                             false /* is_ex */, std::move(client1_remote));
  resolver_service_->Resolve("chromium.org", net::NetworkAnonymizationKey(),
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

  resolver_service_->Resolve("example.com", net::NetworkAnonymizationKey(),
                             false /* is_ex */, std::move(client1_remote));
  resolver_service_->Resolve("example.com", net::NetworkAnonymizationKey(),
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

  resolver_service_->Resolve("failure.fail", net::NetworkAnonymizationKey(),
                             false /* is_ex */, std::move(client_remote));
  client.WaitForResult();

  EXPECT_THAT(client.error_, IsError(net::ERR_NAME_NOT_RESOLVED));
  EXPECT_TRUE(client.results_.empty());
}

TEST_F(MojoHostResolverImplTest, ResolveEx) {
  mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient>
      client_remote;
  TestRequestClient client(client_remote.InitWithNewPipeAndPassReceiver());

  resolver_service_->Resolve("example.com", net::NetworkAnonymizationKey(),
                             true /* is_ex */, std::move(client_remote));
  client.WaitForResult();

  EXPECT_THAT(client.error_, IsOk());
  EXPECT_THAT(client.results_, testing::UnorderedElementsAre(
                                   kExampleComAddress, kExampleComAddressIpv6));
}

// Makes sure that the passed in NetworkAnonymizationKey is passed to the
// HostResolver.
TEST_F(MojoHostResolverImplTest, NetworkAnonymizationKeyUsed) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://foo.test/"));
  const auto kNetworkAnonymizationKey =
      net::NetworkAnonymizationKey::CreateSameSite(net::SchemefulSite(kOrigin));

  mock_host_resolver_.set_ondemand_mode(true);

  mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient>
      client_remote;
  TestRequestClient client(client_remote.InitWithNewPipeAndPassReceiver());

  resolver_service_->Resolve("example.com", kNetworkAnonymizationKey,
                             false /* is_ex */, std::move(client_remote));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, mock_host_resolver_.num_resolve());
  EXPECT_EQ(kNetworkAnonymizationKey,
            mock_host_resolver_.last_request_network_anonymization_key());
}

TEST_F(MojoHostResolverImplTest, DestroyClient) {
  mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient>
      client_remote;
  std::unique_ptr<TestRequestClient> client(
      new TestRequestClient(client_remote.InitWithNewPipeAndPassReceiver()));

  mock_host_resolver_.set_ondemand_mode(true);

  resolver_service_->Resolve("example.com", net::NetworkAnonymizationKey(),
                             false /* is_ex */, std::move(client_remote));
  WaitForRequests(1);

  client.reset();
  base::RunLoop().RunUntilIdle();

  mock_host_resolver_.ResolveAllPending();
  base::RunLoop().RunUntilIdle();
}

// Regression test for a use-after-free when destroying the
// MojoHostResolverImpl. Destroying the MojoHostResolverImpl destroys its
// pending Jobs, which cancels their HostResolver requests. In the real network
// stack, cancelling one request can synchronously complete another (e.g., when
// they share a DNS-over-HTTPS session), which results in the completed Job
// deleting itself while the MojoHostResolverImpl is in the middle of deleting
// Jobs.
TEST_F(MojoHostResolverImplTest,
       DestroyServiceWithJobDestructionCompletingOtherJob) {
  mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient>
      client1_remote;
  TestRequestClient client1(client1_remote.InitWithNewPipeAndPassReceiver());
  mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient>
      client2_remote;
  TestRequestClient client2(client2_remote.InitWithNewPipeAndPassReceiver());

  // Use on-demand mode, so requests don't complete until ResolveNow() is
  // called.
  mock_host_resolver_.set_ondemand_mode(true);

  // Start two requests.
  resolver_service_->Resolve("example.com", net::NetworkAnonymizationKey(),
                             /*is_ex=*/false, std::move(client1_remote));
  WaitForRequests(1);
  const size_t request1_id = mock_host_resolver_.last_id();
  resolver_service_->Resolve("chromium.org", net::NetworkAnonymizationKey(),
                             /*is_ex=*/false, std::move(client2_remote));
  WaitForRequests(2);

  // Simulate the network stack behavior described above: when the request for
  // "chromium.org" is cancelled, synchronously complete the "example.com"
  // request. The mock resolver completes it successfully, using the rule set up
  // in SetUp().
  //
  // MojoHostResolverImpl adds new Jobs to the front of its list of pending
  // Jobs, and destroys Jobs from the front of that list, so the "chromium.org"
  // Job is destroyed first.
  mock_host_resolver_.set_destruction_hook(
      "chromium.org",
      base::BindOnce(&net::MockHostResolver::ResolveNow,
                     base::Unretained(&mock_host_resolver_), request1_id));
  resolver_service_.reset();

  // The "example.com" request should have been completed successfully during
  // destruction, rather than cancelled, due to the ResolveNow() call bound
  // above. The destruction order is important for test validity, but the real
  // point of the test is that there's no crash due to a UAF around destruction
  // of the list.
  client1.WaitForResult();
  EXPECT_THAT(client1.error_, IsOk());
  EXPECT_THAT(client1.results_, testing::ElementsAre(kExampleComAddress));

  // The "chromium.org" request was cancelled, so its pipe should have been
  // closed without a result.
  client2.WaitForConnectionError();
}

// Similar to the above test, but the "chromium.org" Job is destroyed because
// its client disconnected, rather than because the MojoHostResolverImpl was
// destroyed. Cancelling its request synchronously completes the "example.com"
// request, which results in the "example.com" Job being deleted while the
// "chromium.org" Job is being deleted.
TEST_F(MojoHostResolverImplTest,
       DestroyClientWithJobDestructionCompletingOtherJob) {
  mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient>
      client1_remote;
  TestRequestClient client1(client1_remote.InitWithNewPipeAndPassReceiver());
  mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient>
      client2_remote;
  auto client2 = std::make_unique<TestRequestClient>(
      client2_remote.InitWithNewPipeAndPassReceiver());

  // Use on-demand mode, so requests don't complete until ResolveNow() is
  // called.
  mock_host_resolver_.set_ondemand_mode(true);

  // Start two requests.
  resolver_service_->Resolve("example.com", net::NetworkAnonymizationKey(),
                             /*is_ex=*/false, std::move(client1_remote));
  WaitForRequests(1);
  const size_t request1_id = mock_host_resolver_.last_id();
  resolver_service_->Resolve("chromium.org", net::NetworkAnonymizationKey(),
                             /*is_ex=*/false, std::move(client2_remote));
  WaitForRequests(2);

  // When the request for "chromium.org" is cancelled, synchronously complete
  // the "example.com" request. The mock resolver completes it successfully,
  // using the rule set up in SetUp().
  mock_host_resolver_.set_destruction_hook(
      "chromium.org",
      base::BindOnce(&net::MockHostResolver::ResolveNow,
                     base::Unretained(&mock_host_resolver_), request1_id));

  // Destroying the "chromium.org" client closes its pipe. When the
  // MojoHostResolverImpl sees the pipe close, it destroys the corresponding
  // Job, cancelling its request.
  client2.reset();

  client1.WaitForResult();
  EXPECT_THAT(client1.error_, IsOk());
  EXPECT_THAT(client1.results_, testing::ElementsAre(kExampleComAddress));
  EXPECT_FALSE(resolver_service_->request_in_progress());
}

}  // namespace network
