// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/buildflag.h"
#include "components/attribution_reporting/os_support.mojom.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_manager_impl.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_core_observer.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/test/test_content_browser_client.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

using ::testing::_;
using ::testing::Return;

constexpr char kBaseDataDir[] = "content/test/data/";

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
                             base::DictionaryValue(),
                             server) {
    expected_body.SetStringKey("attribution_destination",
                               std::move(attribution_destination));
    expected_body.SetStringKey("source_event_id", std::move(source_event_id));
    expected_body.SetStringKey("source_type", std::move(source_type));
    expected_body.SetStringKey("trigger_data", std::move(trigger_data));
  }

  // ControllableHTTPResponses can only wait for relative urls, so only supply
  // the path.
  ExpectedReportWaiter(GURL report_url,
                       base::Value body,
                       net::EmbeddedTestServer* server)
      : expected_url(std::move(report_url)),
        expected_body(std::move(body)),
        response(std::make_unique<net::test_server::ControllableHttpResponse>(
            server,
            expected_url.path())) {}

  GURL expected_url;
  base::Value expected_body;
  std::string source_debug_key;
  std::string trigger_debug_key;
  std::unique_ptr<net::test_server::ControllableHttpResponse> response;

  bool HasRequest() { return !!response->http_request(); }

  // Waits for a report to be received matching the report url. Verifies that
  // the report url and report body were set correctly.
  void WaitForReport() {
    if (!response->http_request())
      response->WaitForRequest();

    // The embedded test server resolves all urls to 127.0.0.1, so get the real
    // request host from the request headers.
    const net::test_server::HttpRequest& request = *response->http_request();
    DCHECK(base::Contains(request.headers, "Host"));
    const GURL& request_url = request.GetURL();
    GURL header_url = GURL("https://" + request.headers.at("Host"));
    std::string host = header_url.host();
    GURL::Replacements replace_host;
    replace_host.SetHostStr(host);

    base::Value body = base::test::ParseJson(request.content);
    EXPECT_THAT(body, base::test::DictionaryHasValues(expected_body));
    const base::Value::Dict& body_dict = body.GetDict();

    // The report ID is random, so just test that the field exists here and is a
    // valid GUID.
    const std::string* report_id = body_dict.FindString("report_id");
    ASSERT_TRUE(report_id);
    EXPECT_TRUE(base::GUID::ParseLowercase(*report_id).is_valid());

    EXPECT_TRUE(body_dict.FindDouble("randomized_trigger_rate"));

    if (source_debug_key.empty()) {
      EXPECT_FALSE(body_dict.FindString("source_debug_key"));
    } else {
      base::ExpectDictStringValue(source_debug_key, body_dict,
                                  "source_debug_key");
    }

    if (trigger_debug_key.empty()) {
      EXPECT_FALSE(body_dict.FindString("trigger_debug_key"));
    } else {
      base::ExpectDictStringValue(trigger_debug_key, body_dict,
                                  "trigger_debug_key");
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

struct ExpectedDebugReportWaiter {
  ExpectedDebugReportWaiter(GURL report_url,
                            std::string expected_body_serialized,
                            net::EmbeddedTestServer* server)
      : expected_url(std::move(report_url)),
        expected_body_serialized(std::move(expected_body_serialized)),
        response(std::make_unique<net::test_server::ControllableHttpResponse>(
            server,
            expected_url.path())) {}

  GURL expected_url;
  std::string expected_body_serialized;

  std::unique_ptr<net::test_server::ControllableHttpResponse> response;

  // Waits for a report to be received matching the report url. Verifies that
  // the report url and report body were set correctly.
  void WaitForReport() {
    if (!response->http_request())
      response->WaitForRequest();

    // The embedded test server resolves all urls to 127.0.0.1, so get the real
    // request host from the request headers.
    const net::test_server::HttpRequest& request = *response->http_request();
    DCHECK(base::Contains(request.headers, "Host"));
    const GURL& request_url = request.GetURL();
    GURL header_url = GURL("https://" + request.headers.at("Host"));
    std::string host = header_url.host();
    GURL::Replacements replace_host;
    replace_host.SetHostStr(host);

    EXPECT_EQ(base::test::ParseJson(request.content),
              base::test::ParseJson(expected_body_serialized));

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

class AttributionsBrowserTest : public ContentBrowserTest {
 public:
  AttributionsBrowserTest() = default;

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
                OnSourceHandled(_, _, StorableSource::Result::kSuccess))
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
                OnSourceHandled(_, _, StorableSource::Result::kSuccess))
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
};

// Verifies that storage initialization does not hang when initialized in a
// browsertest context, see https://crbug.com/1080764).
IN_PROC_BROWSER_TEST_F(AttributionsBrowserTest,
                       FeatureEnabled_StorageInitWithoutHang) {}

IN_PROC_BROWSER_TEST_F(AttributionsBrowserTest,
                       ImpressionConversion_ReportSent) {
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

  // Create an anchor tag with impression attributes and click the link. By
  // default the target is set to "_top".
  GURL conversion_url = https_server()->GetURL(
      "d.test", "/attribution_reporting/page_with_conversion_redirect.html");
  GURL register_source_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/register_source_headers.html");

  CreateAndClickSource(web_contents(), conversion_url,
                       register_source_url.spec());

  GURL register_trigger_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/register_trigger_headers.html");

  EXPECT_TRUE(ExecJs(web_contents(), JsReplace("createAttributionSrcImg($1);",
                                               register_trigger_url)));

  expected_report.WaitForReport();
}

IN_PROC_BROWSER_TEST_F(AttributionsBrowserTest,
                       ImpressionNavigationRedirect_ReportSent) {
  auto register_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server(), "/register_source_redirect");

  // Expected reports must be registered before the server starts.
  ExpectedReportWaiter expected_report(
      GURL("https://d.test/.well-known/attribution-reporting/"
           "report-event-attribution"),
      /*attribution_destination=*/"https://c.test",
      /*source_event_id=*/"1", /*source_type=*/"navigation",
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
      "Attribution-Reporting-Register-Source",
      R"({"source_event_id":"1","destination":"https://c.test"})");

  http_response->AddCustomHeader(
      "Location",
      https_server()
          ->GetURL("c.test",
                   "/attribution_reporting/page_with_conversion_redirect.html")
          .spec());
  register_response->Send(http_response->ToResponseString());
  register_response->Done();

  // Wait for navigation to complete.
  observer.Wait();

  GURL register_trigger_url = https_server()->GetURL(
      "d.test", "/attribution_reporting/register_trigger_headers.html");
  EXPECT_TRUE(ExecJs(web_contents(), JsReplace("createAttributionSrcImg($1);",
                                               register_trigger_url)));

  expected_report.WaitForReport();
}

IN_PROC_BROWSER_TEST_F(AttributionsBrowserTest,
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
      "Attribution-Reporting-Register-Source",
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
      "Attribution-Reporting-Register-Source",
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

IN_PROC_BROWSER_TEST_F(AttributionsBrowserTest,
                       ImpressionNavigationRedirectWindowOpen_ReportSent) {
  // Expected reports must be registered before the server starts.
  ExpectedReportWaiter expected_report(
      GURL("https://d.test/.well-known/attribution-reporting/"
           "report-event-attribution"),
      /*attribution_destination=*/"https://d.test",
      /*source_event_id=*/"1", /*source_type=*/"navigation",
      /*trigger_data=*/"7", https_server());
  ASSERT_TRUE(https_server()->Start());

  GURL impression_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), impression_url));

  // Create an anchor tag with impression attributes and click the link. By
  // default the target is set to "_top".
  GURL register_source_url = https_server()->GetURL(
      "d.test",
      "/attribution_reporting/register_source_navigation_redirect.html");

  TestNavigationObserver observer(web_contents());
  EXPECT_TRUE(ExecJs(web_contents(), JsReplace(R"(window.open($1, '_top',
      "attributionsrc="+$2);)",
                                               register_source_url, "")));
  observer.Wait();

  GURL register_trigger_url = https_server()->GetURL(
      "d.test", "/attribution_reporting/register_trigger_headers.html");

  EXPECT_TRUE(ExecJs(web_contents(), JsReplace("createAttributionSrcImg($1);",
                                               register_trigger_url)));

  expected_report.WaitForReport();
}

