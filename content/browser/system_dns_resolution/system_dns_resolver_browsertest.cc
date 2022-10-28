// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>

#include "base/barrier_closure.h"
#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/network_service_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "net/base/address_list.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/network_interfaces.h"
#include "net/base/request_priority.h"
#include "net/dns/host_resolver_system_task.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/dns/public/secure_dns_policy.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resolve_host_client_base.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const int kHttpPort = 80;

const char kHostname1[] = "hostname1";
const char kIpAddress1[] = "127.0.0.2";
const char kHostname2[] = "hostname2";
const char kIpAddress2[] = "127.0.0.3";
const char kFailHostname[] = "failhostname";

}  // namespace

class MockResolveHostClient : public network::ResolveHostClientBase {
 public:
  MockResolveHostClient(
      mojo::PendingReceiver<network::mojom::ResolveHostClient> receiver,
      base::OnceClosure callback)
      : receiver_(this, std::move(receiver)), callback_(std::move(callback)) {
    receiver_.set_disconnect_handler(
        base::BindOnce(&MockResolveHostClient::ResolveHostClientDisconnected,
                       base::Unretained(this)));
  }

  // network::mojom::ResolveHostClient:
  void OnComplete(int result,
                  const net::ResolveErrorInfo& resolve_error_info,
                  const absl::optional<net::AddressList>& resolved_addresses,
                  const absl::optional<net::HostResolverEndpointResults>&
                      endpoint_results_with_metadata) override {
    result_ = result;
    resolve_error_info_ = resolve_error_info;
    resolved_addresses_ = resolved_addresses;
    endpoint_results_with_metadata_ = endpoint_results_with_metadata;
    std::move(callback_).Run();
  }

  void ResolveHostClientDisconnected() {
    if (callback_) {
      ADD_FAILURE() << "Unexpected disconnection of ResolveHostClient";
    }
  }

  int result() { return result_; }
  const net::ResolveErrorInfo resolve_error_info() {
    return resolve_error_info_;
  }
  const absl::optional<net::AddressList>& resolved_addresses() const {
    return resolved_addresses_;
  }

 private:
  int result_;
  net::ResolveErrorInfo resolve_error_info_;
  absl::optional<net::AddressList> resolved_addresses_;
  absl::optional<net::HostResolverEndpointResults>
      endpoint_results_with_metadata_;

  mojo::Receiver<network::mojom::ResolveHostClient> receiver_;
  base::OnceClosure callback_;
};

class SystemDnsResolverBrowserTest : public content::ContentBrowserTest {
 public:
  SystemDnsResolverBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        network::features::kOutOfProcessSystemDnsResolution);
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule(kHostname1, kIpAddress1);
    host_resolver()->AddRule(kHostname2, kIpAddress2);
    host_resolver()->AddSimulatedFailure(kFailHostname, 0);
    host_resolver()->AddRule(net::GetHostName(), "127.0.0.1");

    shell()
        ->web_contents()
        ->GetBrowserContext()
        ->GetDefaultStoragePartition()
        ->GetNetworkContext()
        ->CreateHostResolver(absl::nullopt,
                             host_resolver_.BindNewPipeAndPassReceiver());
  }

  MockResolveHostClient* ResolveHostname(std::string hostname,
                                         base::OnceClosure cb) {
    network::mojom::ResolveHostParametersPtr parameters =
        network::mojom::ResolveHostParameters::New();
    parameters->initial_priority = net::RequestPriority::HIGHEST;
    // Use the SYSTEM resolver, and don't allow the cache or attempt DoH.
    parameters->source = net::HostResolverSource::SYSTEM;
    parameters->cache_usage =
        network::mojom::ResolveHostParameters::CacheUsage::DISALLOWED;
    parameters->secure_dns_policy = network::mojom::SecureDnsPolicy::DISABLE;

    mojo::PendingReceiver<network::mojom::ResolveHostClient> receiver;
    host_resolver_->ResolveHost(
        network::mojom::HostResolverHost::NewHostPortPair(
            net::HostPortPair(hostname, kHttpPort)),
        net::NetworkAnonymizationKey::CreateTransient(), std::move(parameters),
        receiver.InitWithNewPipeAndPassRemote());
    return &client_list_.emplace_back(std::move(receiver), std::move(cb));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  std::list<MockResolveHostClient> client_list_;

  mojo::Remote<network::mojom::HostResolver> host_resolver_;
};

