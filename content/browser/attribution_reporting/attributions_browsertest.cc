// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/attribution_reporting/constants.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_manager_impl.h"
#include "content/browser/attribution_reporting/attribution_os_level_manager.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/test/mock_attribution_observer.h"
#include "content/browser/attribution_reporting/test/mock_content_browser_client.h"
#include "content/browser/fenced_frame/fenced_frame_reporter.h"
#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"
#include "content/browser/private_aggregation/private_aggregation_manager.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/page_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_core_observer.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/fenced_frame_test_utils.h"
#include "content/test/test_content_browser_client.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/network/public/cpp/features.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"
#include "third_party/blink/public/common/fenced_frame/redacted_fenced_frame_config.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using ::testing::_;
using ::testing::Return;

constexpr char kBaseDataDir[] = "content/test/data/";

using attribution_reporting::kAttributionReportingRegisterSourceHeader;

void ExpectRegisterResultAndRun(blink::ServiceWorkerStatusCode expected,
                                base::RepeatingClosure continuation,
                                blink::ServiceWorkerStatusCode actual) {
  EXPECT_EQ(expected, actual);
  continuation.Run();
}

// Observer which waits for a service worker to register in the browser process
// by observing worker activation status.
class WorkerStateObserver : public ServiceWorkerContextCoreObserver {
 public:
  WorkerStateObserver(scoped_refptr<ServiceWorkerContextWrapper> context,
                      ServiceWorkerVersion::Status target)
      : context_(std::move(context)), target_(target) {
    observation_.Observe(context_.get());
  }

  WorkerStateObserver(const WorkerStateObserver&) = delete;
  WorkerStateObserver& operator=(const WorkerStateObserver&) = delete;

  ~WorkerStateObserver() override = default;

  // ServiceWorkerContextCoreObserver overrides.
  void OnVersionStateChanged(int64_t version_id,
                             const GURL& scope,
                             const blink::StorageKey& key,
                             ServiceWorkerVersion::Status) override {
    const ServiceWorkerVersion* version = context_->GetLiveVersion(version_id);
    if (version->status() == target_) {
      context_->RemoveObserver(this);
      run_loop_.Quit();
    }
  }
  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;
  scoped_refptr<ServiceWorkerContextWrapper> context_;
  const ServiceWorkerVersion::Status target_;
  base::ScopedObservation<ServiceWorkerContextWrapper,
                          ServiceWorkerContextCoreObserver>
      observation_{this};
};

// Waits for the a given |report_url| to be received by the test server. Wraps a
// ControllableHttpResponse so that it can wait for the server request in a
// thread-safe manner. Therefore, these must be registered prior to |server|
// starting.
struct ExpectedReportWaiter {
  ExpectedReportWaiter(GURL report_url,
                       std::string attribution_destination,
                       std::string source_event_id,
                       std::string source_type,
                       std::string trigger_data,
                       net::EmbeddedTestServer* server)
      : ExpectedReportWaiter(std::move(report_url),
                             base::Value::Dict(),
                             server) {
    expected_body.Set("attribution_destination",
                      std::move(attribution_destination));
    expected_body.Set("source_event_id", std::move(source_event_id));
    expected_body.Set("source_type", std::move(source_type));
    expected_body.Set("trigger_data", std::move(trigger_data));
  }

  // ControllableHTTPResponses can only wait for relative urls, so only supply
  // the path.
  ExpectedReportWaiter(GURL report_url,
                       base::Value::Dict body,
                       net::EmbeddedTestServer* server)
      : expected_url(std::move(report_url)),
        expected_body(std::move(body)),
        response(std::make_unique<net::test_server::ControllableHttpResponse>(
            server,
            expected_url.path())) {}

  GURL expected_url;
  base::Value::Dict expected_body;
  std::string source_debug_key;
  std::string trigger_debug_key;
  std::unique_ptr<net::test_server::ControllableHttpResponse> response;

  bool HasRequest() { return !!response->http_request(); }

  // Waits for a report to be received matching the report url. Verifies that
  // the report url and report body were set correctly.
  void WaitForReport() {
    if (!response->http_request()) {
      response->WaitForRequest();
    }

    // The embedded test server resolves all urls to 127.0.0.1, so get the real
    // request host from the request headers.
    const net::test_server::HttpRequest& request = *response->http_request();
    DCHECK(base::Contains(request.headers, "Host"));
    const GURL& request_url = request.GetURL();
    GURL header_url = GURL("https://" + request.headers.at("Host"));
    std::string host = header_url.host();
    GURL::Replacements replace_host;
    replace_host.SetHostStr(host);

    base::Value::Dict body = base::test::ParseJsonDict(request.content);
    EXPECT_THAT(body, base::test::DictionaryHasValues(expected_body));

    // The report ID is random, so just test that the field exists here and is a
    // valid GUID.
    const std::string* report_id = body.FindString("report_id");
    ASSERT_TRUE(report_id);
    EXPECT_TRUE(base::Uuid::ParseLowercase(*report_id).is_valid());

    EXPECT_TRUE(body.FindDouble("randomized_trigger_rate"));

    if (source_debug_key.empty()) {
      EXPECT_FALSE(body.FindString("source_debug_key"));
    } else {
      base::ExpectDictStringValue(source_debug_key, body, "source_debug_key");
    }

    if (trigger_debug_key.empty()) {
      EXPECT_FALSE(body.FindString("trigger_debug_key"));
    } else {
      base::ExpectDictStringValue(trigger_debug_key, body, "trigger_debug_key");
    }

    // Clear the port as it is assigned by the EmbeddedTestServer at runtime.
    replace_host.SetPortStr("");

    // Compare the expected report url with a URL formatted with the host
    // defined in the headers. This would not match |expected_url| if the host
    // for report url was not set properly.
    EXPECT_EQ(expected_url, request_url.ReplaceComponents(replace_host));

    EXPECT_TRUE(base::Contains(request.headers, "User-Agent"));
    EXPECT_EQ(request.headers.at("Content-Type"), "application/json");
  }
};

}  // namespace

class InterestGroupEnabledContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  explicit InterestGroupEnabledContentBrowserClient() = default;

  InterestGroupEnabledContentBrowserClient(
      const InterestGroupEnabledContentBrowserClient&) = delete;
  InterestGroupEnabledContentBrowserClient& operator=(
      const InterestGroupEnabledContentBrowserClient&) = delete;

  // ContentBrowserClient overrides:
  // This is needed so that the interest group related APIs can run without
  // failing with the result AuctionResult::kSellerRejected.
  bool IsPrivacySandboxReportingDestinationAttested(
      content::BrowserContext* browser_context,
      const url::Origin& destination_origin,
      content::PrivacySandboxInvokingAPI invoking_api) override {
    return true;
  }
};

