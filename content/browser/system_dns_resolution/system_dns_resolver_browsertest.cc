// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>

#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
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
#include "services/network/public/cpp/simple_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const int kHttpPort = 80;

const char kHostname1[] = "hostname1";
const char kIpAddress1[] = "127.0.0.2";
const char kHostname2[] = "hostname2";
const char kIpAddress2[] = "127.0.0.3";
const char kFailHostname[] = "failhostname";

using ResolveHostFuture = base::test::TestFuture<
    int,
    const net::ResolveErrorInfo&,
    const absl::optional<net::AddressList>&,
    const absl::optional<net::HostResolverEndpointResults>&>;

}  // namespace

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

    resolver_ =
        network::SimpleHostResolver::Create(shell()
                                                ->web_contents()
                                                ->GetBrowserContext()
                                                ->GetDefaultStoragePartition()
                                                ->GetNetworkContext());
  }

  void TearDownOnMainThread() override {
    // Has to be torn down before its network context (which is a raw_ptr).
    resolver_.reset();

    ContentBrowserTest::TearDownOnMainThread();
  }

  void ResolveHostname(
      std::string hostname,
      network::SimpleHostResolver::ResolveHostCallback callback) {
    network::mojom::ResolveHostParametersPtr parameters =
        network::mojom::ResolveHostParameters::New();
    parameters->initial_priority = net::RequestPriority::HIGHEST;
    // Use the SYSTEM resolver, and don't allow the cache or attempt DoH.
    parameters->source = net::HostResolverSource::SYSTEM;
    parameters->cache_usage =
        network::mojom::ResolveHostParameters::CacheUsage::DISALLOWED;
    parameters->secure_dns_policy = network::mojom::SecureDnsPolicy::DISABLE;

    mojo::PendingReceiver<network::mojom::ResolveHostClient> receiver;
    resolver_->ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                               net::HostPortPair(hostname, kHttpPort)),
                           net::NetworkAnonymizationKey::CreateTransient(),
                           std::move(parameters), std::move(callback));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<network::SimpleHostResolver> resolver_;
};

IN_PROC_BROWSER_TEST_F(SystemDnsResolverBrowserTest,
                       NetworkServiceResolvesOneHostname) {
  ResolveHostFuture future;
  ResolveHostname(kHostname1, future.GetCallback());

  const auto& addr_list1 = future.Get<absl::optional<net::AddressList>>();

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
  // If system DNS resolution runs in the browser process, check here that the
  // resolver received the correct number of resolves.
  EXPECT_EQ(host_resolver()->NumResolvesForHostPattern(kHostname1), 1u);
#endif

  ASSERT_TRUE(addr_list1);
  net::IPAddress address1;
  EXPECT_TRUE(address1.AssignFromIPLiteral(kIpAddress1));
  EXPECT_EQ(addr_list1->front(),
            net::IPEndPoint(net::IPAddress(address1), kHttpPort));
}

IN_PROC_BROWSER_TEST_F(SystemDnsResolverBrowserTest,
                       NetworkServiceResolvesTwoHostnames) {
  ResolveHostFuture future1;
  ResolveHostname(kHostname1, future1.GetCallback());
  ResolveHostFuture future2;
  ResolveHostname(kHostname2, future2.GetCallback());

  const auto& addr_list1 = future1.Get<absl::optional<net::AddressList>>();
  const auto& addr_list2 = future2.Get<absl::optional<net::AddressList>>();

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
  // If system DNS resolution runs in the browser process, check here that the
  // resolver received the correct number of resolves.
  EXPECT_EQ(host_resolver()->NumResolvesForHostPattern(kHostname1), 1u);
  EXPECT_EQ(host_resolver()->NumResolvesForHostPattern(kHostname2), 1u);
#endif

  ASSERT_TRUE(addr_list1);
  net::IPAddress address1;
  EXPECT_TRUE(address1.AssignFromIPLiteral(kIpAddress1));
  EXPECT_EQ((*addr_list1)[0],
            net::IPEndPoint(net::IPAddress(address1), kHttpPort));

  ASSERT_TRUE(addr_list2);
  net::IPAddress address2;
  EXPECT_TRUE(address2.AssignFromIPLiteral(kIpAddress2));
  EXPECT_EQ((*addr_list2)[0],
            net::IPEndPoint(net::IPAddress(address2), kHttpPort));
}

IN_PROC_BROWSER_TEST_F(SystemDnsResolverBrowserTest,
                       NetworkServiceFailsResolvingBadHostname) {
  ResolveHostFuture future;
  ResolveHostname(kFailHostname, future.GetCallback());

  auto [result, resolve_error_info, resolved_addresses, endpoints] =
      future.Take();

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
  // If system DNS resolution runs in the browser process, check here that
  // the resolver received the correct number of resolves.
  EXPECT_EQ(host_resolver()->NumResolvesForHostPattern(kFailHostname), 1u);
#endif

  EXPECT_EQ(resolve_error_info.error, net::ERR_NAME_NOT_RESOLVED);
  EXPECT_EQ(result, net::ERR_NAME_NOT_RESOLVED);
}

// Check if the system's own host name resolves, which is a slightly different
// code path from normal resolution.
IN_PROC_BROWSER_TEST_F(SystemDnsResolverBrowserTest,
                       NetworkServiceResolvesOwnHostname) {
  base::test::TestFuture<const net::AddressList&, /*os_error_result=*/int,
                         /*net_error_result=*/int>
      future;

  // Systems with an in-process network service (e.g. some Android) will not
  // have a network_service_test().
  std::unique_ptr<net::HostResolverSystemTask> system_task;
  if (IsInProcessNetworkService()) {
    system_task = net::HostResolverSystemTask::CreateForOwnHostname(
        net::AddressFamily::ADDRESS_FAMILY_UNSPECIFIED, 0);
    system_task->Start(future.GetCallback());
  } else {
    network_service_test()->ResolveOwnHostnameWithSystemDns(
        future.GetCallback());
  }

  auto [addr_list, os_error_result, net_error_result] = future.Take();

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
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