IN_PROC_BROWSER_TEST_F(AttributionsBrowserTest,
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
  EXPECT_EQ(register_response1->http_request()->headers.at(
                "Attribution-Reporting-Eligible"),
            "navigation-source");
  EXPECT_FALSE(base::Contains(register_response1->http_request()->headers,
                              "Attribution-Reporting-Support"));

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader("Location", "/register_source_redirect2");
  register_response1->Send(http_response->ToResponseString());
  register_response1->Done();

  // Ensure that redirect requests also contain the header.
  register_response2->WaitForRequest();
  ASSERT_EQ(register_response2->http_request()->headers.at(
                "Attribution-Reporting-Eligible"),
            "navigation-source");
  ASSERT_FALSE(base::Contains(register_response2->http_request()->headers,
                              "Attribution-Reporting-Support"));
}

IN_PROC_BROWSER_TEST_F(AttributionsBrowserTest,
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

IN_PROC_BROWSER_TEST_F(AttributionsBrowserTest,
                       WindowOpenDeprecatedAPI_NoException) {
  // Expected reports must be registered before the server starts.
  ExpectedReportWaiter expected_report(
      GURL("https://a.test/.well-known/attribution-reporting/"
           "report-event-attribution"),
      /*body=*/base::Value(), https_server());
  ASSERT_TRUE(https_server()->Start());

  GURL impression_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), impression_url));

  // Create an anchor tag with impression attributes and click the link. By
  // default the target is set to "_top".
  GURL conversion_url = https_server()->GetURL(
      "b.test", "/attribution_reporting/page_with_conversion_redirect.html");
  TestNavigationObserver observer(web_contents());
  EXPECT_TRUE(
      ExecJs(web_contents(),
             JsReplace(R"(window.open($1, '_top', '',
               {attributionSourceEventId: '1', attributeOn: $2});)",
                       conversion_url, url::Origin::Create(conversion_url))));
  observer.Wait();

  GURL register_trigger_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/register_trigger_headers.html");

  EXPECT_TRUE(ExecJs(web_contents(), JsReplace("createAttributionSrcImg($1);",
                                               register_trigger_url)));

  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(100));
  run_loop.Run();
  EXPECT_FALSE(expected_report.HasRequest());
}