class AttributionsBrowserTestBase : public ContentBrowserTest {
 public:
  AttributionsBrowserTestBase() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kAttributionReportingDebugMode);

    // Sets up the blink runtime feature for ConversionMeasurement.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetUpOnMainThread() override {
    // These tests don't cover online/offline behavior; that is covered by
    // `AttributionManagerImpl`'s unit tests. Here we use a fake tracker that
    // always indicates online. See crbug.com/1285057 for details.
    network_connection_tracker_ =
        network::TestNetworkConnectionTracker::CreateInstance();
    SetNetworkConnectionTrackerForTesting(nullptr);
    SetNetworkConnectionTrackerForTesting(network_connection_tracker_.get());

    host_resolver()->AddRule("*", "127.0.0.1");

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    net::test_server::RegisterDefaultHandlers(https_server_.get());
    https_server_->ServeFilesFromSourceDirectory("content/test/data");
    https_server_->ServeFilesFromSourceDirectory(
        "content/test/data/attribution_reporting");

    StoragePartition* partition = shell()
                                      ->web_contents()
                                      ->GetBrowserContext()
                                      ->GetDefaultStoragePartition();
    wrapper_ = static_cast<ServiceWorkerContextWrapper*>(
        partition->GetServiceWorkerContext());
    content_browser_client_ =
        std::make_unique<InterestGroupEnabledContentBrowserClient>();
  }

  void TearDownOnMainThread() override {
    SetNetworkConnectionTrackerForTesting(nullptr);
  }

  WebContents* web_contents() { return shell()->web_contents(); }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  AttributionManager* attribution_manager() {
    return static_cast<StoragePartitionImpl*>(
               web_contents()
                   ->GetBrowserContext()
                   ->GetDefaultStoragePartition())
        ->GetAttributionManager();
  }

  void RegisterSource(const GURL& attribution_src_url) {
    MockAttributionObserver observer;
    base::ScopedObservation<AttributionManager, AttributionObserver>
        observation(&observer);
    observation.Observe(attribution_manager());

    base::RunLoop loop;
    EXPECT_CALL(observer,
                OnSourceHandled(_, _, _, StorableSource::Result::kSuccess))
        .WillOnce([&]() { loop.Quit(); });

    EXPECT_TRUE(ExecJs(web_contents(), JsReplace("createAttributionSrcImg($1);",
                                                 attribution_src_url)));

    // Wait until the source has been stored before registering the trigger;
    // otherwise the trigger could be processed before the source, in which case
    // there would be no matching source: crbug.com/1309173.
    loop.Run();
  }

  void CreateAndClickSource(WebContents* web_contents,
                            const GURL& href,
                            const std::string& attribution_src) {
    CreateAndClickSourceInFrame(web_contents,
                                web_contents->GetPrimaryMainFrame(), href,
                                attribution_src,
                                /*target=*/"_top");
  }

  WebContents* CreateAndClickPopupSource(WebContents* web_contents,
                                         const GURL& href,
                                         const std::string& attribution_src,
                                         const std::string& target) {
    return CreateAndClickSourceInFrame(nullptr,
                                       web_contents->GetPrimaryMainFrame(),
                                       href, attribution_src, target);
  }

  WebContents* CreateAndClickSourceInFrame(WebContents* web_contents,
                                           RenderFrameHost* rfh,
                                           const GURL& href,
                                           const std::string& attribution_src,
                                           const std::string& target) {
    EXPECT_TRUE(ExecJs(rfh, JsReplace(R"(
    createAttributionSrcAnchor({id: 'link',
                        url: $1,
                        attributionsrc: $2,
                        target: $3});)",
                                      href, attribution_src, target)));

    MockAttributionObserver source_observer;
    base::ScopedObservation<AttributionManager, AttributionObserver>
        observation(&source_observer);
    observation.Observe(attribution_manager());

    base::RunLoop loop;
    bool received = false;
    EXPECT_CALL(source_observer,
                OnSourceHandled(_, _, _, StorableSource::Result::kSuccess))
        .WillOnce([&]() {
          received = true;
          loop.Quit();
        });

    WebContents* popup_contents = nullptr;
    if (!web_contents) {
      ShellAddedObserver new_shell_observer;
      TestNavigationObserver observer(nullptr);
      observer.StartWatchingNewWebContents();
      EXPECT_TRUE(ExecJs(rfh, "simulateClick('link');"));
      popup_contents = new_shell_observer.GetShell()->web_contents();
      observer.Wait();
    } else {
      TestNavigationObserver observer(web_contents);
      EXPECT_TRUE(ExecJs(rfh, "simulateClick('link');"));
      observer.Wait();
    }

    // If the source wasn't processed, wait to ensure we handle events in test
    // order. See https://crbug.com/1309173.
    if (!received) {
      loop.Run();
    }

    return popup_contents;
  }

  ServiceWorkerContextWrapper* wrapper() { return wrapper_.get(); }
  ServiceWorkerContext* public_context() { return wrapper(); }

 private:
  AttributionManagerImpl::ScopedUseInMemoryStorageForTesting
      attribution_manager_in_memory_setting_;

  std::unique_ptr<net::EmbeddedTestServer> https_server_;

  std::unique_ptr<network::TestNetworkConnectionTracker>
      network_connection_tracker_;

  scoped_refptr<ServiceWorkerContextWrapper> wrapper_;

  std::unique_ptr<InterestGroupEnabledContentBrowserClient>
      content_browser_client_;
};

