// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "content/browser/browsing_data/browsing_data_browsertest_utils.h"
#include "content/browser/declarative_performance_observer/declarative_performance_observer_store.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/clear_site_data_utils.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_utils.h"
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
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kDeclarativePerformanceObserver,
         features::kBackForwardCache},
        {});
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

  void ClearReceivedReports() {
    base::AutoLock lock(report_lock_);
    received_reports_.clear();
  }

  void WaitForReportCount(size_t expected_count) {
    while (true) {
      base::RunLoop run_loop;
      {
        base::AutoLock lock(report_lock_);
        if (received_reports_.size() >= expected_count) {
          return;
        }
        report_quit_closure_ = run_loop.QuitClosure();
      }
      run_loop.Run();
    }
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
    if (request.relative_url == "/dpo-page" ||
        request.relative_url == "/declarative-performance-observer-page") {
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
    if (request.relative_url ==
        "/declarative-performance-observer-early-failures-page") {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("text/html");
      response->set_content(
          "<html><body>DeclarativePerformanceObserver Page</body></html>");
      response->AddCustomHeader(
          "Reporting-Endpoints",
          "telemetry_sink=\"" + https_server_->GetURL("/report").spec() + "\"");
      response->AddCustomHeader(
          "Performance-Observer",
          "report-to=\"telemetry_sink\", capture-early-failures=?1, "
          "entry-types=(\"navigation\" \"visibility-state\")");
      return response;
    }
    if (request.relative_url == "/early-failure-1" ||
        request.relative_url == "/early-failure-2") {
      return std::make_unique<net::test_server::RawHttpResponse>("", "");
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
  base::Lock report_lock_;
  std::vector<std::string> received_reports_ GUARDED_BY(report_lock_);
  base::OnceClosure report_quit_closure_ GUARDED_BY(report_lock_);
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
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
  GURL declarative_performance_observer_url =
      https_server()->GetURL("origin1.com", "/dpo-page");
  GURL page2_url = https_server()->GetURL("origin1.com", "/clear-site-data");

  EXPECT_TRUE(NavigateToURL(shell(), declarative_performance_observer_url));
  RenderFrameHostWrapper rfh(shell()->web_contents()->GetPrimaryMainFrame());

  base::RunLoop report_loop;
  SetReportQuitClosure(report_loop.QuitClosure());

  // Navigate to page 2 (putting page into BFCache).
  EXPECT_TRUE(NavigateToURL(shell(), page2_url));

  ASSERT_TRUE(rfh->IsInLifecycleState(
      RenderFrameHost::LifecycleState::kInBackForwardCache));

  // The report should be dispatched immediately via SendReportsForSource
  // without waiting for 1 minute.
  report_loop.Run();

  std::vector<std::string> reports = GetReceivedReports();
  ASSERT_EQ(reports.size(), 1u);
  EXPECT_TRUE(reports[0].contains("session-end"));

  // Restore the cached document and verify that subsequent observer reports
  // are queued and delivered properly.
  base::RunLoop restore_report_loop;
  SetReportQuitClosure(restore_report_loop.QuitClosure());

  shell()->web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(rfh.get(), shell()->web_contents()->GetPrimaryMainFrame());
  EXPECT_FALSE(rfh->IsInLifecycleState(
      RenderFrameHost::LifecycleState::kInBackForwardCache));

  // Navigate away again to trigger subsequent session-end report.
  EXPECT_TRUE(NavigateToURL(shell(), page2_url));
  restore_report_loop.Run();

  reports = GetReceivedReports();
  ASSERT_EQ(reports.size(), 2u);
  EXPECT_TRUE(reports[1].contains("session-end"));
}

// Verifies that when a renderer process crashes or is killed
// (crbug.com/558351483), DeclarativePerformanceObserver::OnFrameDeleted()
// queues the performance-observer report before
// RenderFrameHostImpl::RenderProcessGone() calls SendReportsAndRemoveSource(),
// resulting in immediate delivery to the reporting endpoint.
IN_PROC_BROWSER_TEST_F(DeclarativePerformanceObserverBrowserTest,
                       SendsReportImmediatelyOnRendererCrash) {
  GURL declarative_performance_observer_url =
      https_server()->GetURL("origin1.com", "/dpo-page");
  EXPECT_TRUE(NavigateToURL(shell(), declarative_performance_observer_url));

  base::RunLoop report_loop;
  SetReportQuitClosure(report_loop.QuitClosure());

  RenderFrameHost* main_frame = shell()->web_contents()->GetPrimaryMainFrame();
  ScopedAllowRendererCrashes allow_renderer_crashes(main_frame);
  main_frame->GetProcess()->Shutdown(RESULT_CODE_KILLED);

  report_loop.Run();

  std::vector<std::string> reports = GetReceivedReports();
  ASSERT_EQ(reports.size(), 1u);
  EXPECT_TRUE(reports[0].contains("performance-observer"));
  EXPECT_TRUE(reports[0].contains("session-end"));
}

// Verifies that when a cross-RenderFrameHost navigation destroys the previous
// RenderFrameHost without storing it in BackForwardCache (crbug.com/558351483),
// RenderFrameHostImpl::~RenderFrameHostImpl() flushes
// DeclarativePerformanceObserver and invokes SendReportsAndRemoveSource(),
// delivering the performance-observer report immediately without waiting for
// the 1-minute ReportingDeliveryAgent timer.
IN_PROC_BROWSER_TEST_F(DeclarativePerformanceObserverBrowserTest,
                       SendsReportImmediatelyOnRFHDestructionWithoutBFCache) {
  DisableBackForwardCacheForTesting(shell()->web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL declarative_performance_observer_url = https_server()->GetURL(
      "origin1.com", "/declarative-performance-observer-page");
  GURL cross_site_url =
      https_server()->GetURL("origin2.com", "/clear-site-data");

  EXPECT_TRUE(NavigateToURL(shell(), declarative_performance_observer_url));
  RenderFrameHostWrapper old_rfh(
      shell()->web_contents()->GetPrimaryMainFrame());

  base::RunLoop report_loop;
  SetReportQuitClosure(report_loop.QuitClosure());

  // Navigate cross-site so the old RenderFrameHost is unloaded and destroyed
  // instead of entering BackForwardCache.
  EXPECT_TRUE(NavigateToURL(shell(), cross_site_url));
  ASSERT_TRUE(old_rfh.WaitUntilRenderFrameDeleted());

  // The report should be dispatched immediately via SendReportsAndRemoveSource
  // in ~RenderFrameHostImpl() without waiting for the 1-minute timer.
  report_loop.Run();

  std::vector<std::string> reports = GetReceivedReports();
  ASSERT_EQ(reports.size(), 1u);
  EXPECT_TRUE(reports[0].contains("performance-observer"));
  EXPECT_TRUE(reports[0].contains("session-end"));
}

// Verifies that when stored early failure reports are taken upon navigation
// commit, `DeclarativePerformanceObserver` immediately dispatches them via
// `SendReportsForSource()` using the document's `reporting_source_` while the
// document is still active, and subsequently dispatches the document's own
// session-end report in a separate HTTP POST request upon session end.
IN_PROC_BROWSER_TEST_F(
    DeclarativePerformanceObserverBrowserTest,
    SendsEarlyFailureReportsImmediatelyAndSessionEndSeparately) {
  GURL observer_url = https_server()->GetURL(
      "origin1.com", "/declarative-performance-observer-early-failures-page");
  GURL fail_url_1 = https_server()->GetURL("origin1.com", "/early-failure-1");
  GURL fail_url_2 = https_server()->GetURL("origin1.com", "/early-failure-2");
  GURL page2_url = https_server()->GetURL("origin1.com", "/clear-site-data");

  // 1. Navigate to the opted-in page so `capture-early-failures=?1` is
  // persisted for `origin1.com`.
  EXPECT_TRUE(NavigateToURL(shell(), observer_url));
  EXPECT_TRUE(NavigateToURL(shell(), page2_url));
  WaitForReportCount(1u);
  ClearReceivedReports();

  // 2. Trigger two early navigation failures (`ERR_EMPTY_RESPONSE`) on
  // `origin1.com`.
  EXPECT_FALSE(NavigateToURL(shell(), fail_url_1));
  EXPECT_FALSE(NavigateToURL(shell(), fail_url_2));

  // 3. Commit a new navigation to `observer_url`. Both stored early failure
  // reports must be dispatched immediately via `SendReportsForSource()` while
  // the committed document is still active (before navigating away).
  EXPECT_TRUE(NavigateToURL(shell(), observer_url));
  WaitForReportCount(2u);

  std::vector<std::string> early_reports = GetReceivedReports();
  ASSERT_EQ(early_reports.size(), 2u);
  for (const std::string& raw_post : early_reports) {
    std::optional<base::Value> parsed =
        base::JSONReader::Read(raw_post, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    ASSERT_TRUE(parsed.has_value());
    ASSERT_TRUE(parsed->is_list());
    EXPECT_EQ(parsed->GetList().size(), 1u);
    EXPECT_TRUE(raw_post.contains("ERR_EMPTY_RESPONSE"));
    EXPECT_FALSE(raw_post.contains("session-end"));
  }

  // 4. Navigate away to end the document session and verify that the
  // session-end report arrives in a third separate HTTP POST request.
  EXPECT_TRUE(NavigateToURL(shell(), page2_url));
  WaitForReportCount(3u);

  std::vector<std::string> all_reports = GetReceivedReports();
  ASSERT_EQ(all_reports.size(), 3u);
  std::optional<base::Value> session_post = base::JSONReader::Read(
      all_reports[2], base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(session_post.has_value());
  ASSERT_TRUE(session_post->is_list());
  EXPECT_EQ(session_post->GetList().size(), 1u);
  EXPECT_TRUE(all_reports[2].contains("session-end"));
  EXPECT_FALSE(all_reports[2].contains("ERR_EMPTY_RESPONSE"));
}

}  // namespace content