IN_PROC_BROWSER_TEST_F(AttributionsBrowserTest,
                       WindowOpenImpressionConversion_ReportSent) {
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

  GURL register_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/register_source_headers.html");

  GURL conversion_url = https_server()->GetURL(
      "d.test", "/attribution_reporting/page_with_conversion_redirect.html");

  TestNavigationObserver observer(web_contents());
  EXPECT_TRUE(ExecJs(web_contents(), JsReplace(R"(window.open($1, '_top',
      "attributionsrc="+$2);)",
                                               conversion_url, register_url)));
  observer.Wait();

  GURL register_trigger_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/register_trigger_headers.html");
  EXPECT_TRUE(ExecJs(web_contents(), JsReplace("createAttributionSrcImg($1);",
                                               register_trigger_url)));

  expected_report.WaitForReport();
}

IN_PROC_BROWSER_TEST_F(AttributionsBrowserTest,
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
IN_PROC_BROWSER_TEST_F(AttributionsBrowserTest,
                       AttributionSrcInSandboxedIframe_NoCrash) {
  ExpectedReportWaiter expected_report(
      GURL("https://a.test/.well-known/attribution-reporting/"
           "report-event-attribution"),
      /*attribution_destination=*/"https://a.test",
      /*source_event_id=*/"5", /*source_type=*/"event",
      /*trigger_data=*/"1", https_server());
  ASSERT_TRUE(https_server()->Start());

  GURL page_url = https_server()->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), page_url));

  GURL register_source_url = https_server()->GetURL(
      "a.test",
      "/attribution_reporting/"
      "register_source_headers_trigger_same_origin.html");

  GURL register_trigger_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/register_trigger_headers.html");

  // Setting the frame's sandbox attribute causes its origin to be opaque.
  ASSERT_TRUE(
      ExecJs(shell(), JsReplace(R"(
    let frame = document.getElementById('test_iframe');
    frame.setAttribute('sandbox', '');

    frame.setAttribute('srcdoc', `
      <img attributionsrc=$1>
      <img attributionsrc=$2>
    `);
  )",
                                register_source_url, register_trigger_url)));

  expected_report.WaitForReport();
}