class AttributionsBrowserTest : public AttributionsBrowserTestBase,
                                public ::testing::WithParamInterface<bool> {
 public:
  explicit AttributionsBrowserTest(
      std::vector<base::test::FeatureRef> enabled_features = {},
      std::vector<base::test::FeatureRef> disabled_features = {}) {
    const bool enable_in_browser_migration = GetParam();

    if (enable_in_browser_migration) {
      enabled_features.emplace_back(
          blink::features::kKeepAliveInBrowserMigration);
      enabled_features.emplace_back(
          blink::features::kAttributionReportingInBrowserMigration);
    } else {
      disabled_features.emplace_back(
          blink::features::kKeepAliveInBrowserMigration);
    }

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, AttributionsBrowserTest, ::testing::Bool());

// Verifies that storage initialization does not hang when initialized in a
// browsertest context, see https://crbug.com/1080764).
IN_PROC_BROWSER_TEST_P(AttributionsBrowserTest,
                       FeatureEnabled_StorageInitWithoutHang) {}

IN_PROC_BROWSER_TEST_P(AttributionsBrowserTest,
                       ImpressionNavigationMultipleRedirects_FirstReportSent) {
  auto register_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server(), "/register_source_redirect");

  auto register_response2 =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server(), "/register_source_redirect");

  // Expected reports must be registered before the server starts.
  ExpectedReportWaiter expected_report(
      GURL("https://d.test/.well-known/attribution-reporting/"
           "report-event-attribution"),
      /*attribution_destination=*/"https://c.test",
      /*source_event_id=*/"1", /*source_type=*/"navigation",
      /*trigger_data=*/"7", https_server());
  ExpectedReportWaiter expected_report2(
      GURL("https://b.test/.well-known/attribution-reporting/"
           "report-event-attribution"),
      /*attribution_destination=*/"https://c.test",
      /*source_event_id=*/"2", /*source_type=*/"navigation",
      /*trigger_data=*/"7", https_server());
  ASSERT_TRUE(https_server()->Start());

  GURL impression_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), impression_url));

  // Create an anchor tag with impression attributes and click the link. By
  // default the target is set to "_top".
  GURL register_source_url =
      https_server()->GetURL("d.test", "/register_source_redirect");

  EXPECT_TRUE(ExecJs(web_contents(), JsReplace(R"(
    createAttributionSrcAnchor({id: 'link',
                        url: $1,
                        attributionsrc: '',
                        target: $2});)",
                                               register_source_url, "_top")));

  TestNavigationObserver observer(web_contents());
  EXPECT_TRUE(ExecJs(web_contents(), "simulateClick('link');"));

  register_response->WaitForRequest();
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader(
      kAttributionReportingRegisterSourceHeader,
      R"({"source_event_id":"1","destination":"https://c.test"})");

  http_response->AddCustomHeader(
      "Location",
      https_server()->GetURL("b.test", "/register_source_redirect").spec());
  register_response->Send(http_response->ToResponseString());
  register_response->Done();

  register_response2->WaitForRequest();
  auto http_response2 = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response2->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response2->AddCustomHeader(
      kAttributionReportingRegisterSourceHeader,
      R"({"source_event_id":"2","destination":"https://c.test"})");

  http_response2->AddCustomHeader(
      "Location",
      https_server()
          ->GetURL("c.test",
                   "/attribution_reporting/page_with_conversion_redirect.html")
          .spec());
  register_response2->Send(http_response2->ToResponseString());
  register_response2->Done();

  // Wait for navigation to complete.
  observer.Wait();

  GURL register_trigger_url = https_server()->GetURL(
      "d.test", "/attribution_reporting/register_trigger_headers.html");
  EXPECT_TRUE(ExecJs(web_contents(), JsReplace("createAttributionSrcImg($1);",
                                               register_trigger_url)));
  expected_report.WaitForReport();

  GURL register_trigger_url2 = https_server()->GetURL(
      "b.test", "/attribution_reporting/register_trigger_headers.html");
  EXPECT_TRUE(ExecJs(web_contents(), JsReplace("createAttributionSrcImg($1);",
                                               register_trigger_url2)));
  expected_report2.WaitForReport();
}

IN_PROC_BROWSER_TEST_P(AttributionsBrowserTest,
                       ImpressionsRegisteredOnNavigation_ReportSent) {
  // Expected reports must be registered before the server starts.
  ExpectedReportWaiter expected_report(
      GURL("https://c.test/.well-known/attribution-reporting/"
           "report-event-attribution"),
      /*attribution_destination=*/"https://c.test",
      /*source_event_id=*/"1", /*source_type=*/"navigation",
      /*trigger_data=*/"7", https_server());
  ASSERT_TRUE(https_server()->Start());

  // 1 - Navigate on a page that can register a source
  GURL impression_url = https_server()->GetURL(
      "b.test", "/attribution_reporting/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), impression_url));

  // 2 - Create an anchor tag with impression attributes and click the link.
  GURL register_source_url = https_server()->GetURL(
      "c.test",
      "/attribution_reporting/"
      "page_with_source_registration_and_conversion.html");
  EXPECT_TRUE(ExecJs(web_contents(), JsReplace(R"(
    createAttributionSrcAnchor({id: 'link',
                        url: $1,
                        attributionsrc: ''});)",
                                               register_source_url)));

  TestNavigationObserver observer(web_contents());
  EXPECT_TRUE(ExecJs(web_contents(), "simulateClick('link');"));

  // Wait for navigation to complete.
  observer.Wait();

  // 3 - On the landing page and destination origin, register a trigger that
  // should match the navigation-source.
  GURL register_trigger_url = https_server()->GetURL(
      "c.test", "/attribution_reporting/register_trigger_headers.html");
  EXPECT_TRUE(ExecJs(web_contents(), JsReplace("createAttributionSrcImg($1);",
                                               register_trigger_url)));
  expected_report.WaitForReport();
}

class AttributionsCrossAppWebDisabledBrowserTest
    : public AttributionsBrowserTest {
 public:
  AttributionsCrossAppWebDisabledBrowserTest()
      : AttributionsBrowserTest(
            /*enabled_features=*/{},
            /*disabled_features=*/{
                network::features::kAttributionReportingCrossAppWeb}) {}
};

INSTANTIATE_TEST_SUITE_P(All,
                         AttributionsCrossAppWebDisabledBrowserTest,
                         ::testing::Bool());

IN_PROC_BROWSER_TEST_P(AttributionsCrossAppWebDisabledBrowserTest,
                       AttributionEligibleNavigation_SetsEligibleHeader) {
  auto register_response1 =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server(), "/register_source_redirect");
  auto register_response2 =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server(), "/register_source_redirect2");
  ASSERT_TRUE(https_server()->Start());

  GURL impression_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), impression_url));

  GURL register_source_url =
      https_server()->GetURL("d.test", "/register_source_redirect");

  // Don't use `CreateAndClickSource()` as we need to observe navigation
  // redirects prior to the navigation finishing.
  EXPECT_TRUE(ExecJs(web_contents(), JsReplace(R"(
    createAttributionSrcAnchor({id: 'link',
                        url: $1,
                        attributionsrc: '',
                        target: $2});)",
                                               register_source_url, "_top")));
  EXPECT_TRUE(ExecJs(web_contents(), "simulateClick('link');"));

  // Verify the navigation redirects contain the eligibility header.
  register_response1->WaitForRequest();
  ExpectValidAttributionReportingEligibleHeaderForNavigation(
      register_response1->http_request()->headers.at(
          "Attribution-Reporting-Eligible"));
  EXPECT_FALSE(base::Contains(register_response1->http_request()->headers,
                              "Attribution-Reporting-Support"));

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader("Location", "/register_source_redirect2");
  register_response1->Send(http_response->ToResponseString());
  register_response1->Done();

  // Ensure that redirect requests also contain the header.
  register_response2->WaitForRequest();
  ExpectValidAttributionReportingEligibleHeaderForNavigation(
      register_response2->http_request()->headers.at(
          "Attribution-Reporting-Eligible"));
  ASSERT_FALSE(base::Contains(register_response2->http_request()->headers,
                              "Attribution-Reporting-Support"));
}

IN_PROC_BROWSER_TEST_P(AttributionsBrowserTest,
                       NonAttributionEligibleNavigation_NoEligibleHeader) {
  auto register_response1 =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server(), "/register_source_redirect");
  ASSERT_TRUE(https_server()->Start());

  GURL impression_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), impression_url));

  GURL register_source_url =
      https_server()->GetURL("d.test", "/register_source_redirect");

  // Create a non-attribution eligible anchor and click.
  EXPECT_TRUE(ExecJs(web_contents(), JsReplace(R"(
    const anchor = document.createElement('a');
    anchor.href = $1;
    anchor.target = '_top';
    anchor.id = 'link';
    document.body.appendChild(anchor);)",
                                               register_source_url)));
  EXPECT_TRUE(ExecJs(web_contents(), "simulateClick('link');"));

  // Verify the navigation request does not contain the eligibility header.
  register_response1->WaitForRequest();
  EXPECT_FALSE(base::Contains(register_response1->http_request()->headers,
                              "Attribution-Reporting-Eligible"));
  EXPECT_FALSE(base::Contains(register_response1->http_request()->headers,
                              "Attribution-Reporting-Support"));

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  register_response1->Send(http_response->ToResponseString());
  register_response1->Done();
}

IN_PROC_BROWSER_TEST_P(AttributionsBrowserTest,
                       ImpressionFromCrossOriginSubframe_ReportSent) {
  ExpectedReportWaiter expected_report(
      GURL("https://a.test/.well-known/attribution-reporting/"
           "report-event-attribution"),
      /*attribution_destination=*/"https://d.test",
      /*source_event_id=*/"5", /*source_type=*/"navigation",
      /*trigger_data=*/"7", https_server());
  ASSERT_TRUE(https_server()->Start());

  GURL page_url = https_server()->GetURL("a.test", "/page_with_iframe.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  GURL subframe_url = https_server()->GetURL(
      "c.test", "/attribution_reporting/page_with_impression_creator.html");
  EXPECT_TRUE(ExecJs(shell(), R"(
    let frame= document.getElementById('test_iframe');
    frame.setAttribute('allow', 'attribution-reporting');)"));
  NavigateIframeToURL(web_contents(), "test_iframe", subframe_url);
  RenderFrameHost* subframe =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);

  GURL conversion_url = https_server()->GetURL(
      "d.test", "/attribution_reporting/page_with_conversion_redirect.html");
  GURL register_source_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/register_source_headers.html");

  // Create an impression tag in the subframe and target a popup window.
  auto* popup_contents = CreateAndClickSourceInFrame(
      /*web-contents=*/nullptr, subframe, conversion_url,
      register_source_url.spec(),
      /*target=*/"new_frame");

  GURL register_trigger_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/register_trigger_headers.html");
  EXPECT_TRUE(ExecJs(popup_contents, JsReplace("createAttributionSrcImg($1);",
                                               register_trigger_url)));

  expected_report.WaitForReport();
}

