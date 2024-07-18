// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "net/android/network_change_notifier_delegate_android.h"
#include "net/android/network_library.h"
#include "net/base/features.h"
#include "net/base/network_change_notifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/test/cert_test_util.h"
#include "net/test/quic_simple_test_server.h"
#include "net/test/test_data_directory.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"

namespace content {

namespace {

class ScopedNetworkChangeWaiter
    : public net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  ScopedNetworkChangeWaiter() {
    net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
    if (!observed_) {
      run_loop_.Run();
    }
  }

  ~ScopedNetworkChangeWaiter() override {
    net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
  }

 private:
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override {
    observed_ = true;
    run_loop_.Quit();
  }

  bool observed_ = false;
  base::RunLoop run_loop_;
};

void WaitForNetworkChange() {
  ScopedNetworkChangeWaiter waiter;
}

}  // namespace

class QuicConnectionMigrationTest : public ContentBrowserTest {
 public:
  QuicConnectionMigrationTest() {
    feature_list_.InitWithFeatures(
        // Enabled features
        {net::features::kQuicMigrationIgnoreDisconnectSignalDuringProbing},
        // Disabled features
        {});
  }
  ~QuicConnectionMigrationTest() override = default;

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    // Configure mock cert verifier.
    auto test_cert =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "quic-chain.pem");
    net::CertVerifyResult verify_result;
    verify_result.verified_cert = test_cert;
    mock_cert_verifier_.mock_cert_verifier()->AddResultForCert(
        test_cert, verify_result, net::OK);
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);

    host_resolver()->AddRule("*", "127.0.0.1");

    net::NetworkChangeNotifierDelegateAndroid::
        EnableNetworkChangeNotifierAutoDetectForTest();
    WaitForNetworkChange();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kOriginToForceQuicOn, "*");
    command_line->AppendSwitchASCII(switches::kForceFieldTrials,
                                    "QUIC/Enabled");
    command_line->AppendSwitchASCII(
        switches::kForceFieldTrialParams,
        "QUIC.Enabled:migrate_sessions_on_network_change_v2/true/"
        "retry_without_alt_svc_on_quic_errors/false");
    mock_cert_verifier_.SetUpCommandLine(command_line);

    ASSERT_TRUE(net::QuicSimpleTestServer::Start());

    // Set up a test page that fetches a resource.
    quiche::HttpHeaderBlock headers;
    headers[":status"] = "200";
    headers["content-type"] = "text/html";
    const std::string body = R"(
      <script>
        const response = fetch('/simple.txt').then(res => res.text());
      </script>
    )";
    net::QuicSimpleTestServer::AddResponse("/test.html", std::move(headers),
                                           body);
    // Configure the resource to be delayed so that we can trigger connection
    // migrations while fetching the resource.
    net::QuicSimpleTestServer::SetResponseDelay("/simple.txt",
                                                base::Seconds(2));
  }

  void TearDown() override {
    base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
    net::QuicSimpleTestServer::Shutdown();
    ContentBrowserTest::TearDown();
  }

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  bool EnsureWifiEnabled() {
    if (net::NetworkChangeNotifier::IsOffline()) {
      return false;
    }

    std::string wifi_ssid = net::android::GetWifiSSID();
    if (!wifi_ssid.empty()) {
      return true;
    }

    net::android::SetWifiEnabledForTesting(true);
    WaitForNetworkChange();
    wifi_ssid = net::android::GetWifiSSID();
    return !wifi_ssid.empty();
  }

  GURL GetTestPageUrl() {
    return net::QuicSimpleTestServer::GetFileURL("/test.html");
  }

  EvalJsResult ResolveDelayedResponse() { return EvalJs(shell(), "response"); }

 private:
  base::test::ScopedFeatureList feature_list_;
  ContentMockCertVerifier mock_cert_verifier_;
};

// Currently trybots skip this test because trybots disabled real networks.
// TODO(crbug.com/40282869): Run this test once the infra
// supports enabling networks.
IN_PROC_BROWSER_TEST_F(QuicConnectionMigrationTest, Basic) {
  if (!EnsureWifiEnabled()) {
    GTEST_SKIP() << "This test requires wifi network.";
  }

  base::HistogramTester histograms;

  ASSERT_TRUE(NavigateToURL(shell(), GetTestPageUrl()));

  net::android::SetWifiEnabledForTesting(false);
  WaitForNetworkChange();

  EvalJsResult result = ResolveDelayedResponse();
  ASSERT_EQ(result, "Simple Hello from QUIC Server");

  FetchHistogramsFromChildProcesses();
  ASSERT_GE(histograms.GetBucketCount(
                "Net.QuicSession.ConnectionMigration",
                net::QuicConnectionMigrationStatus::MIGRATION_STATUS_SUCCESS),
            1);
}

// Currently trybots skip this test because trybots disabled real networks.
// TODO(crbug.com/40282869): Run this test once the infra
// supports enabling networks.
IN_PROC_BROWSER_TEST_F(QuicConnectionMigrationTest,
                       ConnectionCloseDuringMigration) {
  if (!EnsureWifiEnabled()) {
    GTEST_SKIP() << "This test requires wifi network.";
  }

  base::HistogramTester histograms;

  // This will close all connections in the middle of a connection migration.
  net::QuicChromiumClientSession::SetMidMigrationCallbackForTesting(
      base::BindLambdaForTesting(
          []() { net::QuicSimpleTestServer::ShutdownDispatcherForTesting(); }));

  ASSERT_TRUE(NavigateToURL(shell(), GetTestPageUrl()));

  net::android::SetWifiEnabledForTesting(false);
  WaitForNetworkChange();

  // The subresource fetch should fail because the server closed the connection
  // and retry is disabled.
  EvalJsResult result = ResolveDelayedResponse();
  EXPECT_FALSE(result.error.empty());
  EXPECT_EQ(histograms.GetTotalSum("Net.QuicProtocolError.RetryStatus"), 0);

  FetchHistogramsFromChildProcesses();
  ASSERT_EQ(histograms.GetBucketCount(
                "Net.QuicSession.ConnectionMigration",
                net::QuicConnectionMigrationStatus::MIGRATION_STATUS_SUCCESS),
            0);
}

}  // namespace content