IN_PROC_BROWSER_TEST_F(AttributionsBrowserTest,
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

// TODO(https://crbug.com/1374121): This is failing flakily because clicking the
// link will cause a navigation. It is possible that the navigation will
// complete before the attributionsSrc resource has been received. In this case,
// the browser will mark the page as "frozen", causing MojoURLLoaderClient to
// store the message and never dispatch it.
IN_PROC_BROWSER_TEST_F(AttributionsBrowserTest,
                       DISABLED_ImpressionConversionSameDomain_ReportSent) {
  // Expected reports must be registered before the server starts.
  ExpectedReportWaiter expected_report(
      GURL("https://d.test/.well-known/attribution-reporting/"
           "report-event-attribution"),
      /*attribution_destination=*/"https://d.test",
      /*source_event_id=*/"5", /*source_type=*/"navigation",
      /*trigger_data=*/"7", https_server());
  ASSERT_TRUE(https_server()->Start());

  GURL impression_url = https_server()->GetURL(
      "d.test", "/attribution_reporting/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), impression_url));

  GURL conversion_url = https_server()->GetURL(
      "d.test", "/attribution_reporting/page_with_conversion_redirect.html");
  GURL register_source_url = https_server()->GetURL(
      "d.test", "/attribution_reporting/register_source_headers.html");

  CreateAndClickSource(web_contents(), conversion_url,
                       register_source_url.spec());

  GURL register_trigger_url = https_server()->GetURL(
      "d.test", "/attribution_reporting/register_trigger_headers.html");
  EXPECT_TRUE(ExecJs(web_contents(), JsReplace("createAttributionSrcImg($1);",
                                               register_trigger_url)));

  expected_report.WaitForReport();
}

IN_PROC_BROWSER_TEST_F(
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

IN_PROC_BROWSER_TEST_F(
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
  blink::StorageKey key(url::Origin::Create(options.scope));
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
  EXPECT_CALL(observer, OnSourceHandled(_, _, StorableSource::Result::kSuccess))
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

IN_PROC_BROWSER_TEST_F(
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
  blink::StorageKey key(url::Origin::Create(options.scope));
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
  EXPECT_CALL(observer, OnSourceHandled(_, _, StorableSource::Result::kSuccess))
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

IN_PROC_BROWSER_TEST_F(AttributionsBrowserTest,
                       EventSourceWithDebugKeyConversion_ReportSent) {
  // Expected reports must be registered before the server starts.
  ExpectedReportWaiter expected_report(
      GURL("https://a.test/.well-known/attribution-reporting/"
           "report-event-attribution"),
      /*attribution_destination=*/"https://b.test",
      /*source_event_id=*/"5", /*source_type=*/"event", /*trigger_data=*/"1",
      https_server());
  expected_report.source_debug_key = "789";
  ASSERT_TRUE(https_server()->Start());

  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL(
          "a.test", "/set-cookie?ar_debug=1;HttpOnly;Secure;SameSite=None")));

  GURL impression_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), impression_url));

  RegisterSource(https_server()->GetURL(
      "a.test",
      "/attribution_reporting/register_source_headers_debug_key.html"));

  GURL conversion_url = https_server()->GetURL(
      "b.test", "/attribution_reporting/page_with_conversion_redirect.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), conversion_url));

  GURL register_trigger_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/register_trigger_headers.html");
  EXPECT_TRUE(ExecJs(web_contents(), JsReplace("createAttributionSrcImg($1);",
                                               register_trigger_url)));

  expected_report.WaitForReport();
}

IN_PROC_BROWSER_TEST_F(AttributionsBrowserTest,
                       SourceAndDebugCookieRegisteredInSameResponse) {
  // Expected reports must be registered before the server starts.
  ExpectedReportWaiter expected_report(
      GURL("https://a.test/.well-known/attribution-reporting/"
           "report-event-attribution"),
      /*attribution_destination=*/"https://b.test",
      /*source_event_id=*/"5", /*source_type=*/"event", /*trigger_data=*/"1",
      https_server());
  expected_report.source_debug_key = "789";
  ASSERT_TRUE(https_server()->Start());

  GURL impression_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), impression_url));

  RegisterSource(
      https_server()->GetURL("a.test",
                             "/attribution_reporting/"
                             "register_source_headers_debug_key_cookie.html"));

  GURL conversion_url = https_server()->GetURL(
      "b.test", "/attribution_reporting/page_with_conversion_redirect.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), conversion_url));

  GURL register_trigger_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/register_trigger_headers.html");
  EXPECT_TRUE(ExecJs(web_contents(), JsReplace("createAttributionSrcImg($1);",
                                               register_trigger_url)));

  expected_report.WaitForReport();
}