IN_PROC_BROWSER_TEST_F(SystemDnsResolverBrowserTest,
                       NetworkServiceResolvesOneHostname) {
  base::RunLoop run_loop;
  MockResolveHostClient* client1 =
      ResolveHostname(kHostname1, run_loop.QuitClosure());
  run_loop.Run();

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // If system DNS resolution runs in the browser process, check here that the
  // resolver received the correct number of resolves.
  EXPECT_EQ(host_resolver()->NumResolvesForHostPattern(kHostname1), 1u);
#endif

  const absl::optional<net::AddressList>& addr_list1 =
      client1->resolved_addresses();
  ASSERT_TRUE(addr_list1);
  net::IPAddress address1;
  EXPECT_TRUE(address1.AssignFromIPLiteral(kIpAddress1));
  EXPECT_EQ((*addr_list1)[0],
            net::IPEndPoint(net::IPAddress(address1), kHttpPort));
}

IN_PROC_BROWSER_TEST_F(SystemDnsResolverBrowserTest,
                       NetworkServiceResolvesTwoHostnames) {
  base::RunLoop run_loop;
  base::RepeatingClosure barrier =
      base::BarrierClosure(2, run_loop.QuitClosure());
  MockResolveHostClient* client1 = ResolveHostname(kHostname1, barrier);
  MockResolveHostClient* client2 = ResolveHostname(kHostname2, barrier);
  run_loop.Run();

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // If system DNS resolution runs in the browser process, check here that the
  // resolver received the correct number of resolves.
  EXPECT_EQ(host_resolver()->NumResolvesForHostPattern(kHostname1), 1u);
  EXPECT_EQ(host_resolver()->NumResolvesForHostPattern(kHostname2), 1u);
#endif

  const absl::optional<net::AddressList>& addr_list1 =
      client1->resolved_addresses();
  ASSERT_TRUE(addr_list1);
  net::IPAddress address1;
  EXPECT_TRUE(address1.AssignFromIPLiteral(kIpAddress1));
  EXPECT_EQ((*addr_list1)[0],
            net::IPEndPoint(net::IPAddress(address1), kHttpPort));

  const absl::optional<net::AddressList>& addr_list2 =
      client2->resolved_addresses();
  ASSERT_TRUE(addr_list2);
  net::IPAddress address2;
  EXPECT_TRUE(address2.AssignFromIPLiteral(kIpAddress2));
  EXPECT_EQ((*addr_list2)[0],
            net::IPEndPoint(net::IPAddress(address2), kHttpPort));
}

IN_PROC_BROWSER_TEST_F(SystemDnsResolverBrowserTest,
                       NetworkServiceFailsResolvingBadHostname) {
  base::RunLoop run_loop;
  MockResolveHostClient* client =
      ResolveHostname(kFailHostname, run_loop.QuitClosure());
  run_loop.Run();

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // If system DNS resolution runs in the browser process, check here that the
  // resolver received the correct number of resolves.
  EXPECT_EQ(host_resolver()->NumResolvesForHostPattern(kFailHostname), 1u);
#endif

  const net::ResolveErrorInfo& resolve_error_info =
      client->resolve_error_info();
  EXPECT_EQ(resolve_error_info.error, net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(client->result(), net::ERR_NAME_NOT_RESOLVED);
}

// Check if the system's own host name resolves, which is a slightly different
// code path from normal resolution.
IN_PROC_BROWSER_TEST_F(SystemDnsResolverBrowserTest,
                       NetworkServiceResolvesOwnHostname) {
  base::RunLoop run_loop;
  net::AddressList addr_list;
  int os_error, net_error;
  auto cb = base::BindLambdaForTesting(
      [&](const net::AddressList& addr_list_result, int os_error_result,
          int net_error_result) {
        addr_list = addr_list_result;
        os_error = os_error_result;
        net_error = net_error_result;
        run_loop.Quit();
      });

  // Systems with an in-process network service (e.g. some Android) will not
  // have a network_service_test().
  std::unique_ptr<net::HostResolverSystemTask> system_task;
  if (IsInProcessNetworkService()) {
    system_task = net::HostResolverSystemTask::CreateForOwnHostname(
        net::AddressFamily::ADDRESS_FAMILY_UNSPECIFIED, 0);
    system_task->Start(std::move(cb));
  } else {
    network_service_test()->ResolveOwnHostnameWithSystemDns(std::move(cb));
  }

  run_loop.Run();

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // If system DNS resolution runs in the browser process, check here that the
  // resolver received the correct number of resolves.
  EXPECT_EQ(host_resolver()->NumResolvesForHostPattern(net::GetHostName()), 1u);
#endif

  ASSERT_EQ(addr_list.size(), 1u);
  net::IPAddress address;
  EXPECT_TRUE(address.AssignFromIPLiteral("127.0.0.1"));
  EXPECT_EQ(addr_list[0].address(), address);
}

}  // namespace content
