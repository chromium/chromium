// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/containers/contains.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/destination_set.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/os_registration.h"
#include "components/attribution_reporting/registration_eligibility.mojom.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/test_utils.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "content/browser/attribution_reporting/attribution_data_host_manager_impl.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_manager_impl.h"
#include "content/browser/attribution_reporting/attribution_os_level_manager.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/os_registration.h"
#include "content/browser/attribution_reporting/test/mock_attribution_host.h"
#include "content/browser/attribution_reporting/test/mock_attribution_manager.h"
#include "content/browser/attribution_reporting/test/mock_content_browser_client.h"
#include "content/browser/attribution_reporting/test/source_observer.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_throttle.h"
#include "content/public/test/test_navigation_throttle_inserter.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/base/net_errors.h"
#include "net/base/schemeful_site.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/navigation/impression.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

using ::attribution_reporting::DestinationSet;
using ::attribution_reporting::EventTriggerData;
using ::attribution_reporting::SourceRegistration;
using ::attribution_reporting::SuitableOrigin;
using ::attribution_reporting::TriggerRegistration;
using ::attribution_reporting::mojom::RegistrationEligibility;
using ::attribution_reporting::mojom::SourceType;
using ::net::test_server::EmbeddedTestServer;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::Property;
using ::testing::StrictMock;

using attribution_reporting::kAttributionReportingRegisterSourceHeader;

constexpr char kRegistrationMethod[] = "Conversions.RegistrationMethod2";

}  // namespace

