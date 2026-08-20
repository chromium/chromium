// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/browsing_data/browsing_data_browsertest_utils.h"
#include "content/browser/declarative_performance_observer/declarative_performance_observer_store.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/clear_site_data_utils.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/base/url_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class DeclarativePerformanceObserverBrowserTest : public ContentBrowserTest {
 public:
  DeclarativePerformanceObserverBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kDeclarativePerformanceObserver);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    browsing_data_browsertest_utils::SetIgnoreCertificateErrors(command_line);
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");

    if (IsOutOfProcessNetworkService()) {
      browsing_data_browsertest_utils::SetUpMockCertVerifier(net::OK);
    }

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_->RegisterRequestHandler(base::BindRepeating(
        &DeclarativePerformanceObserverBrowserTest::HandleRequest,
        base::Unretained(this)));
    ASSERT_TRUE(https_server_->Start());
  }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  void SetReportQuitClosure(base::OnceClosure closure) {
    base::AutoLock lock(report_lock_);
    report_quit_closure_ = std::move(closure);
  }

  std::vector<std::string> GetReceivedReports() {
    base::AutoLock lock(report_lock_);
    return received_reports_;
  }

  StoragePartition* storage_partition() {
    return shell()
        ->web_contents()
        ->GetBrowserContext()
        ->GetDefaultStoragePartition();
  }

  const StoragePartitionConfig& storage_partition_config() {
    return storage_partition()->GetConfig();
  }

  BrowserContext* browser_context() {
    return shell()->web_contents()->GetBrowserContext();
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url == "/dpo-page") {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("text/html");
      response->set_content("<html><body>DPO Page</body></html>");
      response->AddCustomHeader(
          "Reporting-Endpoints",
          "dpo_sink=\"" + https_server_->GetURL("/report").spec() + "\"");
      response->AddCustomHeader(
          "Performance-Observer",
          "report-to=\"dpo_sink\", entry-types=(\"navigation\" "
          "\"visibility-state\")");
      return response;
    }
    if (request.relative_url == "/report") {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->AddCustomHeader("Access-Control-Allow-Origin", "*");
      response->AddCustomHeader("Access-Control-Allow-Headers", "*");
      response->AddCustomHeader("Access-Control-Allow-Methods",
                                "POST, OPTIONS");

      if (request.method == net::test_server::METHOD_POST) {
        base::AutoLock lock(report_lock_);
        received_reports_.push_back(request.content);
        if (report_quit_closure_) {
          std::move(report_quit_closure_).Run();
        }
      }
      return response;
    }
    if (request.relative_url.rfind("/clear-site-data", 0) == 0) {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("text/html");
      response->set_content("<html><body>Cleared</body></html>");

      size_t pos = request.relative_url.find("header=");
      if (pos != std::string::npos) {
        std::string header_value = request.relative_url.substr(pos + 7);
        base::ReplaceSubstringsAfterOffset(&header_value, 0, "%22", "\"");
        response->AddCustomHeader("Clear-Site-Data", header_value);
      }
      return response;
    }
    return nullptr;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  base::Lock report_lock_;
  std::vector<std::string> received_reports_ GUARDED_BY(report_lock_);
  base::OnceClosure report_quit_closure_ GUARDED_BY(report_lock_);
};