// Regression test for crbug.com/1366513.
// TODO(b/331159758): Disabled due to flakiness.
IN_PROC_BROWSER_TEST_P(AttributionsBrowserTest,
                       DISABLED_AttributionSrcInSandboxedIframe_NoCrash) {
  ExpectedReportWaiter expected_report(
      GURL("https://a.test/.well-known/attribution-reporting/"
           "report-event-attribution"),
      /*attribution_destination=*/"https://a.test",
      /*source_event_id=*/"5", /*source_type=*/"event",
      /*trigger_data=*/"1", https_server());

  auto register_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server(), "/register_source_redirect");

  auto register_response2 =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server(), "/register_trigger");

  ASSERT_TRUE(https_server()->Start());

  GURL page_url = https_server()->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), page_url));

  // Setting the frame's sandbox attribute causes its origin to be opaque.
  ASSERT_TRUE(ExecJs(shell(), R"(
    const frame = document.getElementById('test_iframe');
    frame.setAttribute('sandbox', '');
    frame.setAttribute('srcdoc', '<img attributionsrc=/register_source_redirect>');
  )"));

  {
    register_response->WaitForRequest();
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
    http_response->AddCustomHeader(
        kAttributionReportingRegisterSourceHeader,
        R"({"source_event_id":"5","destination":"https://a.test"})");
    http_response->AddCustomHeader(
        "Location",
        https_server()->GetURL("a.test", "/register_trigger").spec());
    register_response->Send(http_response->ToResponseString());
    register_response->Done();
  }

  {
    register_response2->WaitForRequest();
    auto http_response2 =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response2->AddCustomHeader(
        "Attribution-Reporting-Register-Trigger",
        R"({"event_trigger_data":[{"trigger_data":"1"}]})");
    register_response2->Send(http_response2->ToResponseString());
    register_response2->Done();
  }

  expected_report.WaitForReport();
}

IN_PROC_BROWSER_TEST_P(AttributionsBrowserTest,
                       ImpressionOnNoOpenerNavigation_ReportSent) {
  ExpectedReportWaiter expected_report(
      GURL("https://a.test/.well-known/attribution-reporting/"
           "report-event-attribution"),
      /*attribution_destination=*/"https://d.test",
      /*source_event_id=*/"5", /*source_type=*/"navigation",
      /*trigger_data=*/"7", https_server());
  ASSERT_TRUE(https_server()->Start());

  GURL impression_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), impression_url));

  GURL conversion_url = https_server()->GetURL(
      "d.test", "/attribution_reporting/page_with_conversion_redirect.html");
  GURL register_source_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/register_source_headers.html");

  // target="_blank" navs are rel="noopener" by default.
  CreateAndClickPopupSource(web_contents(), conversion_url,
                            register_source_url.spec(),
                            /*target=*/"_blank");

  GURL register_trigger_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/register_trigger_headers.html");
  EXPECT_TRUE(
      ExecJs(Shell::windows()[1]->web_contents(),
             JsReplace("createAttributionSrcImg($1);", register_trigger_url)));

  expected_report.WaitForReport();
}

IN_PROC_BROWSER_TEST_P(
    AttributionsBrowserTest,
    ConversionOnDifferentSubdomainThanLandingPage_ReportSent) {
  // Expected reports must be registered before the server starts.
  ExpectedReportWaiter expected_report(
      GURL("https://a.test/.well-known/attribution-reporting/"
           "report-event-attribution"),
      /*attribution_destination=*/"https://d.test",
      /*source_event_id=*/"5", /*source_type=*/"navigation",
      /*trigger_data=*/"7", https_server());
  ASSERT_TRUE(https_server()->Start());

  GURL impression_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), impression_url));

  GURL conversion_url = https_server()->GetURL(
      "sub.d.test",
      "/attribution_reporting/page_with_conversion_redirect.html");
  GURL register_source_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/register_source_headers.html");

  CreateAndClickSource(web_contents(), conversion_url,
                       register_source_url.spec());

  // Navigate to a same domain origin that is different than the landing page
  // for the click and convert there. A report should still be sent.
  GURL other_conversion_url = https_server()->GetURL(
      "other.d.test",
      "/attribution_reporting/page_with_conversion_redirect.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), other_conversion_url));

  GURL register_trigger_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/register_trigger_headers.html");
  EXPECT_TRUE(ExecJs(web_contents(), JsReplace("createAttributionSrcImg($1);",
                                               register_trigger_url)));

  expected_report.WaitForReport();
}

IN_PROC_BROWSER_TEST_P(
    AttributionsBrowserTest,
    ServiceWorkerPerformsAttributionSrcRedirect_ReporterSet) {
  auto register_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server(), "/attribution_reporting/register_source_redirect");

  ExpectedReportWaiter expected_report(
      GURL("https://c.test/.well-known/attribution-reporting/"
           "report-event-attribution"),
      /*attribution_destination=*/"https://d.test",
      /*source_event_id=*/"5", /*source_type=*/"event", /*trigger_data=*/"1",
      https_server());
  ASSERT_TRUE(https_server()->Start());

  GURL impression_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/page_with_impression_creator.html");

  // Setup our service worker.
  WorkerStateObserver sw_observer(wrapper(), ServiceWorkerVersion::ACTIVATED);
  blink::mojom::ServiceWorkerRegistrationOptions options(
      impression_url, blink::mojom::ScriptType::kClassic,
      blink::mojom::ServiceWorkerUpdateViaCache::kImports);
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(options.scope));
  public_context()->RegisterServiceWorker(
      https_server()->GetURL("a.test",
                             "/attribution_reporting/service_worker.js"),
      key, options,
      base::BindOnce(&ExpectRegisterResultAndRun,
                     blink::ServiceWorkerStatusCode::kOk, base::DoNothing()));
  sw_observer.Wait();

  EXPECT_TRUE(NavigateToURL(web_contents(), impression_url));

  MockAttributionObserver observer;
  base::ScopedObservation<AttributionManager, AttributionObserver> observation(
      &observer);
  observation.Observe(attribution_manager());

  base::RunLoop loop;
  EXPECT_CALL(observer,
              OnSourceHandled(_, _, _, StorableSource::Result::kSuccess))
      .WillOnce([&]() { loop.Quit(); });

  EXPECT_TRUE(ExecJs(
      web_contents(),
      JsReplace(
          "createAttributionSrcImg($1);",
          https_server()->GetURL(
              "a.test", "/attribution_reporting/register_source_redirect"))));

  register_response->WaitForRequest();
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader(
      "Location",
      https_server()
          ->GetURL("c.test",
                   "/attribution_reporting/register_source_headers.html")
          .spec());
  register_response->Send(http_response->ToResponseString());
  register_response->Done();

  // Wait until the source has been stored before registering the trigger;
  // otherwise the trigger could be processed before the source, in which case
  // there would be no matching source: crbug.com/1309173.
  loop.Run();

  GURL conversion_url = https_server()->GetURL(
      "d.test", "/attribution_reporting/page_with_conversion_redirect.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), conversion_url));

  GURL register_trigger_url = https_server()->GetURL(
      "c.test", "/attribution_reporting/register_trigger_headers.html");
  EXPECT_TRUE(ExecJs(web_contents(), JsReplace("createAttributionSrcImg($1);",
                                               register_trigger_url)));

  expected_report.WaitForReport();
}