class AttributionSrcBrowserTest : public ContentBrowserTest,
                                  public ::testing::WithParamInterface<bool> {
 public:
  explicit AttributionSrcBrowserTest(
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

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    https_server_ = CreateAttributionTestHttpsServer();
    ASSERT_TRUE(https_server_->Start());

    auto mock_manager = std::make_unique<StrictMock<MockAttributionManager>>();
    auto data_host_manager =
        std::make_unique<AttributionDataHostManagerImpl>(mock_manager.get());
    mock_manager->SetDataHostManager(std::move(data_host_manager));
    static_cast<StoragePartitionImpl*>(
        web_contents()->GetBrowserContext()->GetDefaultStoragePartition())
        ->OverrideAttributionManagerForTesting(std::move(mock_manager));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Sets up the blink runtime feature for ConversionMeasurement.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  WebContents* web_contents() { return shell()->web_contents(); }

 protected:
  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  StrictMock<MockAttributionManager>& mock_attribution_manager() {
    return *static_cast<StrictMock<MockAttributionManager>*>(
        AttributionManager::FromWebContents(web_contents()));
  }

  // By default, the attribution_host isn't mocked. A real attribution host will
  // be used and assertions should be on the `mock_attribution_manager`. If a
  // test case wants to ensure that the attribution_host isn't reached, it can
  // setup the mock attribution host with this method.
  void SetupMockAttributionHost() {
    MockAttributionHost::Override(web_contents());
    attribution_host_mocked_ = true;
  }
  MockAttributionHost& mock_attribution_host() {
    CHECK(attribution_host_mocked_);
    AttributionHost* attribution_host =
        AttributionHost::FromWebContents(web_contents());
    return *static_cast<MockAttributionHost*>(attribution_host);
  }

  std::unique_ptr<EmbeddedTestServer> CreateAttributionTestHttpsServer() {
    auto https_server =
        std::make_unique<EmbeddedTestServer>(EmbeddedTestServer::TYPE_HTTPS);

    https_server->SetSSLConfig(EmbeddedTestServer::CERT_TEST_NAMES);
    RegisterDefaultHandlers(https_server.get());
    https_server->ServeFilesFromSourceDirectory(
        "content/test/data/attribution_reporting");
    https_server->ServeFilesFromSourceDirectory("content/test/data");

    return https_server;
  }

 private:
  bool attribution_host_mocked_ = false;
  AttributionManagerImpl::ScopedUseInMemoryStorageForTesting
      attribution_manager_in_memory_setting_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, AttributionSrcBrowserTest, ::testing::Bool());

IN_PROC_BROWSER_TEST_P(AttributionSrcBrowserTest, SourceRegistered) {
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  GURL register_url =
      https_server()->GetURL("c.test", "/register_source_headers.html");

  base::RunLoop run_loop;
  EXPECT_CALL(
      mock_attribution_manager(),
      HandleSource(
          AllOf(SourceTypeIs(SourceType::kEvent),
                ImpressionOriginIs(*SuitableOrigin::Create(page_url)),
                ReportingOriginIs(*SuitableOrigin::Create(register_url))),
          web_contents()->GetPrimaryMainFrame()->GetGlobalId()))
      .Times(1)
      .WillOnce([&run_loop]() { run_loop.Quit(); });

  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("createAttributionSrcImg($1);", register_url)));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(AttributionSrcBrowserTest,
                       SourceRegisteredViaEligibilityHeader) {
  const char* kTestCases[] = {
      "createAttributionEligibleImgSrc($1);", "createAttributionSrcScript($1);",
      "doAttributionEligibleFetch($1);", "doAttributionEligibleXHR($1);",
      "createAttributionEligibleScriptSrc($1);"};
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");

  for (const char* registration_js : kTestCases) {
    EXPECT_TRUE(NavigateToURL(web_contents(), page_url));
    GURL register_url =
        https_server()->GetURL("c.test", "/register_source_headers.html");
    base::RunLoop run_loop;
    EXPECT_CALL(
        mock_attribution_manager(),
        HandleSource(
            AllOf(SourceTypeIs(SourceType::kEvent),
                  ImpressionOriginIs(*SuitableOrigin::Create(page_url)),
                  ReportingOriginIs(*SuitableOrigin::Create(register_url))),
            web_contents()->GetPrimaryMainFrame()->GetGlobalId()))
        .Times(1)
        .WillOnce([&run_loop]() { run_loop.Quit(); });
    EXPECT_TRUE(
        ExecJs(web_contents(), JsReplace(registration_js, register_url)));

    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_P(AttributionSrcBrowserTest, ForegroundRegistration) {
  base::HistogramTester histograms;
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");

  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));
  GURL register_url =
      https_server()->GetURL("c.test", "/register_source_headers.html");
  base::RunLoop run_loop;
  EXPECT_CALL(mock_attribution_manager(), HandleSource).WillOnce([&run_loop]() {
    run_loop.Quit();
  });
  EXPECT_TRUE(
      ExecJs(web_contents(),
             JsReplace("createAttributionEligibleImgSrc($1);", register_url)));

  run_loop.Run();

  // kForegroundBlink = 6
  histograms.ExpectBucketCount(kRegistrationMethod, 6, /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_P(AttributionSrcBrowserTest,
                       AttributionSrcWindowOpen_MultipleFeatures_RequestsAll) {
  // Create a separate server as we cannot register a `ControllableHttpResponse`
  // after the server starts.
  std::unique_ptr<EmbeddedTestServer> https_server =
      CreateAttributionTestHttpsServer();

  auto register_response1 =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/source1");
  auto register_response2 =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/source2");
  ASSERT_TRUE(https_server->Start());

  SourceObserver source_observer(web_contents());
  GURL page_url =
      https_server->GetURL("b.test", "/page_with_impression_creator.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), page_url));

  ASSERT_TRUE(ExecJs(web_contents(), R"(
    window.open("page_with_conversion_redirect.html", "_top",
                "attributionsrc=/source1 attributionsrc=/source2");
  )"));

  register_response1->WaitForRequest();
  register_response2->WaitForRequest();
  register_response1->Done();
  register_response2->Done();
}

// See crbug.com/1322450
IN_PROC_BROWSER_TEST_P(AttributionSrcBrowserTest,
                       AttributionSrcWindowOpen_URLEncoded_SourceRegistered) {
  // Create a separate server as we cannot register a `ControllableHttpResponse`
  // after the server starts.
  std::unique_ptr<EmbeddedTestServer> https_server =
      CreateAttributionTestHttpsServer();

  auto register_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/register_source?a=b&c=d");
  ASSERT_TRUE(https_server->Start());

  GURL page_url =
      https_server->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  // This attributionsrc will only be handled properly if the value is
  // URL-decoded before being passed to the attributionsrc loader.
  EXPECT_TRUE(ExecJs(web_contents(), R"(
  window.open("page_with_conversion_redirect.html", "_top",
  "attributionsrc=register_source%3Fa%3Db%26c%3Dd");)"));

  register_response->WaitForRequest();
  register_response->Done();

  EXPECT_EQ(register_response->http_request()->relative_url,
            "/register_source?a=b&c=d");
}

// See crbug.com/1338698
IN_PROC_BROWSER_TEST_P(AttributionSrcBrowserTest,
                       AttributionSrcWindowOpen_RetainsOriginalURLCase) {
  // Create a separate server as we cannot register a `ControllableHttpResponse`
  // after the server starts.
  std::unique_ptr<EmbeddedTestServer> https_server =
      CreateAttributionTestHttpsServer();

  auto register_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/register_source?a=B&C=d");
  ASSERT_TRUE(https_server->Start());

  GURL page_url =
      https_server->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  // This attributionsrc will only be handled properly if the URL's original
  // case is retained before being passed to the attributionsrc loader.
  EXPECT_TRUE(ExecJs(web_contents(), R"(
  window.open("page_with_conversion_redirect.html", "_top",
  "attributionsrc=register_source%3Fa%3DB%26C%3Dd");)"));

  register_response->WaitForRequest();
  register_response->Done();

  EXPECT_EQ(register_response->http_request()->relative_url,
            "/register_source?a=B&C=d");
}

// See crbug.com/1338698
IN_PROC_BROWSER_TEST_P(AttributionSrcBrowserTest,
                       AttributionSrcWindowOpen_NonAsciiUrl) {
  // Create a separate server as we cannot register a `ControllableHttpResponse`
  // after the server starts.
  std::unique_ptr<EmbeddedTestServer> https_server =
      CreateAttributionTestHttpsServer();

  auto register_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/%F0%9F%98%80");
  ASSERT_TRUE(https_server->Start());

  GURL page_url =
      https_server->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  // Ensure that the special handling of the original case for attributionsrc
  // features works with non-ASCII characters.
  EXPECT_TRUE(ExecJs(web_contents(), R"(
  window.open("page_with_conversion_redirect.html", "_top",
  "attributionsrc=😀");)"));

  register_response->WaitForRequest();
  register_response->Done();

  EXPECT_EQ(register_response->http_request()->relative_url, "/%F0%9F%98%80");
}

IN_PROC_BROWSER_TEST_P(
    AttributionSrcBrowserTest,
    AttributionSrcWindowOpenNoUserGesture_NoBackgroundRequestNoImpression) {
  SetupMockAttributionHost();

  // Create a separate server as we cannot register a `ControllableHttpResponse`
  // after the server starts.
  std::unique_ptr<EmbeddedTestServer> https_server =
      CreateAttributionTestHttpsServer();

  auto register_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/source");
  ASSERT_TRUE(https_server->Start());

  SourceObserver source_observer(web_contents());
  GURL page_url =
      https_server->GetURL("b.test", "/page_with_impression_creator.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), page_url));

  EXPECT_CALL(mock_attribution_host(), RegisterNavigationDataHost).Times(0);
  EXPECT_CALL(mock_attribution_host(),
              NotifyNavigationWithBackgroundRegistrationsWillStart)
      .Times(0);

  ASSERT_TRUE(ExecJs(web_contents(), R"(
    window.open("page_with_conversion_redirect.html", "_top",
      "attributionsrc=/source");
  )",
                     EXECUTE_SCRIPT_NO_USER_GESTURE));

  EXPECT_TRUE(source_observer.WaitForNavigationWithNoImpression());
  EXPECT_FALSE(register_response->has_received_request());
}

IN_PROC_BROWSER_TEST_P(AttributionSrcBrowserTest,
                       AttributionSrcImgRedirect_MultipleSourcesRegistered) {
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  base::RunLoop run_loop;
  const auto on_source = base::BarrierClosure(2, run_loop.QuitClosure());
  EXPECT_CALL(
      mock_attribution_manager(),
      HandleSource(
          SourceRegistrationIs(
              AllOf(Field(&SourceRegistration::source_event_id, 1u),
                    Field(&SourceRegistration::destination_set,
                          Property(&DestinationSet::destinations,
                                   ElementsAre(net::SchemefulSite::Deserialize(
                                       "https://d.test")))))),
          _))
      .Times(1)
      .WillOnce([&on_source]() { on_source.Run(); });
  EXPECT_CALL(
      mock_attribution_manager(),
      HandleSource(
          SourceRegistrationIs(
              AllOf(Field(&SourceRegistration::source_event_id, 5u),
                    Field(&SourceRegistration::destination_set,
                          Property(&DestinationSet::destinations,
                                   ElementsAre(net::SchemefulSite::Deserialize(
                                       "https://d.test")))))),
          _))
      .Times(1)
      .WillOnce([&on_source]() { on_source.Run(); });

  GURL register_url = https_server()->GetURL(
      "c.test", "/register_source_headers_and_redirect.html");

  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("createAttributionSrcImg($1);", register_url)));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(AttributionSrcBrowserTest,
                       AttributionSrcImgRedirect_InvalidJsonIgnored) {
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  base::RunLoop run_loop;
  EXPECT_CALL(
      mock_attribution_manager(),
      HandleSource(
          SourceRegistrationIs(
              AllOf(Field(&SourceRegistration::source_event_id, 5u),
                    Field(&SourceRegistration::destination_set,
                          Property(&DestinationSet::destinations,
                                   ElementsAre(net::SchemefulSite::Deserialize(
                                       "https://d.test")))))),
          _))
      .Times(1)
      .WillOnce([&run_loop]() { run_loop.Quit(); });

  GURL register_url = https_server()->GetURL(
      "c.test", "/register_source_headers_and_redirect_invalid.html");

  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("createAttributionSrcImg($1);", register_url)));

  // Only the second source is registered.
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(AttributionSrcBrowserTest,
                       AttributionSrcImgSlowResponse_SourceRegistered) {
  // Create a separate server as we cannot register a `ControllableHttpResponse`
  // after the server starts.
  std::unique_ptr<EmbeddedTestServer> https_server =
      CreateAttributionTestHttpsServer();

  auto register_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/register_source");
  ASSERT_TRUE(https_server->Start());

  GURL page_url =
      https_server->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  base::RunLoop run_loop;
  EXPECT_CALL(
      mock_attribution_manager(),
      HandleSource(
          SourceRegistrationIs(
              AllOf(Field(&SourceRegistration::source_event_id, 5u),
                    Field(&SourceRegistration::destination_set,
                          Property(&DestinationSet::destinations,
                                   ElementsAre(net::SchemefulSite::Deserialize(
                                       "https://d.test")))))),
          _))
      .Times(1)
      .WillOnce([&run_loop]() { run_loop.Quit(); });

  GURL register_url = https_server->GetURL("d.test", "/register_source");
  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("createAttributionSrcImg($1);", register_url)));

  // Navigate cross-site before sending a response.
  GURL page2_url =
      https_server->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page2_url));

  register_response->WaitForRequest();
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->AddCustomHeader("Access-Control-Allow-Origin", "*");
  http_response->AddCustomHeader(
      kAttributionReportingRegisterSourceHeader,
      R"({"source_event_id":"5", "destination":"https://d.test"})");
  register_response->Send(http_response->ToResponseString());
  register_response->Done();

  // Only the second source is registered.
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(AttributionSrcBrowserTest,
                       NoReferrerPolicy_UsesDefault) {
  // Create a separate server as we cannot register a `ControllableHttpResponse`
  // after the server starts.
  std::unique_ptr<EmbeddedTestServer> https_server =
      CreateAttributionTestHttpsServer();

  auto register_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/register_source");
  ASSERT_TRUE(https_server->Start());

  GURL page_url =
      https_server->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  GURL register_url = https_server->GetURL("d.test", "/register_source");
  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("createAttributionSrcImg($1);", register_url)));

  register_response->WaitForRequest();
  const net::test_server::HttpRequest* request =
      register_response->http_request();
  EXPECT_EQ(request->headers.at("Referer"), page_url.GetWithEmptyPath());
}

class AttributionSrcCrossAppWebDisabledBrowserTest
    : public AttributionSrcBrowserTest {
 public:
  AttributionSrcCrossAppWebDisabledBrowserTest()
      : AttributionSrcBrowserTest(
            /*enabled_features=*/{},
            /*disabled_features=*/{
                network::features::kAttributionReportingCrossAppWeb}) {}
};
INSTANTIATE_TEST_SUITE_P(All,
                         AttributionSrcCrossAppWebDisabledBrowserTest,
                         ::testing::Bool());

IN_PROC_BROWSER_TEST_P(AttributionSrcCrossAppWebDisabledBrowserTest,
                       Img_SetsAttributionReportingEligibleHeader) {
  // Create a separate server as we cannot register a `ControllableHttpResponse`
  // after the server starts.
  std::unique_ptr<EmbeddedTestServer> https_server =
      CreateAttributionTestHttpsServer();

  auto register_response1 =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/register_source1");
  auto register_response2 =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/register_source2");
  ASSERT_TRUE(https_server->Start());

  GURL page_url =
      https_server->GetURL("b.test", "/page_with_impression_creator.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), page_url));

  GURL register_url = https_server->GetURL("d.test", "/register_source1");
  ASSERT_TRUE(ExecJs(web_contents(),
                     JsReplace("createAttributionSrcImg($1);", register_url)));

  register_response1->WaitForRequest();
  ExpectValidAttributionReportingEligibleHeaderForImg(
      register_response1->http_request()->headers.at(
          "Attribution-Reporting-Eligible"));
  ASSERT_FALSE(base::Contains(register_response1->http_request()->headers,
                              "Attribution-Reporting-Support"));

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader("Location", "/register_source2");
  register_response1->Send(http_response->ToResponseString());
  register_response1->Done();

  // Ensure that redirect requests also contain the header.
  register_response2->WaitForRequest();
  ExpectValidAttributionReportingEligibleHeaderForImg(
      register_response2->http_request()->headers.at(
          "Attribution-Reporting-Eligible"));
  ASSERT_FALSE(base::Contains(register_response2->http_request()->headers,
                              "Attribution-Reporting-Support"));
}

// Regression test for crbug.com/1345955.
IN_PROC_BROWSER_TEST_P(AttributionSrcBrowserTest,
                       UntrustworthyUrl_DoesNotSetEligibleHeader) {
  auto http_server = std::make_unique<net::EmbeddedTestServer>();
  net::test_server::RegisterDefaultHandlers(http_server.get());

  auto response1 = std::make_unique<net::test_server::ControllableHttpResponse>(
      http_server.get(), "/register_source1");
  auto response2 = std::make_unique<net::test_server::ControllableHttpResponse>(
      http_server.get(), "/register_source2");
  ASSERT_TRUE(http_server->Start());

  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), page_url));

  GURL register_url1 = http_server->GetURL("d.test", "/register_source1");
  ASSERT_TRUE(ExecJs(web_contents(), JsReplace(R"(
  createAndClickAttributionSrcAnchor({url: $1, attributionsrc: '', target: '_blank'});)",
                                               register_url1)));

  response1->WaitForRequest();
  ASSERT_FALSE(base::Contains(response1->http_request()->headers,
                              "Attribution-Reporting-Eligible"));
  ASSERT_FALSE(base::Contains(response1->http_request()->headers,
                              "Attribution-Reporting-Support"));

  GURL register_url2 = http_server->GetURL("d.test", "/register_source2");
  ASSERT_TRUE(ExecJs(web_contents(), JsReplace(R"(
    window.open($1, '_blank', 'attributionsrc=');)",
                                               register_url2)));

  response2->WaitForRequest();
  ASSERT_FALSE(base::Contains(response2->http_request()->headers,
                              "Attribution-Reporting-Eligible"));
  ASSERT_FALSE(base::Contains(response2->http_request()->headers,
                              "Attribution-Reporting-Support"));
}