IN_PROC_BROWSER_TEST_F(AttributionsBrowserTest,
                       AttributionSrcSourceAndTrigger_ReportSent) {
  // Expected reports must be registered before the server starts.
  ExpectedReportWaiter expected_report(
      GURL("https://a.test/.well-known/attribution-reporting/"
           "report-event-attribution"),
      /*attribution_destination=*/"https://d.test",
      /*source_event_id=*/"5", /*source_type=*/"event", /*trigger_data=*/"1",
      https_server());
  expected_report.trigger_debug_key = "789";
  ASSERT_TRUE(https_server()->Start());

  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL(
          "a.test", "/set-cookie?ar_debug=1;HttpOnly;Secure;SameSite=None")));

  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL(
          "b.test",
          "/attribution_reporting/page_with_impression_creator.html")));

  RegisterSource(https_server()->GetURL(
      "a.test", "/attribution_reporting/register_source_headers.html"));

  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL(
          "d.test",
          "/attribution_reporting/page_with_impression_creator.html")));

  EXPECT_TRUE(ExecJs(
      web_contents(),
      JsReplace(
          "createAttributionSrcImg($1);",
          https_server()->GetURL("a.test",
                                 "/attribution_reporting/"
                                 "register_trigger_headers_all_params.html"))));

  expected_report.WaitForReport();
}

IN_PROC_BROWSER_TEST_F(AttributionsBrowserTest,
                       AttributionSrcNavigationSourceAndTrigger_ReportSent) {
  // Expected reports must be registered before the server starts.
  ExpectedReportWaiter expected_report(
      GURL("https://a.test/.well-known/attribution-reporting/"
           "report-event-attribution"),
      /*attribution_destination=*/"https://d.test",
      /*source_event_id=*/"5", /*source_type=*/"navigation",
      /*trigger_data=*/"1", https_server());
  ASSERT_TRUE(https_server()->Start());

  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL(
          "b.test",
          "/attribution_reporting/page_with_impression_creator.html")));

  TestNavigationObserver observer(web_contents());

  EXPECT_TRUE(ExecJs(
      web_contents(),
      JsReplace(R"(createAndClickAttributionSrcAnchor({url: $1,
                                      attributionsrc: $2});)",
                https_server()->GetURL(
                    "d.test",
                    "/attribution_reporting/page_with_impression_creator.html"),
                https_server()->GetURL(
                    "a.test",
                    "/attribution_reporting/register_source_headers.html"))));

  observer.Wait();

  EXPECT_TRUE(ExecJs(
      web_contents(),
      JsReplace(
          "createAttributionSrcImg($1);",
          https_server()->GetURL("a.test",
                                 "/attribution_reporting/"
                                 "register_trigger_headers_all_params.html"))));

  expected_report.WaitForReport();
}

IN_PROC_BROWSER_TEST_F(AttributionsBrowserTest,
                       TriggerAndSourceSameRedirectChain_Handled) {
  ASSERT_TRUE(https_server()->Start());

  GURL impression_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), impression_url));

  MockAttributionObserver observer;
  base::ScopedObservation<AttributionManager, AttributionObserver> observation(
      &observer);
  observation.Observe(attribution_manager());

  base::RunLoop loop;
  int count = 0;
  EXPECT_CALL(observer, OnTriggerHandled).WillRepeatedly([&]() {
    count++;
    if (count < 2)
      return;
    loop.Quit();
  });

  bool received_source = false;
  base::RunLoop source_loop;
  EXPECT_CALL(observer, OnSourceHandled).WillOnce([&]() {
    received_source = true;
    source_loop.Quit();
  });

  GURL register_url = https_server()->GetURL(
      "a.test", "/attribution_reporting/register_trigger_source_trigger.html");
  EXPECT_TRUE(
      ExecJs(web_contents(),
             JsReplace("createAttributionEligibleImgSrc($1);", register_url)));

  // Ensure we don't error out processing the redirect chain.
  if (count < 2)
    loop.Run();

  if (!received_source)
    source_loop.Run();
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

