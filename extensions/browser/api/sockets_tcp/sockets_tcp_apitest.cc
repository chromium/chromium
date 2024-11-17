// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/network_service_test_helper.h"
#include "extensions/browser/api/socket/write_quota_checker.h"
#include "extensions/browser/api/sockets_tcp/sockets_tcp_api.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/shell/test/shell_apitest.h"
#include "extensions/shell/test/shell_test.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/network_anonymization_key.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/test/test_dns_util.h"

namespace extensions {

const char kHostname[] = "www.foo.test";

class SocketsTcpApiTest : public ShellApiTest {
 public:
  SocketsTcpApiTest() {
    // Enable kPartitionConnectionsByNetworkIsolationKey so the test can verify
    // that the correct NetworkAnonymizationKey was used for the DNS lookup.
    scoped_feature_list_.InitAndEnableFeature(
        net::features::kPartitionConnectionsByNetworkIsolationKey);

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseMockCertVerifierForTesting);
  }

  void SetUpOnMainThread() override {
    ShellApiTest::SetUpOnMainThread();
    host_resolver()->AddRule(kHostname, "127.0.0.1");
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SocketsTcpApiTest, SocketsTcpCreateGood) {
  scoped_refptr<api::SocketsTcpCreateFunction> socket_create_function(
      new api::SocketsTcpCreateFunction());
  scoped_refptr<const Extension> empty_extension =
      ExtensionBuilder("Test").Build();

  socket_create_function->set_extension(empty_extension.get());
  socket_create_function->set_has_callback(true);

  std::optional<base::Value> result(
      api_test_utils::RunFunctionAndReturnSingleResult(
          socket_create_function.get(), "[]", browser_context()));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_dict());
  std::optional<int> socket_id = result->GetDict().FindInt("socketId");
  ASSERT_TRUE(socket_id);
  ASSERT_GT(*socket_id, 0);
}

IN_PROC_BROWSER_TEST_F(SocketsTcpApiTest, SocketTcpExtension) {
  net::EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTP);
  test_server.AddDefaultHandlers();
  EXPECT_TRUE(test_server.Start());

  net::HostPortPair host_port_pair = test_server.host_port_pair();
  int port = host_port_pair.port();
  ASSERT_TRUE(port > 0);

  // Test that connect() is properly resolving hostnames.
  host_port_pair.set_host(kHostname);

  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser_context());

  ExtensionTestMessageListener listener("info_please",
                                        ReplyBehavior::kWillReply);

  scoped_refptr<const Extension> test_extension = LoadApp("sockets_tcp/api");
  ASSERT_TRUE(test_extension);

  EXPECT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply(
      base::StringPrintf("tcp:%s:%d", host_port_pair.host().c_str(), port));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  // Make sure the extension's NetworkAnonymizationKey was used. Do a cache only
  // DNS lookup using the expected NIK, and make sure the IP address is
  // retrieved.
  network::mojom::NetworkContext* network_context =
      browser_context()->GetDefaultStoragePartition()->GetNetworkContext();
  network::mojom::ResolveHostParametersPtr params =
      network::mojom::ResolveHostParameters::New();
  // Cache only lookup.
  params->source = net::HostResolverSource::LOCAL_ONLY;
  net::SchemefulSite site = net::SchemefulSite(test_extension->url());
  auto network_anonymization_key =
      net::NetworkAnonymizationKey::CreateSameSite(site);
  network::DnsLookupResult result1 =
      network::BlockingDnsLookup(network_context, host_port_pair,
                                 std::move(params), network_anonymization_key);
  EXPECT_EQ(net::OK, result1.error);
  ASSERT_TRUE(result1.resolved_addresses.has_value());
  ASSERT_EQ(1u, result1.resolved_addresses->size());
  EXPECT_EQ("127.0.0.1",
            result1.resolved_addresses.value()[0].ToStringWithoutPort());

  // Check that the entry isn't present in the cache with the empty
  // NetworkAnonymizationKey.
  params = network::mojom::ResolveHostParameters::New();
  // Cache only lookup.
  params->source = net::HostResolverSource::LOCAL_ONLY;
  network::DnsLookupResult result2 = network::BlockingDnsLookup(
      network_context, host_port_pair, std::move(params),
      net::NetworkAnonymizationKey());
  EXPECT_EQ(net::ERR_NAME_NOT_RESOLVED, result2.error);
}

IN_PROC_BROWSER_TEST_F(SocketsTcpApiTest, SocketTcpExtensionTLS) {
  // Because the network service runs in a utility process, the cert of the
  // EmbeddedTestServer won't be recognized, so inject mock cert verifier
  // through the test helper interface.
  mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
  content::GetNetworkService()->BindTestInterfaceForTesting(
      network_service_test.BindNewPipeAndPassReceiver());
  mojo::ScopedAllowSyncCallForTesting allow_sync_call;
  network_service_test->MockCertVerifierSetDefaultResult(net::OK);

  net::EmbeddedTestServer test_https_server(
      net::EmbeddedTestServer::TYPE_HTTPS);
  test_https_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("net/data")));
  EXPECT_TRUE(test_https_server.Start());

  net::HostPortPair https_host_port_pair = test_https_server.host_port_pair();
  int https_port = https_host_port_pair.port();
  ASSERT_GT(https_port, 0);

  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser_context());

  ExtensionTestMessageListener listener("info_please",
                                        ReplyBehavior::kWillReply);

  ASSERT_TRUE(LoadApp("sockets_tcp/api"));
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply(base::StringPrintf(
      "https:%s:%d", https_host_port_pair.host().c_str(), https_port));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(SocketsTcpApiTest, SocketTcpSendWriteQuota) {
  WriteQuotaChecker* write_quota_checker =
      WriteQuotaChecker::Get(browser_context());
  constexpr size_t kBytesLimit = 1;
  WriteQuotaChecker::ScopedBytesLimitForTest scoped_quota(write_quota_checker,
                                                          kBytesLimit);

  net::EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTP);
  test_server.AddDefaultHandlers();
  EXPECT_TRUE(test_server.Start());

  net::HostPortPair host_port_pair = test_server.host_port_pair();
  int port = host_port_pair.port();
  ASSERT_GT(port, 0);

  // Test that connect() is properly resolving hostnames.
  host_port_pair.set_host(kHostname);

  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser_context());

  ExtensionTestMessageListener listener("info_please",
                                        ReplyBehavior::kWillReply);

  scoped_refptr<const Extension> test_extension = LoadApp("sockets_tcp/api");
  ASSERT_TRUE(test_extension);

  EXPECT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply(base::StringPrintf("tcp_send_write_quota:%s:%d",
                                    host_port_pair.host().c_str(), port));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  // namespace extensions