IN_PROC_BROWSER_TEST_P(AttributionSrcBrowserTest,
                       ReferrerPolicy_RespectsDocument) {
  // Create a separate server as we cannot register a `ControllableHttpResponse`
  // after the server starts.
  std::unique_ptr<EmbeddedTestServer> https_server =
      CreateAttributionTestHttpsServer();

  auto register_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/register_source");
  ASSERT_TRUE(https_server->Start());

  GURL page_url = https_server->GetURL(
      "b.test", "/page_with_impression_creator_no_referrer.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  GURL register_url = https_server->GetURL("d.test", "/register_source");
  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("createAttributionSrcImg($1);", register_url)));

  register_response->WaitForRequest();
  const net::test_server::HttpRequest* request =
      register_response->http_request();
  EXPECT_FALSE(base::Contains(request->headers, "Referer"));
}

IN_PROC_BROWSER_TEST_P(AttributionSrcBrowserTest,
                       BasicTrigger_TriggerRegistered) {
  const char* kTestCases[] = {"createAttributionSrcImg($1)",
                              "window.fetch($1, {mode:'no-cors'})"};
  for (const char* js_template : kTestCases) {
    SCOPED_TRACE(js_template);
    GURL page_url =
        https_server()->GetURL("b.test", "/page_with_impression_creator.html");
    EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

    base::RunLoop run_loop;
    EXPECT_CALL(mock_attribution_manager(), HandleTrigger)
        .Times(1)
        .WillOnce([&run_loop]() { run_loop.Quit(); });
    GURL register_url =
        https_server()->GetURL("c.test", "/register_trigger_headers.html");
    EXPECT_TRUE(ExecJs(web_contents(), JsReplace(js_template, register_url)));

    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_P(AttributionSrcBrowserTest,
                       PermissionsPolicyDisabled_SourceNotRegistered) {
  SetupMockAttributionHost();

  GURL page_url = https_server()->GetURL(
      "b.test", "/page_with_conversion_measurement_disabled.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  EXPECT_CALL(mock_attribution_host(), RegisterDataHost).Times(0);

  GURL register_url =
      https_server()->GetURL("c.test", "/register_source_headers.html");

  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("createAttributionSrcImg($1);", register_url)));

  // If a data host were registered, it would arrive in the browser process
  // before the navigation finished.
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));
}