// TODO(crbug.com/1344264): these tests are flaky on most release bots.
#if defined(NDEBUG)
#define ATTRIBUTION_PRERENDER_BROWSER_TEST(TEST_NAME) \
  IN_PROC_BROWSER_TEST_F(AttributionsPrerenderBrowserTest, DISABLED_##TEST_NAME)
#else
#define ATTRIBUTION_PRERENDER_BROWSER_TEST(TEST_NAME) \
  IN_PROC_BROWSER_TEST_F(AttributionsPrerenderBrowserTest, TEST_NAME)
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
    int host_id = prerender_helper_.AddPrerender(kConversionUrl);
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

  for (const char* registration_js : kTestCases) {
    auto https_server = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server->ServeFilesFromSourceDirectory("content/test/data");

    ExpectedReportWaiter expected_report(
        GURL("https://a.test/.well-known/attribution-reporting/"
             "report-event-attribution"),
        /*attribution_destination=*/"https://d.test",
        /*source_event_id=*/"5", /*source_type=*/"event", /*trigger_data=*/"1",
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
    int host_id = prerender_helper_.AddPrerender(kConversionUrl);
    content::test::PrerenderHostObserver host_observer(*web_contents(),
                                                       host_id);

    prerender_helper_.WaitForPrerenderLoadCompletion(kConversionUrl);
    content::RenderFrameHost* prerender_rfh =
        prerender_helper_.GetPrerenderedMainFrameHost(host_id);

    const GURL register_trigger_url = https_server->GetURL(
        "a.test", "/attribution_reporting/register_trigger_headers.html");
    EXPECT_TRUE(ExecJs(prerender_rfh,
                       JsReplace(registration_js, register_trigger_url)));

    // Delay prerender activation so that subresource response is received
    // earlier than that.
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(100));
    run_loop.Run();

    // Navigate to pre-rendered page, bringing it to the fore.
    prerender_helper_.NavigatePrimaryPage(kConversionUrl);
    ASSERT_EQ(kConversionUrl, web_contents()->GetLastCommittedURL());
    ASSERT_TRUE(host_observer.was_activated());

    // Confirm that reports work as expected, and impressions were retrieved
    // from the pre-rendered page, once it became a primary page.
    expected_report.WaitForReport();
  }
}

class AttributionsCrossAppWebEnabledBrowserTest
    : public AttributionsBrowserTest {
 public:
  AttributionsCrossAppWebEnabledBrowserTest() = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      blink::features::kAttributionReportingCrossAppWeb};
};

IN_PROC_BROWSER_TEST_F(AttributionsCrossAppWebEnabledBrowserTest,
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
  EXPECT_EQ(register_response1->http_request()->headers.at(
                "Attribution-Reporting-Support"),
            "web");

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader("Location", "/register_source_redirect2");
  register_response1->Send(http_response->ToResponseString());
  register_response1->Done();

  // Ensure that redirect requests also contain the header.
  register_response2->WaitForRequest();
  ASSERT_EQ(register_response2->http_request()->headers.at(
                "Attribution-Reporting-Support"),
            "web");
}

IN_PROC_BROWSER_TEST_F(
    AttributionsCrossAppWebEnabledBrowserTest,
    AttributionEligibleNavigationOsLevelEnabled_SetsSupportHeader) {
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

  AttributionManagerImpl::ScopedOsSupportForTesting scoped_os_support_setting(
      attribution_reporting::mojom::OsSupport::kEnabled);

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
  EXPECT_EQ(register_response1->http_request()->headers.at(
                "Attribution-Reporting-Support"),
            "web, os");

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader("Location", "/register_source_redirect2");
  register_response1->Send(http_response->ToResponseString());
  register_response1->Done();

  // Ensure that redirect requests also contain the header.
  register_response2->WaitForRequest();
  ASSERT_EQ(register_response2->http_request()->headers.at(
                "Attribution-Reporting-Support"),
            "web, os");
}

IN_PROC_BROWSER_TEST_F(AttributionsBrowserTest,
                       NoMatchingSourceDebugReporting_DebugReportSent) {
  // Expected reports must be registered before the server starts.
  ExpectedDebugReportWaiter expected_report(
      GURL("https://a.test/.well-known/attribution-reporting/"
           "debug/verbose"),
      R"json([{
        "body": {
          "attribution_destination": "https://b.test"
        },
        "type": "trigger-no-matching-source"
      }])json",
      https_server());
  ASSERT_TRUE(https_server()->Start());

  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      https_server()->GetURL(
          "a.test", "/set-cookie?ar_debug=1;HttpOnly;Secure;SameSite=None")));

  GURL conversion_url = https_server()->GetURL(
      "b.test", "/attribution_reporting/page_with_conversion_redirect.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), conversion_url));

  GURL register_trigger_url = https_server()->GetURL(
      "a.test",
      "/attribution_reporting/register_trigger_headers_debug_reporting.html");
  EXPECT_TRUE(ExecJs(web_contents(), JsReplace("createAttributionSrcImg($1);",
                                               register_trigger_url)));

  expected_report.WaitForReport();
}

}  // namespace content