IN_PROC_BROWSER_TEST_P(
    AttributionsBrowserTest,
    ServiceWorkerPerformsAttributionEligibleRedirect_ReporterSet) {
  auto register_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server(), "/attribution_reporting/register_source_redirect");

  ExpectedReportWaiter expected_report(
      GURL("https://c.test/.well-known/attribution-reporting/"
           "report-event-attribution"),
      /*attribution_destination=*/"https://d.test",
      /*source_event_id=*/"5", /*source_type=*/"event", /*trigger_data=*/"1",
      https_server());
  ASSERT_TRUE(https_server()->Start());

  GURL impression_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/page_with_impression_creator.html");

  // Setup our service worker.
  WorkerStateObserver sw_observer(wrapper(), ServiceWorkerVersion::ACTIVATED);
  blink::mojom::ServiceWorkerRegistrationOptions options(
      impression_url, blink::mojom::ScriptType::kClassic,
      blink::mojom::ServiceWorkerUpdateViaCache::kImports);
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(options.scope));
  public_context()->RegisterServiceWorker(
      https_server()->GetURL("a.test",
                             "/attribution_reporting/service_worker.js"),
      key, options,
      base::BindOnce(&ExpectRegisterResultAndRun,
                     blink::ServiceWorkerStatusCode::kOk, base::DoNothing()));
  sw_observer.Wait();

  EXPECT_TRUE(NavigateToURL(web_contents(), impression_url));

  MockAttributionObserver observer;
  base::ScopedObservation<AttributionManager, AttributionObserver> observation(
      &observer);
  observation.Observe(attribution_manager());

  base::RunLoop loop;
  EXPECT_CALL(observer,
              OnSourceHandled(_, _, _, StorableSource::Result::kSuccess))
      .WillOnce([&]() { loop.Quit(); });

  EXPECT_TRUE(ExecJs(
      web_contents(),
      JsReplace(
          "createAttributionEligibleImgSrc($1);",
          https_server()->GetURL(
              "a.test", "/attribution_reporting/register_source_redirect"))));

  register_response->WaitForRequest();
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader(
      "Location",
      https_server()
          ->GetURL("c.test",
                   "/attribution_reporting/register_source_headers.html")
          .spec());
  register_response->Send(http_response->ToResponseString());
  register_response->Done();

  // Wait until the source has been stored before registering the trigger;
  // otherwise the trigger could be processed before the source, in which case
  // there would be no matching source: crbug.com/1309173.
  loop.Run();

  GURL conversion_url = https_server()->GetURL(
      "d.test", "/attribution_reporting/page_with_conversion_redirect.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), conversion_url));

  GURL register_trigger_url = https_server()->GetURL(
      "c.test", "/attribution_reporting/register_trigger_headers.html");
  EXPECT_TRUE(ExecJs(web_contents(), JsReplace("createAttributionSrcImg($1);",
                                               register_trigger_url)));

  expected_report.WaitForReport();
}

IN_PROC_BROWSER_TEST_P(AttributionsBrowserTest,
                       NavigationNoneSupported_EligibleHeaderNotSet) {
  MockAttributionReportingContentBrowserClientBase<
      ContentBrowserTestContentBrowserClient>
      browser_client;
  EXPECT_CALL(
      browser_client,
      GetAttributionSupport(
          ContentBrowserClient::AttributionReportingOsApiState::kDisabled,
          /*client_os_disabled=*/false))
      .WillRepeatedly(Return(network::mojom::AttributionSupport::kNone));

  auto register_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server(), "/register_source");
  ASSERT_TRUE(https_server()->Start());

  GURL impression_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), impression_url));

  GURL register_source_url =
      https_server()->GetURL("d.test", "/register_source");

  EXPECT_TRUE(ExecJs(web_contents(), JsReplace(R"(
    createAttributionSrcAnchor({id: 'link',
                        url: $1,
                        attributionsrc: '',
                        target: $2});)",
                                               register_source_url, "_top")));
  EXPECT_TRUE(ExecJs(web_contents(), "simulateClick('link');"));

  register_response->WaitForRequest();
  ExpectEmptyAttributionReportingEligibleHeader(
      register_response->http_request()->headers.at(
          "Attribution-Reporting-Eligible"));
  ExpectValidAttributionReportingSupportHeader(
      register_response->http_request()->headers.at(
          "Attribution-Reporting-Support"),
      /*web_expected=*/false,
      /*os_expected=*/false);
}

class AttributionsPrerenderBrowserTest : public AttributionsBrowserTest {
 public:
  AttributionsPrerenderBrowserTest()
      : prerender_helper_(
            base::BindRepeating(&AttributionsBrowserTest::web_contents,
                                base::Unretained(this))) {}
  ~AttributionsPrerenderBrowserTest() override = default;

 protected:
  content::test::PrerenderTestHelper prerender_helper_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         AttributionsPrerenderBrowserTest,
                         ::testing::Bool());

// TODO(crbug.com/40231714): these tests are flaky on most release bots.
#if defined(NDEBUG)
#define ATTRIBUTION_PRERENDER_BROWSER_TEST(TEST_NAME) \
  IN_PROC_BROWSER_TEST_P(AttributionsPrerenderBrowserTest, DISABLED_##TEST_NAME)
#else
#define ATTRIBUTION_PRERENDER_BROWSER_TEST(TEST_NAME) \
  IN_PROC_BROWSER_TEST_P(AttributionsPrerenderBrowserTest, TEST_NAME)
#endif

ATTRIBUTION_PRERENDER_BROWSER_TEST(NoConversionsOnPrerender) {
  const char* kTestCases[] = {"createAttributionSrcImg($1);",
                              "createTrackingPixel($1);"};

  for (const char* registration_js : kTestCases) {
    auto https_server = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server->ServeFilesFromSourceDirectory("content/test/data");

    ExpectedReportWaiter expected_report(
        GURL("https://a.test/.well-known/attribution-reporting/"
             "report-event-attribution"),
        /*attribution_destination=*/"https://d.test",
        /*source_event_id=*/"7", /*source_type=*/"event", /*trigger_data=*/"1",
        https_server.get());
    ASSERT_TRUE(https_server->Start());

    // Navigate to a page with impression creator.
    const GURL kImpressionUrl = https_server->GetURL(
        "a.test", "/attribution_reporting/page_with_impression_creator.html");
    EXPECT_TRUE(NavigateToURL(web_contents(), kImpressionUrl));

    // Register impression for the target conversion url.
    GURL register_url = https_server->GetURL(
        "a.test", "/attribution_reporting/register_source_headers.html");

    EXPECT_TRUE(ExecJs(web_contents(), JsReplace("createAttributionSrcImg($1);",
                                                 register_url)));

    // Navigate to a starting same origin page with the conversion url.
    const GURL kEmptyUrl = https_server->GetURL("d.test", "/empty.html");
    {
      auto url_loader_interceptor =
          content::URLLoaderInterceptor::ServeFilesFromDirectoryAtOrigin(
              kBaseDataDir, kEmptyUrl.DeprecatedGetOriginAsURL());
      EXPECT_TRUE(NavigateToURL(web_contents(), kEmptyUrl));
    }

    // Pre-render the conversion url.
    const GURL kConversionUrl = https_server->GetURL(
        "d.test", "/attribution_reporting/page_with_conversion_redirect.html");
    FrameTreeNodeId host_id = prerender_helper_.AddPrerender(kConversionUrl);
    content::test::PrerenderHostObserver host_observer(*web_contents(),
                                                       host_id);

    prerender_helper_.WaitForPrerenderLoadCompletion(kConversionUrl);
    content::RenderFrameHost* prerender_rfh =
        prerender_helper_.GetPrerenderedMainFrameHost(host_id);

    // Register a conversion with the original page as the reporting origin
    // during pre-rendering.
    const GURL register_trigger_url = https_server->GetURL(
        "a.test", "/attribution_reporting/register_trigger_headers.html");
    EXPECT_TRUE(ExecJs(prerender_rfh,
                       JsReplace(registration_js, register_trigger_url)));

    // Verify that registering a conversion had no effect on reports, as the
    // impressions were never passed to the conversion URL, as the page was only
    // pre-rendered.
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(100));
    run_loop.Run();
    EXPECT_FALSE(expected_report.HasRequest());
  }
}