IN_PROC_BROWSER_TEST_P(AttributionSrcBrowserTest,
                       AttributionSrcImg_InvalidTriggerJsonIgnored) {
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  base::RunLoop run_loop;
  EXPECT_CALL(
      mock_attribution_manager(),
      HandleTrigger(
          Property(&AttributionTrigger::registration,
                   Field(&TriggerRegistration::event_triggers,
                         ElementsAre(Field(&EventTriggerData::data, 7u)))),
          _))
      .Times(1)
      .WillOnce([&run_loop]() { run_loop.Quit(); });

  GURL register_url = https_server()->GetURL(
      "c.test", "/register_trigger_headers_then_redirect_invalid.html");

  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("createAttributionSrcImg($1);", register_url)));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(AttributionSrcBrowserTest,
                       ImgNoneSupported_EligibleHeaderNotSet) {
  MockAttributionReportingContentBrowserClientBase<
      ContentBrowserTestContentBrowserClient>
      browser_client;
  EXPECT_CALL(
      browser_client,
      GetAttributionSupport(
          ContentBrowserClient::AttributionReportingOsApiState::kDisabled,
          /*client_os_disabled=*/false))
      .WillRepeatedly(
          testing::Return(network::mojom::AttributionSupport::kNone));

  // Create a separate server as we cannot register a `ControllableHttpResponse`
  // after the server starts.
  std::unique_ptr<EmbeddedTestServer> https_server =
      CreateAttributionTestHttpsServer();

  auto register_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/register_source");
  ASSERT_TRUE(https_server->Start());

  GURL page_url =
      https_server->GetURL("b.test", "/page_with_impression_creator.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), page_url));

  GURL register_url = https_server->GetURL("d.test", "/register_source");
  ASSERT_TRUE(
      ExecJs(web_contents(),
             JsReplace("createAttributionEligibleImgSrc($1);", register_url)));

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

// Regression test for https://crbug.com/1498717.
IN_PROC_BROWSER_TEST_P(AttributionSrcBrowserTest,
                       ResponseReceivedInDetachedFrame_NoCrash) {
  // Create a separate server as we cannot register a `ControllableHttpResponse`
  // after the server starts.
  std::unique_ptr<EmbeddedTestServer> https_server =
      CreateAttributionTestHttpsServer();

  auto register_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/register_source");
  ASSERT_TRUE(https_server->Start());

  GURL page_url = https_server->GetURL("b.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), page_url));

  GURL subframe_url =
      https_server->GetURL("c.test", "/page_with_impression_creator.html");
  NavigateIframeToURL(web_contents(), "test_iframe", subframe_url);

  base::RunLoop run_loop;
  EXPECT_CALL(mock_attribution_manager(), HandleSource)
      .Times(1)
      .WillOnce([&run_loop]() { run_loop.Quit(); });

  RenderFrameHost* subframe =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);

  GURL register_url = https_server->GetURL("d.test", "/register_source");
  ASSERT_TRUE(ExecJs(subframe,
                     JsReplace("createAttributionSrcImg($1);", register_url)));
  register_response->WaitForRequest();

  ASSERT_TRUE(ExecJs(web_contents(),
                     R"(
                       const iframe = document.getElementById('test_iframe');
                       iframe.remove();
                     )"));

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->AddCustomHeader(kAttributionReportingRegisterSourceHeader,
                                 R"({"destination":"https://d.test"})");
  register_response->Send(http_response->ToResponseString());
  register_response->Done();

  run_loop.Run();
}

// Regression test for https://crbug.com/1520612.
IN_PROC_BROWSER_TEST_P(AttributionSrcBrowserTest,
                       ForegroundNavigationRedirectCancelled_SourceRegistered) {
  base::HistogramTester histograms;
  TestNavigationThrottleInserter throttle_inserter(
      web_contents(),
      base::BindLambdaForTesting(
          [&](NavigationHandle* handle) -> std::unique_ptr<NavigationThrottle> {
            auto throttle = std::make_unique<TestNavigationThrottle>(handle);
            throttle->SetResponse(TestNavigationThrottle::WILL_REDIRECT_REQUEST,
                                  TestNavigationThrottle::SYNCHRONOUS,
                                  NavigationThrottle::CANCEL_AND_IGNORE);

            return throttle;
          }));

  // Create a separate server as we cannot register a `ControllableHttpResponse`
  // after the server starts.
  std::unique_ptr<EmbeddedTestServer> https_server =
      CreateAttributionTestHttpsServer();

  auto register_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/register_source_redirect");

  ASSERT_TRUE(https_server->Start());

  GURL page_url =
      https_server->GetURL("b.test", "/page_with_impression_creator.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), page_url));

  GURL register_source_url =
      https_server->GetURL("d.test", "/register_source_redirect");

  base::RunLoop run_loop;
  EXPECT_CALL(
      mock_attribution_manager(),
      HandleSource(
          ReportingOriginIs(*SuitableOrigin::Create(register_source_url)), _))
      .Times(1)
      .WillOnce([&run_loop]() { run_loop.Quit(); });

  ASSERT_TRUE(ExecJs(web_contents(), JsReplace(R"(
    createAttributionSrcAnchor({id: 'link',
                        url: $1,
                        attributionsrc: '',
                        target: $2});)",
                                               register_source_url, "_top")));

  ASSERT_TRUE(ExecJs(web_contents(), "simulateClick('link');"));

  register_response->WaitForRequest();
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader(kAttributionReportingRegisterSourceHeader,
                                 R"({"destination":"https://a.test"})");
  http_response->AddCustomHeader(
      "Location",
      https_server
          ->GetURL("c.test",
                   "/attribution_reporting/page_with_conversion_redirect.html")
          .spec());
  register_response->Send(http_response->ToResponseString());
  register_response->Done();

  run_loop.Run();

  // kNavForegrounnd = 0
  histograms.ExpectBucketCount(kRegistrationMethod, 0, /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_P(AttributionSrcBrowserTest,
                       MultipleBackgroundRequest_AllRegistered) {
  const char* kTestCases[] = {"createAttributionSrcImg($1)",
                              "createAttributionSrcScript($1)"};
  for (const char* js_template : kTestCases) {
    base::HistogramTester histograms;
    SCOPED_TRACE(js_template);
    // Create a separate server as we cannot register a
    // `ControllableHttpResponse` after the server starts.
    std::unique_ptr<EmbeddedTestServer> https_server =
        CreateAttributionTestHttpsServer();

    auto register_response1 =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            https_server.get(), "/source1");
    auto register_response2 =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            https_server.get(), "/trigger1");
    ASSERT_TRUE(https_server->Start());

    base::RunLoop run_loop;
    const auto receive_registration =
        base::BarrierClosure(2, run_loop.QuitClosure());
    EXPECT_CALL(mock_attribution_manager(), HandleSource)
        .Times(1)
        .WillOnce([&]() { receive_registration.Run(); });
    EXPECT_CALL(mock_attribution_manager(), HandleTrigger)
        .Times(1)
        .WillOnce([&]() { receive_registration.Run(); });

    SourceObserver source_observer(web_contents());
    GURL page_url =
        https_server->GetURL("b.test", "/page_with_impression_creator.html");
    ASSERT_TRUE(NavigateToURL(web_contents(), page_url));

    ASSERT_TRUE(ExecJs(
        web_contents(),
        JsReplace(js_template, "/source1 http://invalid.test /trigger1")));

    register_response1->WaitForRequest();
    register_response2->WaitForRequest();

    {
      auto http_response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      http_response->set_code(net::HTTP_OK);
      http_response->AddCustomHeader(kAttributionReportingRegisterSourceHeader,
                                     R"({"destination":"https://d.test"})");
      register_response1->Send(http_response->ToResponseString());
      register_response1->Done();
    }

    {
      auto http_response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      http_response->set_code(net::HTTP_OK);
      http_response->AddCustomHeader("Attribution-Reporting-Register-Trigger",
                                     R"({})");
      register_response2->Send(http_response->ToResponseString());
      register_response2->Done();
    }

    run_loop.Run();
    // kBackgroundBlink = 8, kForegroundOrBackgroundBrowser = 10
    histograms.ExpectBucketCount(kRegistrationMethod, GetParam() ? 10 : 8,
                                 /*expected_count=*/2);
  }
}

class AttributionSrcPrerenderBrowserTest : public AttributionSrcBrowserTest {
 public:
  AttributionSrcPrerenderBrowserTest()
      : prerender_helper_(
            base::BindRepeating(&AttributionSrcBrowserTest::web_contents,
                                base::Unretained(this))) {}

  ~AttributionSrcPrerenderBrowserTest() override = default;

 protected:
  content::test::PrerenderTestHelper prerender_helper_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         AttributionSrcPrerenderBrowserTest,
                         ::testing::Bool());

IN_PROC_BROWSER_TEST_P(AttributionSrcPrerenderBrowserTest,
                       SourceNotRegisteredOnPrerender) {
  SetupMockAttributionHost();
  EXPECT_CALL(mock_attribution_host(), RegisterDataHost).Times(0);

  const GURL kInitialUrl =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), kInitialUrl));

  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  FrameTreeNodeId host_id = prerender_helper_.AddPrerender(page_url);
  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);

  prerender_helper_.WaitForPrerenderLoadCompletion(page_url);
  content::RenderFrameHost* prerender_rfh =
      prerender_helper_.GetPrerenderedMainFrameHost(host_id);

  EXPECT_TRUE(ExecJs(
      prerender_rfh,
      JsReplace(
          "createAttributionSrcImg($1);",
          https_server()->GetURL("c.test", "/register_source_headers.html"))));

  // If a data host were registered, it would arrive in the browser process
  // before the navigation finished.
  EXPECT_TRUE(NavigateToURL(web_contents(), kInitialUrl));
}

IN_PROC_BROWSER_TEST_P(AttributionSrcPrerenderBrowserTest,
                       SourceRegisteredOnActivatedPrerender) {
  base::RunLoop run_loop;
  EXPECT_CALL(mock_attribution_manager(),
              HandleSource(SourceRegistrationIs(
                               Field(&SourceRegistration::source_event_id, 5u)),
                           _))
      .Times(1)
      .WillOnce([&run_loop]() { run_loop.Quit(); });

  const GURL kInitialUrl =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), kInitialUrl));

  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  FrameTreeNodeId host_id = prerender_helper_.AddPrerender(page_url);
  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);

  prerender_helper_.WaitForPrerenderLoadCompletion(page_url);
  content::RenderFrameHost* prerender_rfh =
      prerender_helper_.GetPrerenderedMainFrameHost(host_id);

  EXPECT_TRUE(ExecJs(
      prerender_rfh,
      JsReplace(
          "createAttributionSrcImg($1);",
          https_server()->GetURL("c.test", "/register_source_headers.html"))));

  prerender_helper_.NavigatePrimaryPage(page_url);
  ASSERT_EQ(page_url, web_contents()->GetLastCommittedURL());

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(AttributionSrcPrerenderBrowserTest,
                       SubresourceTriggerNotRegisteredOnPrerender) {
  SetupMockAttributionHost();
  EXPECT_CALL(mock_attribution_host(), RegisterDataHost).Times(0);

  const GURL kInitialUrl =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), kInitialUrl));

  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_conversion_redirect.html");
  FrameTreeNodeId host_id = prerender_helper_.AddPrerender(page_url);
  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);

  prerender_helper_.WaitForPrerenderLoadCompletion(page_url);
  content::RenderFrameHost* prerender_rfh =
      prerender_helper_.GetPrerenderedMainFrameHost(host_id);

  EXPECT_TRUE(ExecJs(
      prerender_rfh,
      JsReplace(
          "createTrackingPixel($1);",
          https_server()->GetURL("c.test", "/register_trigger_headers.html"))));

  // If a data host were registered, it would arrive in the browser process
  // before the navigation finished.
  EXPECT_TRUE(NavigateToURL(web_contents(), kInitialUrl));
}