IN_PROC_BROWSER_TEST_F(DeclarativePerformanceObserverBrowserTest,
                       ClearSiteDataApiTest) {
  StoragePartitionImpl* partition =
      static_cast<StoragePartitionImpl*>(storage_partition());
  DeclarativePerformanceObserverStore* store =
      partition->GetDeclarativePerformanceObserverStore();
  ASSERT_TRUE(store);

  GURL url = https_server()->GetURL("origin1.com", "/");
  url::Origin origin = url::Origin::Create(url);

  base::RunLoop run_loop;
  store->SetEarlyFailurePolicy(origin, true, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_TRUE(store->HasEarlyFailurePolicy(origin));

  base::RunLoop clear_loop;
  content::ClearSiteData(
      browser_context()->GetWeakPtr(), storage_partition_config(), origin,
      content::ClearSiteDataTypeSet({content::ClearSiteDataType::kStorage}),
      /*storage_buckets_to_remove=*/{},
      /*avoid_closing_connections=*/true,
      /*cookie_partition_key=*/std::nullopt,
      /*storage_key=*/std::nullopt,
      /*partitioned_state_allowed_only=*/false,
      /*callback=*/clear_loop.QuitClosure());
  clear_loop.Run();

  EXPECT_FALSE(store->HasEarlyFailurePolicy(origin));
}

IN_PROC_BROWSER_TEST_F(DeclarativePerformanceObserverBrowserTest,
                       ClearSiteDataHeaderClearedByStorageTest) {
  StoragePartitionImpl* partition =
      static_cast<StoragePartitionImpl*>(storage_partition());
  DeclarativePerformanceObserverStore* store =
      partition->GetDeclarativePerformanceObserverStore();
  ASSERT_TRUE(store);

  GURL url = https_server()->GetURL("origin1.com", "/clear-site-data");
  url::Origin origin = url::Origin::Create(url);

  base::RunLoop run_loop;
  store->SetEarlyFailurePolicy(origin, true, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_TRUE(store->HasEarlyFailurePolicy(origin));

  url = net::AppendQueryParameter(url, "header", "\"storage\"");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_FALSE(store->HasEarlyFailurePolicy(origin));
}

IN_PROC_BROWSER_TEST_F(DeclarativePerformanceObserverBrowserTest,
                       ClearSiteDataHeaderNotClearedByCookiesTest) {
  StoragePartitionImpl* partition =
      static_cast<StoragePartitionImpl*>(storage_partition());
  DeclarativePerformanceObserverStore* store =
      partition->GetDeclarativePerformanceObserverStore();
  ASSERT_TRUE(store);

  GURL url = https_server()->GetURL("origin1.com", "/clear-site-data");
  url::Origin origin = url::Origin::Create(url);

  base::RunLoop run_loop;
  store->SetEarlyFailurePolicy(origin, true, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_TRUE(store->HasEarlyFailurePolicy(origin));

  url = net::AppendQueryParameter(url, "header", "\"cookies\"");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(store->HasEarlyFailurePolicy(origin));
}

IN_PROC_BROWSER_TEST_F(DeclarativePerformanceObserverBrowserTest,
                       ClearSiteDataHeaderNotClearedByCacheTest) {
  StoragePartitionImpl* partition =
      static_cast<StoragePartitionImpl*>(storage_partition());
  DeclarativePerformanceObserverStore* store =
      partition->GetDeclarativePerformanceObserverStore();
  ASSERT_TRUE(store);

  GURL url = https_server()->GetURL("origin1.com", "/clear-site-data");
  url::Origin origin = url::Origin::Create(url);

  base::RunLoop run_loop;
  store->SetEarlyFailurePolicy(origin, true, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_TRUE(store->HasEarlyFailurePolicy(origin));

  url = net::AppendQueryParameter(url, "header", "\"cache\"");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(store->HasEarlyFailurePolicy(origin));
}

IN_PROC_BROWSER_TEST_F(DeclarativePerformanceObserverBrowserTest,
                       SendsReportImmediatelyOnBFCacheEntry) {
  GURL dpo_url = https_server()->GetURL("origin1.com", "/dpo-page");
  GURL page2_url = https_server()->GetURL("origin1.com", "/clear-site-data");

  EXPECT_TRUE(NavigateToURL(shell(), dpo_url));

  base::RunLoop report_loop;
  SetReportQuitClosure(report_loop.QuitClosure());

  // Navigate to page 2 (putting dpo-page into BFCache).
  EXPECT_TRUE(NavigateToURL(shell(), page2_url));

  // The report should be dispatched immediately via SendReportsForSource
  // without waiting for 1 minute.
  report_loop.Run();

  std::vector<std::string> reports = GetReceivedReports();
  ASSERT_EQ(reports.size(), 1u);
  EXPECT_TRUE(reports[0].contains("session-end"));
}

}  // namespace content