ATTRIBUTION_PRERENDER_BROWSER_TEST(ConversionsRegisteredOnActivatedPrerender) {
  const char* kTestCases[] = {"createAttributionSrcImg($1);",
                              "createTrackingPixel($1);"};

  ASSERT_TRUE(https_server()->Start());

  for (const char* registration_js : kTestCases) {
    // Navigate to a starting same origin page with the conversion url.
    const GURL kEmptyUrl = https_server()->GetURL("d.test", "/empty.html");
    {
      auto url_loader_interceptor =
          content::URLLoaderInterceptor::ServeFilesFromDirectoryAtOrigin(
              kBaseDataDir, kEmptyUrl.DeprecatedGetOriginAsURL());
      EXPECT_TRUE(NavigateToURL(web_contents(), kEmptyUrl));
    }

    // Pre-render the conversion url.
    const GURL kConversionUrl = https_server()->GetURL(
        "d.test", "/attribution_reporting/page_with_conversion_redirect.html");
    FrameTreeNodeId host_id = prerender_helper_.AddPrerender(kConversionUrl);
    content::test::PrerenderHostObserver host_observer(*web_contents(),
                                                       host_id);

    prerender_helper_.WaitForPrerenderLoadCompletion(kConversionUrl);
    content::RenderFrameHost* prerender_rfh =
        prerender_helper_.GetPrerenderedMainFrameHost(host_id);

    const GURL register_trigger_url = https_server()->GetURL(
        "a.test", "/attribution_reporting/register_trigger_headers.html");
    EXPECT_TRUE(ExecJs(prerender_rfh,
                       JsReplace(registration_js, register_trigger_url)));

    MockAttributionObserver observer;
    base::ScopedObservation<AttributionManager, AttributionObserver>
        observation(&observer);
    observation.Observe(attribution_manager());
    base::RunLoop loop;
    EXPECT_CALL(observer, OnTriggerHandled).WillOnce([&]() { loop.Quit(); });

    // Navigate to pre-rendered page, bringing it to the fore.
    prerender_helper_.NavigatePrimaryPage(kConversionUrl);

    ASSERT_EQ(kConversionUrl, web_contents()->GetLastCommittedURL());
    ASSERT_TRUE(host_observer.was_activated());

    loop.Run();
  }
}

class AttributionsCrossAppWebEnabledBrowserTest
    : public AttributionsBrowserTest {
 public:
  AttributionsCrossAppWebEnabledBrowserTest()
      : AttributionsBrowserTest(
            /*enabled_features=*/{
                network::features::kAttributionReportingCrossAppWeb}) {}
};

INSTANTIATE_TEST_SUITE_P(All,
                         AttributionsCrossAppWebEnabledBrowserTest,
                         ::testing::Bool());

IN_PROC_BROWSER_TEST_P(AttributionsCrossAppWebEnabledBrowserTest,
                       AttributionEligibleNavigation_SetsSupportHeader) {
  auto register_response1 =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server(), "/register_source_redirect");
  auto register_response2 =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server(), "/register_source_redirect2");
  ASSERT_TRUE(https_server()->Start());

  GURL impression_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), impression_url));

  GURL register_source_url =
      https_server()->GetURL("d.test", "/register_source_redirect");

  // Don't use `CreateAndClickSource()` as we need to observe navigation
  // redirects prior to the navigation finishing.
  EXPECT_TRUE(ExecJs(web_contents(), JsReplace(R"(
    createAttributionSrcAnchor({id: 'link',
                        url: $1,
                        attributionsrc: '',
                        target: $2});)",
                                               register_source_url, "_top")));
  EXPECT_TRUE(ExecJs(web_contents(), "simulateClick('link');"));

  // Verify the navigation redirects contain the support header.
  register_response1->WaitForRequest();
  ExpectValidAttributionReportingSupportHeader(
      register_response1->http_request()->headers.at(
          "Attribution-Reporting-Support"),
      /*web_expected=*/true,
      /*os_expected=*/false);

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader("Location", "/register_source_redirect2");
  register_response1->Send(http_response->ToResponseString());
  register_response1->Done();

  // Ensure that redirect requests also contain the header.
  register_response2->WaitForRequest();
  ExpectValidAttributionReportingSupportHeader(
      register_response2->http_request()->headers.at(
          "Attribution-Reporting-Support"),
      /*web_expected=*/true,
      /*os_expected=*/false);
}

IN_PROC_BROWSER_TEST_P(
    AttributionsCrossAppWebEnabledBrowserTest,
    AttributionEligibleNavigationOsLevelEnabled_SetsSupportHeader) {
  auto register_response1 =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server(), "/register_source_redirect");
  auto register_response2 =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server(), "/register_source_redirect2");
  ASSERT_TRUE(https_server()->Start());

  AttributionOsLevelManager::ScopedApiStateForTesting scoped_api_state_setting(
      AttributionOsLevelManager::ApiState::kEnabled);

  GURL impression_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), impression_url));

  GURL register_source_url =
      https_server()->GetURL("d.test", "/register_source_redirect");

  // Don't use `CreateAndClickSource()` as we need to observe navigation
  // redirects prior to the navigation finishing.
  EXPECT_TRUE(ExecJs(web_contents(), JsReplace(R"(
    createAttributionSrcAnchor({id: 'link',
                        url: $1,
                        attributionsrc: '',
                        target: $2});)",
                                               register_source_url, "_top")));
  EXPECT_TRUE(ExecJs(web_contents(), "simulateClick('link');"));

  // Verify the navigation redirects contain the support header.
  register_response1->WaitForRequest();
  ExpectValidAttributionReportingSupportHeader(
      register_response1->http_request()->headers.at(
          "Attribution-Reporting-Support"),
      /*web_expected=*/true,
      /*os_expected=*/true);

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader("Location", "/register_source_redirect2");
  register_response1->Send(http_response->ToResponseString());
  register_response1->Done();

  // Ensure that redirect requests also contain the header.
  register_response2->WaitForRequest();
  ExpectValidAttributionReportingSupportHeader(
      register_response2->http_request()->headers.at(
          "Attribution-Reporting-Support"),
      /*web_expected=*/true,
      /*os_expected=*/true);
}

class AttributionsFencedFrameBrowserTest : public AttributionsBrowserTest {
 public:
  AttributionsFencedFrameBrowserTest()
      : AttributionsBrowserTest(
            /*enabled_features=*/{blink::features::kFencedFrames,
                                  features::kPrivacySandboxAdsAPIsOverride,
                                  blink::features::kFencedFramesAPIChanges,
                                  blink::features::kFencedFramesDefaultMode}) {}