IN_PROC_BROWSER_TEST_P(AttributionSrcPrerenderBrowserTest,
                       SubresourceTriggerRegisteredOnActivatedPrerender) {
  base::RunLoop loop;
  EXPECT_CALL(
      mock_attribution_manager(),
      HandleTrigger(
          Property(&AttributionTrigger::registration,
                   Field(&TriggerRegistration::event_triggers,
                         ElementsAre(Field(&EventTriggerData::data, 7u)))),
          _))
      .Times(1)
      .WillOnce([&loop]() { loop.Quit(); });

  const GURL kInitialUrl =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), kInitialUrl));

  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_conversion_redirect.html");
  FrameTreeNodeId host_id = prerender_helper_.AddPrerender(page_url);
  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);

  prerender_helper_.WaitForPrerenderLoadCompletion(page_url);
  content::RenderFrameHost* prerender_rfh =
      prerender_helper_.GetPrerenderedMainFrameHost(host_id);

  EXPECT_TRUE(ExecJs(
      prerender_rfh,
      JsReplace(
          "createTrackingPixel($1);",
          https_server()->GetURL("c.test", "/register_trigger_headers.html"))));

  // Delay prerender activation so that subresource response is received
  // earlier than that.
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(100));
  run_loop.Run();

  prerender_helper_.NavigatePrimaryPage(page_url);
  ASSERT_EQ(page_url, web_contents()->GetLastCommittedURL());
  ASSERT_TRUE(host_observer.was_activated());

  loop.Run();
}

class AttributionSrcFencedFrameBrowserTest : public AttributionSrcBrowserTest {
 public:
  AttributionSrcFencedFrameBrowserTest() {
    fenced_frame_helper_ = std::make_unique<test::FencedFrameTestHelper>();
  }

  ~AttributionSrcFencedFrameBrowserTest() override = default;

 protected:
  std::unique_ptr<test::FencedFrameTestHelper> fenced_frame_helper_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         AttributionSrcFencedFrameBrowserTest,
                         ::testing::Bool());

IN_PROC_BROWSER_TEST_P(AttributionSrcFencedFrameBrowserTest,
                       DefaultMode_SourceNotRegistered) {
  SetupMockAttributionHost();
  GURL main_url = https_server()->GetURL("b.test", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  GURL fenced_frame_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");

  RenderFrameHost* parent = web_contents()->GetPrimaryMainFrame();

  RenderFrameHost* fenced_frame_host =
      fenced_frame_helper_->CreateFencedFrame(parent, fenced_frame_url);

  ASSERT_NE(fenced_frame_host, nullptr);
  EXPECT_TRUE(fenced_frame_host->IsFencedFrameRoot());

  EXPECT_CALL(mock_attribution_host(), RegisterDataHost).Times(0);

  EXPECT_TRUE(ExecJs(
      fenced_frame_host,
      JsReplace(
          "createAttributionSrcImg($1);",
          https_server()->GetURL("c.test", "/register_source_headers.html"))));

  // If a data host were registered, it would arrive in the browser process
  // before the navigation finished.
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
}

IN_PROC_BROWSER_TEST_P(AttributionSrcFencedFrameBrowserTest,
                       OpaqueAdsMode_SourceRegistered) {
  GURL main_url = https_server()->GetURL("b.test", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  RenderFrameHostImpl* root_rfh =
      static_cast<RenderFrameHostImpl*>(web_contents()->GetPrimaryMainFrame());
  GURL fenced_frame_url(https_server()->GetURL(
      "b.test", "/attribution_reporting/page_with_impression_creator.html"));
  FencedFrameURLMapping& url_mapping =
      root_rfh->GetPage().fenced_frame_urls_map();
  auto fenced_frame_urn =
      test::AddAndVerifyFencedFrameURL(&url_mapping, fenced_frame_url);

  RenderFrameHostWrapper fenced_frame_host(
      fenced_frame_helper_->CreateFencedFrame(
          root_rfh, GURL(url::kAboutBlankURL), net::OK,
          blink::FencedFrame::DeprecatedFencedFrameMode::kOpaqueAds));
  FrameTreeNode* fenced_frame_node =
      static_cast<RenderFrameHostImpl*>(&(*fenced_frame_host))
          ->frame_tree_node();
  bool rfh_should_change =
      fenced_frame_host->ShouldChangeRenderFrameHostOnSameSiteNavigation();
  TestFrameNavigationObserver observer(fenced_frame_host.get());

  EXPECT_TRUE(ExecJs(root_rfh, JsReplace("document.querySelector('fencedframe')"
                                         ".config = new FencedFrameConfig($1);",
                                         fenced_frame_urn.spec())));

  observer.Wait();

  if (rfh_should_change) {
    EXPECT_TRUE(fenced_frame_host.WaitUntilRenderFrameDeleted());
  } else {
    ASSERT_NE(fenced_frame_node->current_frame_host(), nullptr);
  }

  RenderFrameHostWrapper fenced_frame_host2(
      content::test::FencedFrameTestHelper::GetMostRecentlyAddedFencedFrame(
          root_rfh));

  EXPECT_TRUE(fenced_frame_host2->IsFencedFrameRoot());

  base::RunLoop run_loop;
  EXPECT_CALL(mock_attribution_manager(), HandleSource)
      .Times(1)
      .WillOnce([&run_loop]() { run_loop.Quit(); });

  EXPECT_TRUE(ExecJs(
      fenced_frame_host2.get(),
      JsReplace(
          "createAttributionSrcImg($1);",
          https_server()->GetURL("c.test", "/register_source_headers.html"))));

  run_loop.Run();
}

class AttributionSrcCrossAppWebEnabledBrowserTest
    : public AttributionSrcBrowserTest {
 public:
  AttributionSrcCrossAppWebEnabledBrowserTest()
      : AttributionSrcBrowserTest(/*enabled_features=*/{
            network::features::kAttributionReportingCrossAppWeb}) {}
};
INSTANTIATE_TEST_SUITE_P(All,
                         AttributionSrcCrossAppWebEnabledBrowserTest,
                         ::testing::Bool());

IN_PROC_BROWSER_TEST_P(AttributionSrcCrossAppWebEnabledBrowserTest,
                       Img_SetsSupportHeader) {
  // Create a separate server as we cannot register a `ControllableHttpResponse`
  // after the server starts.
  std::unique_ptr<EmbeddedTestServer> https_server =
      CreateAttributionTestHttpsServer();

  auto register_response1 =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/register_source1");
  auto register_response2 =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/register_source2");
  ASSERT_TRUE(https_server->Start());

  GURL page_url =
      https_server->GetURL("b.test", "/page_with_impression_creator.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), page_url));

  GURL register_url = https_server->GetURL("d.test", "/register_source1");
  ASSERT_TRUE(ExecJs(web_contents(),
                     JsReplace("createAttributionSrcImg($1);", register_url)));

  register_response1->WaitForRequest();
  ExpectValidAttributionReportingSupportHeader(
      register_response1->http_request()->headers.at(
          "Attribution-Reporting-Support"),
      /*web_expected=*/true,
      /*os_expected=*/false);

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader("Location", "/register_source2");
  register_response1->Send(http_response->ToResponseString());
  register_response1->Done();

  // Ensure that redirect requests also contain the headers.
  register_response2->WaitForRequest();
  ExpectValidAttributionReportingSupportHeader(
      register_response2->http_request()->headers.at(
          "Attribution-Reporting-Support"),
      /*web_expected=*/true,
      /*os_expected=*/false);
}

IN_PROC_BROWSER_TEST_P(AttributionSrcCrossAppWebEnabledBrowserTest,
                       Subresource_Register) {
  const char* kTestCases[] = {
      "createAttributionSrcImg($1)",         "createAttributionSrcScript($1)",
      "createAttributionEligibleImgSrc($1)", "createAttributionSrcScript($1)",
      "doAttributionEligibleFetch($1)",      "doAttributionEligibleXHR($1)"};
  for (const char* js_template : kTestCases) {
    SCOPED_TRACE(js_template);

    // Create a separate server as we cannot register a
    // `ControllableHttpResponse` after the server starts.
    std::unique_ptr<EmbeddedTestServer> https_server =
        CreateAttributionTestHttpsServer();

    auto register_response1 =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            https_server.get(), "/register_source1");
    auto register_response2 =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            https_server.get(), "/register_source2");
    ASSERT_TRUE(https_server->Start());

    AttributionOsLevelManager::ScopedApiStateForTesting
        scoped_api_state_setting(AttributionOsLevelManager::ApiState::kEnabled);

    GURL page_url =
        https_server->GetURL("b.test", "/page_with_impression_creator.html");
    ASSERT_TRUE(NavigateToURL(web_contents(), page_url));

    GURL register_url = https_server->GetURL("b.test", "/register_source1");
    ASSERT_TRUE(ExecJs(web_contents(), JsReplace(js_template, register_url)));

    register_response1->WaitForRequest();
    ExpectValidAttributionReportingSupportHeader(
        register_response1->http_request()->headers.at(
            "Attribution-Reporting-Support"),
        /*web_expected=*/true,
        /*os_expected=*/true);

    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
    http_response->AddCustomHeader("Location", "/register_source2");
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
}

IN_PROC_BROWSER_TEST_P(
    AttributionSrcCrossAppWebEnabledBrowserTest,
    OsLevelEnabledPostRendererInitialization_SetsSupportHeader) {
  // Create a separate server as we cannot register a `ControllableHttpResponse`
  // after the server starts.
  std::unique_ptr<EmbeddedTestServer> https_server =
      CreateAttributionTestHttpsServer();

  auto register_response1 =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/register_source1");
  auto register_response2 =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/register_source2");
  ASSERT_TRUE(https_server->Start());

  GURL page_url =
      https_server->GetURL("b.test", "/page_with_impression_creator.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), page_url));

  AttributionOsLevelManager::ScopedApiStateForTesting scoped_api_state_setting(
      AttributionOsLevelManager::ApiState::kEnabled);

  GURL register_url = https_server->GetURL("d.test", "/register_source1");
  ASSERT_TRUE(ExecJs(web_contents(),
                     JsReplace("createAttributionSrcImg($1);", register_url)));

  register_response1->WaitForRequest();
  ExpectValidAttributionReportingSupportHeader(
      register_response1->http_request()->headers.at(
          "Attribution-Reporting-Support"),
      /*web_expected=*/true,
      /*os_expected=*/true);

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader("Location", "/register_source2");
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

IN_PROC_BROWSER_TEST_P(AttributionSrcCrossAppWebEnabledBrowserTest,
                       OsRegistration_Register) {
  struct OsRegistrationTestCase {
    const char* name;
    const char* header;
    std::vector<attribution_reporting::OsRegistrationItem>
        expected_os_registrations;
  };
  const OsRegistrationTestCase kTestCases[] = {
      OsRegistrationTestCase{
          .name = "source",
          .header = "Attribution-Reporting-Register-OS-Source",
          .expected_os_registrations =
              {
                  attribution_reporting::OsRegistrationItem{
                      .url = GURL("https://r1.test/x")},
                  attribution_reporting::OsRegistrationItem{
                      .url = GURL("https://r2.test/y"),
                      .debug_reporting = true,
                  },
              },
      },
      OsRegistrationTestCase{
          .name = "trigger",
          .header = "Attribution-Reporting-Register-OS-Trigger",
          .expected_os_registrations =
              {
                  attribution_reporting::OsRegistrationItem{
                      .url = GURL("https://r1.test/x")},
                  attribution_reporting::OsRegistrationItem{
                      .url = GURL("https://r2.test/y"),
                      .debug_reporting = true,
                  },
              },
      }};
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.name);

    // Create a separate server as we cannot register a
    // `ControllableHttpResponse` after the server starts.
    std::unique_ptr<EmbeddedTestServer> https_server =
        CreateAttributionTestHttpsServer();

    base::RunLoop run_loop;
    EXPECT_CALL(
        mock_attribution_manager(),
        HandleOsRegistration(Field(&OsRegistration::registration_items,
                                   test_case.expected_os_registrations)))
        .Times(1)
        .WillOnce([&run_loop]() { run_loop.Quit(); });

    auto register_response =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            https_server.get(), "/register");
    ASSERT_TRUE(https_server->Start());

    AttributionOsLevelManager::ScopedApiStateForTesting
        scoped_api_state_setting(AttributionOsLevelManager::ApiState::kEnabled);

    GURL page_url =
        https_server->GetURL("b.test", "/page_with_impression_creator.html");
    ASSERT_TRUE(NavigateToURL(web_contents(), page_url));

    GURL register_url = https_server->GetURL("d.test", "/register");
    ASSERT_TRUE(ExecJs(web_contents(), JsReplace("createAttributionSrcImg($1);",
                                                 register_url)));

    register_response->WaitForRequest();

    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->AddCustomHeader(
        test_case.header,
        R"("https://r1.test/x", "https://r2.test/y"; debug-reporting)");
    register_response->Send(http_response->ToResponseString());
    register_response->Done();

    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_P(AttributionSrcCrossAppWebEnabledBrowserTest,
                       WebAndOsHeadersAndPreferOs_OsRegistered) {
  AttributionOsLevelManager::ScopedApiStateForTesting scoped_api_state_setting(
      AttributionOsLevelManager::ApiState::kEnabled);

  const char* kTestCases[] = {
      "/register_source_headers_preferred_os.html",
      "/register_trigger_headers_preferred_os.html",
  };

  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");

  for (const char* path : kTestCases) {
    SCOPED_TRACE(path);

    EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

    GURL register_url = https_server()->GetURL("c.test", path);

    base::RunLoop run_loop;
    EXPECT_CALL(
        mock_attribution_manager(),
        HandleOsRegistration(Field(
            &OsRegistration::registration_items,
            ElementsAre(Field(&attribution_reporting::OsRegistrationItem::url,
                              GURL("https://r.test"))))))
        .Times(1)
        .WillOnce([&run_loop]() { run_loop.Quit(); });

    EXPECT_TRUE(ExecJs(web_contents(), JsReplace("createAttributionSrcImg($1);",
                                                 register_url)));

    run_loop.Run();
  }
}

}  // namespace content