  FrameTreeNode* AddFencedFrame(
      FrameTreeNode* root,
      const GURL& fenced_frame_url,
      scoped_refptr<FencedFrameReporter> fenced_frame_reporter) {
    static constexpr char kAddFencedFrameScript[] = R"({
        var f = document.createElement('fencedframe');
        document.body.appendChild(f);
    })";
    EXPECT_TRUE(ExecJs(root, kAddFencedFrameScript));

    EXPECT_EQ(1U, root->child_count());
    FrameTreeNode* fenced_frame_root_node =
        GetFencedFrameRootNode(root->child_at(0));
    EXPECT_TRUE(fenced_frame_root_node->IsFencedFrameRoot());
    EXPECT_TRUE(fenced_frame_root_node->IsInFencedFrameTree());

    // Get the urn mapping object.
    FencedFrameURLMapping& url_mapping =
        root->current_frame_host()->GetPage().fenced_frame_urls_map();

    // Add url and its reporting metadata to fenced frame url mapping.
    auto urn_uuid = AddAndVerifyFencedFrameURL(
        &url_mapping, fenced_frame_url, std::move(fenced_frame_reporter));

    TestFrameNavigationObserver observer(
        fenced_frame_root_node->current_frame_host());

    // Navigate the fenced frame.
    EXPECT_TRUE(ExecJs(
        root, JsReplace("f.config = new FencedFrameConfig($1);", urn_uuid)));

    observer.WaitForCommit();

    return fenced_frame_root_node;
  }

  scoped_refptr<FencedFrameReporter> CreateFencedFrameReporter() {
    return FencedFrameReporter::CreateForFledge(
        web_contents()
            ->GetPrimaryMainFrame()
            ->GetStoragePartition()
            ->GetURLLoaderFactoryForBrowserProcess(),
        web_contents()->GetBrowserContext(),
        /*direct_seller_is_seller=*/false,
        PrivateAggregationManager::GetManager(
            *web_contents()->GetBrowserContext()),
        /*main_frame_origin=*/
        web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin(),
        /*winner_origin=*/url::Origin::Create(GURL("https://a.test")),
        /*winner_aggregation_coordinator_origin=*/std::nullopt);
  }

 private:
  GURL AddAndVerifyFencedFrameURL(
      FencedFrameURLMapping* fenced_frame_url_mapping,
      const GURL& https_url,
      scoped_refptr<FencedFrameReporter> fenced_frame_reporter) {
    std::optional<GURL> urn_uuid =
        fenced_frame_url_mapping->AddFencedFrameURLForTesting(
            https_url, std::move(fenced_frame_reporter));
    EXPECT_TRUE(urn_uuid.has_value());
    EXPECT_TRUE(urn_uuid->is_valid());
    return urn_uuid.value();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         AttributionsFencedFrameBrowserTest,
                         ::testing::Bool());

IN_PROC_BROWSER_TEST_P(AttributionsFencedFrameBrowserTest,
                       ReportEvent_ReportSent) {
  // Expected reports must be registered before the server starts.
  ExpectedReportWaiter expected_report(
      GURL("https://a.test/.well-known/attribution-reporting/"
           "report-event-attribution"),
      /*attribution_destination=*/"https://a.test",
      /*source_event_id=*/"5", /*source_type=*/"event",
      /*trigger_data=*/"1", https_server());
  ASSERT_TRUE(https_server()->Start());

  GURL main_url =
      https_server()->GetURL("a.test", "/page_with_impression_creator.html");
  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  GURL fenced_frame_url(
      https_server()->GetURL("a.test", "/page_with_impression_creator.html"));

  GURL reporting_url = https_server()->GetURL(
      "a.test", "/register_source_headers_trigger_same_origin.html");

  GURL buyer_url = https_server()->GetURL("c.test", "/");

  scoped_refptr<FencedFrameReporter> fenced_frame_reporter =
      CreateFencedFrameReporter();
  // Set valid reporting metadata for buyer.
  fenced_frame_reporter->OnUrlMappingReady(
      blink::FencedFrame::ReportingDestination::kBuyer,
      url::Origin::Create(GURL(buyer_url)), {{"click", reporting_url}});

  FrameTreeNode* fenced_frame_root_node =
      AddFencedFrame(root, fenced_frame_url, std::move(fenced_frame_reporter));

  MockAttributionObserver observer;
  base::ScopedObservation<AttributionManager, AttributionObserver> observation(
      &observer);
  observation.Observe(attribution_manager());

  base::RunLoop loop;
  EXPECT_CALL(observer,
              OnSourceHandled(_, _, _, StorableSource::Result::kSuccess))
      .WillOnce([&]() { loop.Quit(); });

  ASSERT_TRUE(ExecJs(fenced_frame_root_node, R"(
        window.fence.reportEvent({
          eventType: 'click',
          eventData: 'this is a click',
          destination: ['buyer'],
        });
      )"));
  loop.Run();

  ASSERT_TRUE(ExecJs(root, JsReplace("createAttributionSrcImg($1);",
                                     https_server()->GetURL(
                                         "a.test",
                                         "/attribution_reporting/"
                                         "register_trigger_headers.html"))));

  expected_report.WaitForReport();
}

IN_PROC_BROWSER_TEST_P(AttributionsFencedFrameBrowserTest,
                       ReportEventRedirect_BothReportsSent) {
  MockAttributionObserver attribution_manager_observer;
  base::ScopedObservation<AttributionManager, AttributionObserver> observation(
      &attribution_manager_observer);
  observation.Observe(attribution_manager());

  base::RunLoop loop;

  auto register_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server(), "/register_source_redirect");

  // Expected reports must be registered before the server starts.
  ExpectedReportWaiter expected_report(
      GURL("https://a.test/.well-known/attribution-reporting/"
           "report-event-attribution"),
      /*attribution_destination=*/"https://a.test",
      /*source_event_id=*/"1", /*source_type=*/"event",
      /*trigger_data=*/"1", https_server());

  ExpectedReportWaiter expected_report2(
      GURL("https://b.test/.well-known/attribution-reporting/"
           "report-event-attribution"),
      /*attribution_destination=*/"https://a.test",
      /*source_event_id=*/"5", /*source_type=*/"event",
      /*trigger_data=*/"1", https_server());

  ASSERT_TRUE(https_server()->Start());

  GURL main_url =
      https_server()->GetURL("a.test", "/page_with_impression_creator.html");
  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  GURL fenced_frame_url(
      https_server()->GetURL("a.test", "/page_with_impression_creator.html"));

  GURL reporting_url =
      https_server()->GetURL("a.test", "/register_source_redirect");

  GURL buyer_url = https_server()->GetURL("c.test", "/");

  scoped_refptr<FencedFrameReporter> fenced_frame_reporter =
      CreateFencedFrameReporter();
  // Set valid reporting metadata for buyer.
  fenced_frame_reporter->OnUrlMappingReady(
      blink::FencedFrame::ReportingDestination::kBuyer,
      url::Origin::Create(buyer_url), {{"click", reporting_url}});

  FrameTreeNode* fenced_frame_root_node =
      AddFencedFrame(root, fenced_frame_url, std::move(fenced_frame_reporter));

  // Perform the reportEvent call, with a unique body. "this is a click");
  ASSERT_TRUE(ExecJs(fenced_frame_root_node, R"(
        window.fence.reportEvent({
          eventType: 'click',
          eventData: 'this is a click',
          destination: ['buyer'],
        });
      )"));

  register_response->WaitForRequest();
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader(
      kAttributionReportingRegisterSourceHeader,
      R"({"source_event_id":"1","destination":"https://a.test"})");

  http_response->AddCustomHeader(
      "Location",
      https_server()
          ->GetURL("b.test",
                   "/register_source_headers_trigger_same_origin.html")
          .spec());
  http_response->AddCustomHeader("Access-Control-Allow-Origin", "*");
  register_response->Send(http_response->ToResponseString());
  register_response->Done();

  // We wait for the 2 sources to be processed before registering triggers.
  EXPECT_CALL(attribution_manager_observer,
              OnSourceHandled(_, _, _, StorableSource::Result::kSuccess))
      .WillOnce([]() {})
      .WillOnce([&loop]() { loop.Quit(); });
  loop.Run();

  GURL register_trigger_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/register_trigger_headers.html");
  ASSERT_TRUE(ExecJs(
      root, JsReplace("createAttributionSrcImg($1);", register_trigger_url)));
  expected_report.WaitForReport();

  GURL register_trigger_url2 = https_server()->GetURL(
      "b.test", "/attribution_reporting/register_trigger_headers.html");
  ASSERT_TRUE(ExecJs(
      root, JsReplace("createAttributionSrcImg($1);", register_trigger_url2)));
  expected_report2.WaitForReport();
}

IN_PROC_BROWSER_TEST_P(AttributionsFencedFrameBrowserTest,
                       AutomaticBeacon_ReportSent) {
  // Expected reports must be registered before the server starts.
  ExpectedReportWaiter expected_report(
      GURL("https://a.test/.well-known/attribution-reporting/"
           "report-event-attribution"),
      /*attribution_destination=*/"https://a.test",
      /*source_event_id=*/"5", /*source_type=*/"navigation",
      /*trigger_data=*/"7", https_server());
  ASSERT_TRUE(https_server()->Start());

  GURL main_url =
      https_server()->GetURL("a.test", "/page_with_impression_creator.html");
  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  TestFrameNavigationObserver root_observer(root);
  GURL fenced_frame_url(
      https_server()->GetURL("a.test", "/page_with_impression_creator.html"));

  GURL reporting_url = https_server()->GetURL(
      "a.test", "/register_source_headers_trigger_same_origin.html");

  GURL buyer_url = https_server()->GetURL("c.test", "/");

  scoped_refptr<FencedFrameReporter> fenced_frame_reporter =
      CreateFencedFrameReporter();
  // Set valid reporting metadata for buyer.
  fenced_frame_reporter->OnUrlMappingReady(
      blink::FencedFrame::ReportingDestination::kBuyer,
      url::Origin::Create(GURL(buyer_url)),
      {{blink::kDeprecatedFencedFrameTopNavigationBeaconType, reporting_url}});

  FrameTreeNode* fenced_frame_root_node =
      AddFencedFrame(root, fenced_frame_url, std::move(fenced_frame_reporter));

  ASSERT_TRUE(
      ExecJs(fenced_frame_root_node,
             JsReplace(R"(
    window.fence.setReportEventDataForAutomaticBeacons({
      eventType: $1,
      eventData: 'This is the event data!',
      destination: ['buyer']
    });
    )",
                       blink::kDeprecatedFencedFrameTopNavigationBeaconType)));

  GURL navigation_url(
      https_server()->GetURL("a.test", "/page_with_impression_creator.html"));

  ASSERT_TRUE(
      ExecJs(fenced_frame_root_node,
             JsReplace("window.open($1, '_unfencedTop');", navigation_url)));

  // The page must fully load before it can do anything involving attribution
  // reporting.
  root_observer.Wait();

  ASSERT_TRUE(ExecJs(root, JsReplace("createAttributionSrcImg($1);",
                                     https_server()->GetURL(
                                         "a.test",
                                         "/attribution_reporting/"
                                         "register_trigger_headers.html"))));

  expected_report.WaitForReport();
}

class AttributionsBrowserTestWithKeepAliveMigration
    : public AttributionsBrowserTestBase {
 public:
  AttributionsBrowserTestWithKeepAliveMigration() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kKeepAliveInBrowserMigration,
         blink::features::kAttributionReportingInBrowserMigration},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Regression test for crbug.com/1374121.
IN_PROC_BROWSER_TEST_F(AttributionsBrowserTestWithKeepAliveMigration,
                       SourceRegisteredAfterNavigation) {
  auto register_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server(), "/register_source");

  ExpectedReportWaiter expected_report(
      GURL("https://d.test/.well-known/attribution-reporting/"
           "report-event-attribution"),
      /*attribution_destination=*/"https://d.test",
      /*source_event_id=*/"1", /*source_type=*/"navigation",
      /*trigger_data=*/"7", https_server());
  ASSERT_TRUE(https_server()->Start());

  GURL impression_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/page_with_impression_creator.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), impression_url));

  GURL register_source_url =
      https_server()->GetURL("d.test", "/register_source");

  GURL conversion_url = https_server()->GetURL(
      "d.test", "/attribution_reporting/page_with_conversion_redirect.html");

  ASSERT_TRUE(
      ExecJs(web_contents(), JsReplace(R"(
    createAttributionSrcAnchor({id: 'link',
                        url: $1,
                        attributionsrc: $2,
                        target: '_top'});)",
                                       conversion_url, register_source_url)));

  TestNavigationObserver observer(web_contents());
  ASSERT_TRUE(ExecJs(web_contents(), "simulateClick('link');"));

  // Wait for navigation to complete before registering the source.
  observer.Wait();

  register_response->WaitForRequest();
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader(
      "Attribution-Reporting-Register-Source",
      R"({"source_event_id":"1","destination":"https://d.test"})");
  register_response->Send(http_response->ToResponseString());
  register_response->Done();

  GURL register_trigger_url = https_server()->GetURL(
      "d.test", "/attribution_reporting/register_trigger_headers.html");
  ASSERT_TRUE(ExecJs(web_contents(), JsReplace("createAttributionSrcImg($1);",
                                               register_trigger_url)));

  expected_report.WaitForReport();
}

void TestServiceWorker(const char* registration_js,
                       WebContents* web_contents,
                       ServiceWorkerContextWrapper* sw_wrapper,
                       net::EmbeddedTestServer* https_server) {
  auto register_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server, "/attribution_reporting/register_source");
  ASSERT_TRUE(https_server->Start());

  GURL page_url = https_server->GetURL(
      "a.test", "/attribution_reporting/page_with_impression_creator.html");

  // Setup our service worker.
  WorkerStateObserver sw_observer(sw_wrapper, ServiceWorkerVersion::ACTIVATED);
  blink::mojom::ServiceWorkerRegistrationOptions options(
      page_url, blink::mojom::ScriptType::kClassic,
      blink::mojom::ServiceWorkerUpdateViaCache::kImports);
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(options.scope));
  sw_wrapper->RegisterServiceWorker(
      https_server->GetURL("a.test",
                           "/attribution_reporting/service_worker.js"),
      key, options,
      base::BindOnce(&ExpectRegisterResultAndRun,
                     blink::ServiceWorkerStatusCode::kOk, base::DoNothing()));
  sw_observer.Wait();

  EXPECT_TRUE(NavigateToURL(web_contents, page_url));

  EXPECT_TRUE(ExecJs(
      web_contents,
      JsReplace(registration_js,
                https_server->GetURL(
                    "a.test", "/attribution_reporting/register_source"))));

  register_response->WaitForRequest();
  EXPECT_TRUE(base::Contains(register_response->http_request()->headers,
                             "Attribution-Reporting-Eligible"));
  EXPECT_TRUE(base::Contains(register_response->http_request()->headers,
                             "Attribution-Reporting-Support"));
}

IN_PROC_BROWSER_TEST_P(
    AttributionsBrowserTest,
    ServiceWorkerPerformsAttributionSrcRegistration_SupportHeaderSet_createAttributionEligibleImgSrc) {
  TestServiceWorker("createAttributionEligibleImgSrc($1);", web_contents(),
                    wrapper(), https_server());
}
IN_PROC_BROWSER_TEST_P(
    AttributionsBrowserTest,
    ServiceWorkerPerformsAttributionSrcRegistration_SupportHeaderSet_createAttributionSrcScript) {
  TestServiceWorker("createAttributionSrcScript($1);", web_contents(),
                    wrapper(), https_server());
}
IN_PROC_BROWSER_TEST_P(
    AttributionsBrowserTest,
    ServiceWorkerPerformsAttributionSrcRegistration_SupportHeaderSet_doAttributionEligibleFetch) {
  TestServiceWorker("doAttributionEligibleFetch($1);", web_contents(),
                    wrapper(), https_server());
}

IN_PROC_BROWSER_TEST_P(
    AttributionsBrowserTest,
    ServiceWorkerPerformsAttributionSrcRegistration_SupportHeaderSet_doAttributionEligibleXHR) {
  TestServiceWorker("doAttributionEligibleXHR($1);", web_contents(), wrapper(),
                    https_server());
}

IN_PROC_BROWSER_TEST_P(
    AttributionsBrowserTest,
    ServiceWorkerPerformsAttributionSrcRegistration_SupportHeaderSet_createAttributionEligibleScriptSrc) {
  TestServiceWorker("createAttributionEligibleScriptSrc($1);", web_contents(),
                    wrapper(), https_server());
}

}  // namespace content
