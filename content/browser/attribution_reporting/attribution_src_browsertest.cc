// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/aggregation_service/aggregation_service.mojom.h"
#include "components/attribution_reporting/aggregatable_dedup_key.h"
#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/os_support.mojom.h"
#include "components/attribution_reporting/registration_type.mojom.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/test_utils.h"
#include "content/browser/attribution_reporting/attribution_constants.h"
#include "content/browser/attribution_reporting/attribution_manager_impl.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/test/mock_attribution_host.h"
#include "content/browser/attribution_reporting/test/mock_data_host.h"
#include "content/browser/attribution_reporting/test/source_observer.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/base/net_errors.h"
#include "net/base/schemeful_site.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/navigation/impression.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/attribution_reporting/attribution_os_level_manager_android.h"
#endif

namespace content {

namespace {

using ::attribution_reporting::FilterPair;
using ::attribution_reporting::mojom::RegistrationType;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

}  // namespace

class AttributionSrcBrowserTest : public ContentBrowserTest {
 public:
  AttributionSrcBrowserTest() = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    net::test_server::RegisterDefaultHandlers(https_server_.get());
    https_server_->ServeFilesFromSourceDirectory(
        "content/test/data/attribution_reporting");
    https_server_->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(https_server_->Start());

    MockAttributionHost::Override(web_contents());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Sets up the blink runtime feature for ConversionMeasurement.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  WebContents* web_contents() { return shell()->web_contents(); }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  MockAttributionHost& mock_attribution_host() {
    AttributionHost* attribution_host =
        AttributionHost::FromWebContents(web_contents());
    return *static_cast<MockAttributionHost*>(attribution_host);
  }

 private:
  AttributionManagerImpl::ScopedUseInMemoryStorageForTesting
      attribution_manager_in_memory_setting_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

IN_PROC_BROWSER_TEST_F(AttributionSrcBrowserTest, SourceRegistered) {
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  std::unique_ptr<MockDataHost> data_host;
  base::RunLoop loop;
  EXPECT_CALL(mock_attribution_host(), RegisterDataHost)
      .WillOnce(
          [&](mojo::PendingReceiver<blink::mojom::AttributionDataHost> host,
              RegistrationType) {
            data_host = GetRegisteredDataHost(std::move(host));
            loop.Quit();
          });

  GURL register_url =
      https_server()->GetURL("c.test", "/register_source_headers.html");

  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("createAttributionSrcImg($1);", register_url)));
  if (!data_host) {
    loop.Run();
  }
  data_host->WaitForSourceData(/*num_source_data=*/1);
  const auto& source_data = data_host->source_data();

  EXPECT_EQ(source_data.size(), 1u);
  EXPECT_EQ(source_data.front().source_event_id, 5UL);
  EXPECT_THAT(source_data.front().destination_set.destinations(),
              ElementsAre(net::SchemefulSite::Deserialize("https://d.test")));
  EXPECT_EQ(source_data.front().priority, 0);
  EXPECT_EQ(source_data.front().expiry, absl::nullopt);
  EXPECT_FALSE(source_data.front().debug_key);
  EXPECT_THAT(source_data.front().filter_data.filter_values(), IsEmpty());
  EXPECT_THAT(source_data.front().aggregation_keys.keys(), IsEmpty());
  EXPECT_FALSE(source_data.front().debug_reporting);
}

IN_PROC_BROWSER_TEST_F(AttributionSrcBrowserTest,
                       SourceRegisteredViaEligibilityHeader) {
  const char* kTestCases[] = {
      "createAttributionEligibleImgSrc($1);", "createAttributionSrcScript($1);",
      "doAttributionEligibleFetch($1);", "doAttributionEligibleXHR($1);",
      "createAttributionEligibleScriptSrc($1);"};
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");

  for (const char* registration_js : kTestCases) {
    EXPECT_TRUE(NavigateToURL(web_contents(), page_url));
    std::unique_ptr<MockDataHost> data_host;
    base::RunLoop loop, disconnect_loop;
    EXPECT_CALL(mock_attribution_host(), RegisterDataHost)
        .WillOnce(
            [&](mojo::PendingReceiver<blink::mojom::AttributionDataHost> host,
                RegistrationType) {
              data_host = GetRegisteredDataHost(std::move(host));
              data_host->receiver().set_disconnect_handler(
                  disconnect_loop.QuitClosure());
              loop.Quit();
            });

    GURL register_url =
        https_server()->GetURL("c.test", "/register_source_headers.html");

    EXPECT_TRUE(
        ExecJs(web_contents(), JsReplace(registration_js, register_url)));
    if (!data_host) {
      loop.Run();
    }
    data_host->WaitForSourceData(/*num_source_data=*/1);
    const auto& source_data = data_host->source_data();
    // Regression test for crbug.com/1336797. This will timeout flakily if the
    // data host isn't disconnected promptly.
    disconnect_loop.Run();

    EXPECT_EQ(source_data.size(), 1u);
    EXPECT_EQ(source_data.front().source_event_id, 5UL);
    EXPECT_THAT(source_data.front().destination_set.destinations(),
                ElementsAre(net::SchemefulSite::Deserialize("https://d.test")));
    EXPECT_EQ(source_data.front().priority, 0);
    EXPECT_EQ(source_data.front().expiry, absl::nullopt);
    EXPECT_FALSE(source_data.front().debug_key);
    EXPECT_THAT(source_data.front().filter_data.filter_values(), IsEmpty());
    EXPECT_THAT(source_data.front().aggregation_keys.keys(), IsEmpty());
    EXPECT_FALSE(source_data.front().debug_reporting);
  }
}

// TODO(johnidel): Remove when redirect chains consistently register sources or
// triggers. Currently, responses not handled via attributionsrc="url" use
// their own independent data host, so we do not enforce consistency on
// these redirect chains.
IN_PROC_BROWSER_TEST_F(AttributionSrcBrowserTest,
                       SourceTriggerRegistered_ImgSrc) {
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  std::unique_ptr<MockDataHost> source_data_host;
  std::unique_ptr<MockDataHost> trigger_data_host;
  base::RunLoop source_loop;
  base::RunLoop trigger_loop;
  EXPECT_CALL(mock_attribution_host(), RegisterDataHost)
      .WillRepeatedly(
          [&](mojo::PendingReceiver<blink::mojom::AttributionDataHost> host,
              RegistrationType) {
            if (!source_data_host) {
              source_data_host = GetRegisteredDataHost(std::move(host));
              source_loop.Quit();
            } else {
              trigger_data_host = GetRegisteredDataHost(std::move(host));
              trigger_loop.Quit();
            }
          });

  GURL register_url = https_server()->GetURL(
      "c.test", "/register_source_trigger_redirect_chain.html");

  EXPECT_TRUE(
      ExecJs(web_contents(),
             JsReplace("createAttributionEligibleImgSrc($1);", register_url)));
  if (!source_data_host) {
    source_loop.Run();
  }
  source_data_host->WaitForSourceData(/*num_source_data=*/1);

  if (!trigger_data_host) {
    trigger_loop.Run();
  }
  trigger_data_host->WaitForTriggerData(/*num_trigger_data=*/1);
}

IN_PROC_BROWSER_TEST_F(AttributionSrcBrowserTest,
                       AttributionSrcAnchor_SourceRegistered) {
  SourceObserver source_observer(web_contents());
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  std::unique_ptr<MockDataHost> data_host;
  blink::AttributionSrcToken expected_token;
  EXPECT_CALL(mock_attribution_host(), RegisterNavigationDataHost)
      .WillOnce(
          [&](mojo::PendingReceiver<blink::mojom::AttributionDataHost> host,
              const blink::AttributionSrcToken& attribution_src_token) {
            data_host = GetRegisteredDataHost(std::move(host));
            expected_token = attribution_src_token;
          });

  GURL register_url =
      https_server()->GetURL("c.test", "/register_source_headers.html");
  EXPECT_TRUE(ExecJs(web_contents(), JsReplace(R"(
  createAndClickAttributionSrcAnchor({url: 'page_with_conversion_redirect.html',
                                      attributionsrc: $1});)",
                                               register_url)));

  // Wait for the impression to be seen by the observer.
  blink::Impression last_impression = source_observer.Wait();

  // Verify we received the correct token for this source.
  EXPECT_EQ(last_impression.attribution_src_token, expected_token);
  EXPECT_EQ(last_impression.nav_type,
            blink::mojom::AttributionNavigationType::kAnchor);

  // Verify the attributionsrc data was registered with the browser process.
  EXPECT_TRUE(data_host);

  // TODO(johnidel): Verify that the data host receives the correct callback.
  // Direct use of MockDataHost flakes rarely. See
  // AttributionSrcNavigationSourceAndTrigger_ReportSent in
  // AttributionsBrowserTest.
}

IN_PROC_BROWSER_TEST_F(AttributionSrcBrowserTest,
                       AttributionSrcWindowOpen_SourceRegistered) {
  SourceObserver source_observer(web_contents());
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  std::unique_ptr<MockDataHost> data_host;
  blink::AttributionSrcToken expected_token;
  EXPECT_CALL(mock_attribution_host(), RegisterNavigationDataHost)
      .WillOnce(
          [&](mojo::PendingReceiver<blink::mojom::AttributionDataHost> host,
              const blink::AttributionSrcToken& attribution_src_token) {
            data_host = GetRegisteredDataHost(std::move(host));
            expected_token = attribution_src_token;
          });

  GURL register_url =
      https_server()->GetURL("c.test", "/register_source_headers.html");
  EXPECT_TRUE(ExecJs(web_contents(), JsReplace(R"(
  window.open("page_with_conversion_redirect.html", "_top",
  "attributionsrc="+$1);)",
                                               register_url)));

  blink::Impression last_impression = source_observer.Wait();

  // Verify we received the correct token for this source.
  EXPECT_EQ(last_impression.attribution_src_token, expected_token);
  EXPECT_EQ(last_impression.nav_type,
            blink::mojom::AttributionNavigationType::kWindowOpen);

  // Verify the attributionsrc data was registered with the browser process.
  EXPECT_TRUE(data_host);

  // TODO(johnidel): Verify that the data host receives the correct callback.
  // Direct use of MockDataHost flakes rarely. See
  // AttributionSrcNavigationSourceAndTrigger_ReportSent in
  // AttributionsBrowserTest.
}

// See crbug.com/1426892, where attributionsrc window.open features are
// incorrectly handled if multiple attributionsrc features are specified.
// Multiple attributionsrc features should not issue multiple background
// requests, and we should only handle the last attributionsrc entry to comply
// with
// https://html.spec.whatwg.org/multipage/nav-history-apis.html#concept-window-open-features-tokenize
IN_PROC_BROWSER_TEST_F(AttributionSrcBrowserTest,
                       AttributionSrcWindowOpen_MultipleFeatures_UsesLast) {
  // Create a separate server as we cannot register a `ControllableHttpResponse`
  // after the server starts.
  auto https_server = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server->ServeFilesFromSourceDirectory(
      "content/test/data/attribution_reporting");

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

  register_response2->WaitForRequest();
  register_response2->Done();

  // Only the last feature's value should be used.
  EXPECT_FALSE(register_response1->has_received_request());
}

IN_PROC_BROWSER_TEST_F(AttributionSrcBrowserTest,
                       AnchorClickEmptyAttributionSrc_ImpressionReceived) {
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  EXPECT_CALL(mock_attribution_host(), RegisterNavigationDataHost).Times(0);

  SourceObserver source_observer(web_contents());
  EXPECT_TRUE(ExecJs(web_contents(), R"(
  createAndClickAttributionSrcAnchor({url: 'page_with_conversion_redirect.html',
                                      attributionsrc: ''});)"));

  // Wait for the impression to be seen by the observer.
  source_observer.Wait();
}

IN_PROC_BROWSER_TEST_F(AttributionSrcBrowserTest,
                       WindowOpenAttributionSrc_ImpressionReceived) {
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  EXPECT_CALL(mock_attribution_host(), RegisterNavigationDataHost).Times(0);

  SourceObserver source_observer(web_contents());
  EXPECT_TRUE(ExecJs(web_contents(), R"(
  window.open("page_with_conversion_redirect.html", "_top",
  "attributionsrc=");)"));

  // Wait for the impression to be seen by the observer.
  source_observer.Wait();
}

// See crbug.com/1322450
IN_PROC_BROWSER_TEST_F(AttributionSrcBrowserTest,
                       AttributionSrcWindowOpen_URLEncoded_SourceRegistered) {
  // Create a separate server as we cannot register a `ControllableHttpResponse`
  // after the server starts.
  auto https_server = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server->ServeFilesFromSourceDirectory(
      "content/test/data/attribution_reporting");

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
IN_PROC_BROWSER_TEST_F(AttributionSrcBrowserTest,
                       AttributionSrcWindowOpen_RetainsOriginalURLCase) {
  // Create a separate server as we cannot register a `ControllableHttpResponse`
  // after the server starts.
  auto https_server = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server->ServeFilesFromSourceDirectory(
      "content/test/data/attribution_reporting");

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
IN_PROC_BROWSER_TEST_F(AttributionSrcBrowserTest,
                       AttributionSrcWindowOpen_NonAsciiUrl) {
  // Create a separate server as we cannot register a `ControllableHttpResponse`
  // after the server starts.
  auto https_server = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server->ServeFilesFromSourceDirectory(
      "content/test/data/attribution_reporting");

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

IN_PROC_BROWSER_TEST_F(
    AttributionSrcBrowserTest,
    AttributionSrcWindowOpenNoUserGesture_SourceNotRegistered) {
  SourceObserver source_observer(web_contents());
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  std::unique_ptr<MockDataHost> data_host;
  blink::AttributionSrcToken expected_token;
  EXPECT_CALL(mock_attribution_host(), RegisterNavigationDataHost)
      .WillOnce(
          [&](mojo::PendingReceiver<blink::mojom::AttributionDataHost> host,
              const blink::AttributionSrcToken& attribution_src_token) {
            data_host = GetRegisteredDataHost(std::move(host));
            expected_token = attribution_src_token;
          });

  GURL register_url =
      https_server()->GetURL("c.test", "/register_source_headers.html");
  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace(R"(
  window.open("page_with_conversion_redirect.html", "_top",
  "attributionsrc="+$1);)",
                               register_url),
                     EXECUTE_SCRIPT_NO_USER_GESTURE));

  EXPECT_TRUE(source_observer.WaitForNavigationWithNoImpression());
}

IN_PROC_BROWSER_TEST_F(AttributionSrcBrowserTest,
                       AttributionSrcImgRedirect_MultipleSourcesRegistered) {
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  std::unique_ptr<MockDataHost> data_host;
  base::RunLoop loop;
  EXPECT_CALL(mock_attribution_host(), RegisterDataHost)
      .WillOnce(
          [&](mojo::PendingReceiver<blink::mojom::AttributionDataHost> host,
              RegistrationType) {
            data_host = GetRegisteredDataHost(std::move(host));
            loop.Quit();
          });

  GURL register_url = https_server()->GetURL(
      "c.test", "/register_source_headers_and_redirect.html");

  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("createAttributionSrcImg($1);", register_url)));
  if (!data_host) {
    loop.Run();
  }
  data_host->WaitForSourceData(/*num_source_data=*/2);
  const auto& source_data = data_host->source_data();

  EXPECT_EQ(source_data.size(), 2u);
  EXPECT_EQ(source_data.front().source_event_id, 1UL);
  EXPECT_THAT(source_data.front().destination_set.destinations(),
              ElementsAre(net::SchemefulSite::Deserialize("https://d.test")));
  EXPECT_EQ(source_data.back().source_event_id, 5UL);
  EXPECT_THAT(source_data.back().destination_set.destinations(),
              ElementsAre(net::SchemefulSite::Deserialize("https://d.test")));
}

IN_PROC_BROWSER_TEST_F(AttributionSrcBrowserTest,
                       AttributionSrcImgRedirect_InvalidJsonIgnored) {
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  std::unique_ptr<MockDataHost> data_host;
  base::RunLoop loop;
  EXPECT_CALL(mock_attribution_host(), RegisterDataHost)
      .WillOnce(
          [&](mojo::PendingReceiver<blink::mojom::AttributionDataHost> host,
              RegistrationType) {
            data_host = GetRegisteredDataHost(std::move(host));
            loop.Quit();
          });

  GURL register_url = https_server()->GetURL(
      "c.test", "/register_source_headers_and_redirect_invalid.html");

  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("createAttributionSrcImg($1);", register_url)));
  if (!data_host) {
    loop.Run();
  }
  data_host->WaitForSourceData(/*num_source_data=*/1);
  const auto& source_data = data_host->source_data();

  // Only the second source is registered.
  EXPECT_EQ(source_data.size(), 1u);
  EXPECT_EQ(source_data.back().source_event_id, 5UL);
  EXPECT_THAT(source_data.back().destination_set.destinations(),
              ElementsAre(net::SchemefulSite::Deserialize("https://d.test")));
}

IN_PROC_BROWSER_TEST_F(AttributionSrcBrowserTest,
                       AttributionSrcImgSlowResponse_SourceRegistered) {
  // Create a separate server as we cannot register a `ControllableHttpResponse`
  // after the server starts.
  auto https_server = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  net::test_server::RegisterDefaultHandlers(https_server.get());
  https_server->ServeFilesFromSourceDirectory(
      "content/test/data/attribution_reporting");
  https_server->ServeFilesFromSourceDirectory("content/test/data");

  auto register_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/register_source");
  ASSERT_TRUE(https_server->Start());

  GURL page_url =
      https_server->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  std::unique_ptr<MockDataHost> data_host;
  base::RunLoop loop;
  EXPECT_CALL(mock_attribution_host(), RegisterDataHost)
      .WillOnce(
          [&](mojo::PendingReceiver<blink::mojom::AttributionDataHost> host,
              RegistrationType) {
            data_host = GetRegisteredDataHost(std::move(host));
            loop.Quit();
          });

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

  if (!data_host) {
    loop.Run();
  }
  data_host->WaitForSourceData(/*num_source_data=*/1);
  const auto& source_data = data_host->source_data();

  // Only the second source is registered.
  EXPECT_EQ(source_data.size(), 1u);
  EXPECT_EQ(source_data.back().source_event_id, 5UL);
  EXPECT_THAT(source_data.back().destination_set.destinations(),
              ElementsAre(net::SchemefulSite::Deserialize("https://d.test")));
}

IN_PROC_BROWSER_TEST_F(AttributionSrcBrowserTest,
                       NoReferrerPolicy_UsesDefault) {
  // Create a separate server as we cannot register a `ControllableHttpResponse`
  // after the server starts.
  auto https_server = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  net::test_server::RegisterDefaultHandlers(https_server.get());
  https_server->ServeFilesFromSourceDirectory(
      "content/test/data/attribution_reporting");
  https_server->ServeFilesFromSourceDirectory("content/test/data");

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

IN_PROC_BROWSER_TEST_F(AttributionSrcBrowserTest,
                       Img_SetsAttributionReportingEligibleHeader) {
  // Create a separate server as we cannot register a `ControllableHttpResponse`
  // after the server starts.
  auto https_server = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  net::test_server::RegisterDefaultHandlers(https_server.get());
  https_server->ServeFilesFromSourceDirectory(
      "content/test/data/attribution_reporting");
  https_server->ServeFilesFromSourceDirectory("content/test/data");

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
  ASSERT_EQ(register_response1->http_request()->headers.at(
                "Attribution-Reporting-Eligible"),
            "event-source, trigger");
  ASSERT_FALSE(base::Contains(register_response1->http_request()->headers,
                              "Attribution-Reporting-Support"));

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader("Location", "/register_source2");
  register_response1->Send(http_response->ToResponseString());
  register_response1->Done();

  // Ensure that redirect requests also contain the header.
  register_response2->WaitForRequest();
  ASSERT_EQ(register_response2->http_request()->headers.at(
                "Attribution-Reporting-Eligible"),
            "event-source, trigger");
  ASSERT_FALSE(base::Contains(register_response2->http_request()->headers,
                              "Attribution-Reporting-Support"));
}

IN_PROC_BROWSER_TEST_F(AttributionSrcBrowserTest,
                       ImgSrcWithAttributionSrc_SetsEligibleHeader) {
  // Create a separate server as we cannot register a `ControllableHttpResponse`
  // after the server starts.
  auto https_server = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  net::test_server::RegisterDefaultHandlers(https_server.get());
  https_server->ServeFilesFromSourceDirectory(
      "content/test/data/attribution_reporting");
  https_server->ServeFilesFromSourceDirectory("content/test/data");

  auto register_response1 =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/register_source1");
  ASSERT_TRUE(https_server->Start());

  GURL page_url =
      https_server->GetURL("b.test", "/page_with_impression_creator.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), page_url));

  GURL register_url = https_server->GetURL("d.test", "/register_source1");
  ASSERT_TRUE(
      ExecJs(web_contents(),
             JsReplace("createAttributionEligibleImgSrc($1);", register_url)));

  register_response1->WaitForRequest();
  ASSERT_EQ(register_response1->http_request()->headers.at(
                "Attribution-Reporting-Eligible"),
            "event-source, trigger");
  ASSERT_FALSE(base::Contains(register_response1->http_request()->headers,
                              "Attribution-Reporting-Support"));
}

// Regression test for crbug.com/1345955.
IN_PROC_BROWSER_TEST_F(AttributionSrcBrowserTest,
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

IN_PROC_BROWSER_TEST_F(AttributionSrcBrowserTest,
                       ReferrerPolicy_RespectsDocument) {
  // Create a separate server as we cannot register a `ControllableHttpResponse`
  // after the server starts.
  auto https_server = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  net::test_server::RegisterDefaultHandlers(https_server.get());
  https_server->ServeFilesFromSourceDirectory(
      "content/test/data/attribution_reporting");
  https_server->ServeFilesFromSourceDirectory("content/test/data");

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
  EXPECT_TRUE(request->headers.find("Referer") == request->headers.end());
}

class AttributionSrcBasicTriggerBrowserTest
    : public AttributionSrcBrowserTest,
      public ::testing::WithParamInterface<
          std::pair<std::string, std::string>> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    AttributionSrcBasicTriggerBrowserTest,
    ::testing::Values(
        std::make_pair("attributionsrcimg", "createAttributionSrcImg($1)"),
        std::make_pair("fetch", "window.fetch($1, {mode:'no-cors'})")),
    [](const auto& info) { return info.param.first; });  // test name generator

IN_PROC_BROWSER_TEST_P(AttributionSrcBasicTriggerBrowserTest,
                       TriggerRegistered) {
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  std::unique_ptr<MockDataHost> data_host;
  base::RunLoop loop;
  EXPECT_CALL(mock_attribution_host(), RegisterDataHost)
      .WillOnce(
          [&](mojo::PendingReceiver<blink::mojom::AttributionDataHost> host,
              RegistrationType) {
            data_host = GetRegisteredDataHost(std::move(host));
            loop.Quit();
          });

  GURL register_url =
      https_server()->GetURL("c.test", "/register_trigger_headers.html");

  const std::string& js_template = GetParam().second;
  EXPECT_TRUE(ExecJs(web_contents(), JsReplace(js_template, register_url)));
  if (!data_host) {
    loop.Run();
  }
  data_host->WaitForTriggerData(/*num_trigger_data=*/1);

  EXPECT_THAT(
      data_host->trigger_data(),
      ElementsAre(TriggerRegistrationMatches(TriggerRegistrationMatcherConfig(
          FilterPair(),
          /*debug_key=*/Eq(absl::nullopt),
          ElementsAre(EventTriggerDataMatches(EventTriggerDataMatcherConfig(
              /*data=*/7))),
          std::vector<attribution_reporting::AggregatableDedupKey>(),
          /*debug_reporting=*/false,
          std::vector<attribution_reporting::AggregatableTriggerData>(),
          /*aggregatable_values=*/
          attribution_reporting::AggregatableValues(),
          ::aggregation_service::mojom::AggregationCoordinator::kDefault))));
}

IN_PROC_BROWSER_TEST_F(AttributionSrcBrowserTest,
                       PermissionsPolicyDisabled_SourceNotRegistered) {
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

IN_PROC_BROWSER_TEST_F(AttributionSrcBrowserTest,
                       AttributionSrcImg_TriggerRegisteredAllParams) {
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  std::unique_ptr<MockDataHost> data_host;
  base::RunLoop loop;
  EXPECT_CALL(mock_attribution_host(), RegisterDataHost)
      .WillOnce(
          [&](mojo::PendingReceiver<blink::mojom::AttributionDataHost> host,
              RegistrationType) {
            data_host = GetRegisteredDataHost(std::move(host));
            loop.Quit();
          });

  GURL register_url = https_server()->GetURL(
      "c.test", "/register_trigger_headers_all_params.html");

  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("createAttributionSrcImg($1);", register_url)));
  if (!data_host) {
    loop.Run();
  }
  data_host->WaitForTriggerData(/*num_trigger_data=*/1);

  EXPECT_THAT(
      data_host->trigger_data(),
      ElementsAre(TriggerRegistrationMatches(TriggerRegistrationMatcherConfig(
          FilterPair(/*positive=*/
                     {{{"w", {}}, {"x", {"y", "z"}}}},
                     /*negative=*/{{{"a", {"b"}}}}),
          /*debug_key=*/Optional(789),
          ElementsAre(attribution_reporting::EventTriggerData(
                          /*data=*/1,
                          /*priority=*/5, /*dedup_key=*/1024,
                          FilterPair(/*positive=*/
                                     {{{"a", {"b"}}}},
                                     /*negative=*/
                                     {{{"c", {}}}})),
                      attribution_reporting::EventTriggerData(
                          /*data=*/2, /*priority=*/10,
                          /*dedup_key=*/absl::nullopt,
                          FilterPair(/*positive=*/{}, /*negative=*/
                                     {{{"d", {"e", "f"}}, {"g", {}}}}))),
          std::vector<attribution_reporting::AggregatableDedupKey>{
              attribution_reporting::AggregatableDedupKey(
                  /*dedup_key=*/123, FilterPair())},
          /*debug_reporting=*/true,
          std::vector<attribution_reporting::AggregatableTriggerData>{
              *attribution_reporting::AggregatableTriggerData::Create(
                  /*key_piece=*/absl::MakeUint128(/*high=*/0, /*low=*/1),
                  /*source_keys=*/{"key"}, FilterPair())},
          /*aggregatable_values=*/
          *attribution_reporting::AggregatableValues::Create({{"key", 123}}),
          ::aggregation_service::mojom::AggregationCoordinator::kAwsCloud))));
}

IN_PROC_BROWSER_TEST_F(AttributionSrcBrowserTest,
                       AttributionSrcImg_InvalidTriggerJsonIgnored) {
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  std::unique_ptr<MockDataHost> data_host;
  base::RunLoop loop;
  EXPECT_CALL(mock_attribution_host(), RegisterDataHost)
      .WillOnce(
          [&](mojo::PendingReceiver<blink::mojom::AttributionDataHost> host,
              RegistrationType) {
            data_host = GetRegisteredDataHost(std::move(host));
            loop.Quit();
          });

  GURL register_url = https_server()->GetURL(
      "c.test", "/register_trigger_headers_then_redirect_invalid.html");

  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("createAttributionSrcImg($1);", register_url)));
  if (!data_host) {
    loop.Run();
  }
  data_host->WaitForTriggerData(/*num_trigger_data=*/1);
  const auto& trigger_data = data_host->trigger_data();

  EXPECT_EQ(trigger_data.size(), 1u);
  EXPECT_EQ(trigger_data.front().event_triggers.size(), 1u);
  EXPECT_EQ(trigger_data.front().event_triggers.front().data, 7u);
}

IN_PROC_BROWSER_TEST_F(AttributionSrcBrowserTest,
                       AttributionSrcImgTriggerThenSource_SourceIgnored) {
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  std::unique_ptr<MockDataHost> data_host;
  base::RunLoop loop;
  EXPECT_CALL(mock_attribution_host(), RegisterDataHost)
      .WillOnce(
          [&](mojo::PendingReceiver<blink::mojom::AttributionDataHost> host,
              RegistrationType) {
            data_host = GetRegisteredDataHost(std::move(host));
            loop.Quit();
          });

  GURL register_url =
      https_server()->GetURL("c.test", "/register_trigger_source_trigger.html");

  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("createAttributionSrcImg($1);", register_url)));
  if (!data_host) {
    loop.Run();
  }
  data_host->WaitForTriggerData(/*num_trigger_data=*/2);
  const auto& trigger_data = data_host->trigger_data();

  EXPECT_EQ(trigger_data.size(), 2u);

  // Both triggers should be processed.
  EXPECT_EQ(trigger_data.front().event_triggers.front().data, 5u);
  EXPECT_EQ(trigger_data.back().event_triggers.front().data, 7u);

  // Middle redirect source should be ignored.
  EXPECT_EQ(data_host->source_data().size(), 0u);
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

IN_PROC_BROWSER_TEST_F(AttributionSrcPrerenderBrowserTest,
                       SourceNotRegisteredOnPrerender) {
  EXPECT_CALL(mock_attribution_host(), RegisterDataHost).Times(0);

  const GURL kInitialUrl =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), kInitialUrl));

  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  int host_id = prerender_helper_.AddPrerender(page_url);
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

IN_PROC_BROWSER_TEST_F(AttributionSrcPrerenderBrowserTest,
                       SourceRegisteredOnActivatedPrerender) {
  std::unique_ptr<MockDataHost> data_host;
  base::RunLoop loop;
  EXPECT_CALL(mock_attribution_host(), RegisterDataHost)
      .WillOnce(
          [&](mojo::PendingReceiver<blink::mojom::AttributionDataHost> host,
              RegistrationType) {
            data_host = GetRegisteredDataHost(std::move(host));
            loop.Quit();
          });

  const GURL kInitialUrl =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), kInitialUrl));

  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  int host_id = prerender_helper_.AddPrerender(page_url);
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

  if (!data_host) {
    loop.Run();
  }
  data_host->WaitForSourceData(/*num_source_data=*/1);
  const auto& source_data = data_host->source_data();

  EXPECT_EQ(source_data.size(), 1u);
  EXPECT_EQ(source_data.front().source_event_id, 5UL);
}

IN_PROC_BROWSER_TEST_F(AttributionSrcPrerenderBrowserTest,
                       SubresourceTriggerNotRegisteredOnPrerender) {
  EXPECT_CALL(mock_attribution_host(), RegisterDataHost).Times(0);

  const GURL kInitialUrl =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), kInitialUrl));

  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_conversion_redirect.html");
  int host_id = prerender_helper_.AddPrerender(page_url);
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

IN_PROC_BROWSER_TEST_F(AttributionSrcPrerenderBrowserTest,
                       SubresourceTriggerRegisteredOnActivatedPrerender) {
  std::unique_ptr<MockDataHost> data_host;
  base::RunLoop loop;
  EXPECT_CALL(mock_attribution_host(), RegisterDataHost)
      .WillOnce(
          [&](mojo::PendingReceiver<blink::mojom::AttributionDataHost> host,
              RegistrationType) {
            data_host = GetRegisteredDataHost(std::move(host));
            loop.Quit();
          });

  const GURL kInitialUrl =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), kInitialUrl));

  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_conversion_redirect.html");
  int host_id = prerender_helper_.AddPrerender(page_url);
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

  if (!data_host) {
    loop.Run();
  }
  data_host->WaitForTriggerData(/*num_trigger_data=*/1);
  const auto& trigger_data = data_host->trigger_data();

  ASSERT_EQ(trigger_data.size(), 1u);
  ASSERT_EQ(trigger_data.front().event_triggers.size(), 1u);
  EXPECT_EQ(trigger_data.front().event_triggers.front().data, 7u);
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

IN_PROC_BROWSER_TEST_F(AttributionSrcFencedFrameBrowserTest,
                       DefaultMode_SourceNotRegistered) {
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

IN_PROC_BROWSER_TEST_F(AttributionSrcFencedFrameBrowserTest,
                       OpaqueAdsMode_SourceRegistered) {
  GURL main_url = https_server()->GetURL("b.test", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  GURL fenced_frame_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");

  RenderFrameHost* parent = web_contents()->GetPrimaryMainFrame();

  RenderFrameHost* fenced_frame_host = fenced_frame_helper_->CreateFencedFrame(
      parent, fenced_frame_url, net::OK,
      blink::FencedFrame::DeprecatedFencedFrameMode::kOpaqueAds);

  ASSERT_NE(fenced_frame_host, nullptr);
  EXPECT_TRUE(fenced_frame_host->IsFencedFrameRoot());

  std::unique_ptr<MockDataHost> data_host;
  base::RunLoop loop;

  EXPECT_CALL(mock_attribution_host(), RegisterDataHost)
      .WillOnce(
          [&](mojo::PendingReceiver<blink::mojom::AttributionDataHost> host,
              RegistrationType) {
            data_host = GetRegisteredDataHost(std::move(host));
            loop.Quit();
          });

  EXPECT_TRUE(ExecJs(
      fenced_frame_host,
      JsReplace(
          "createAttributionSrcImg($1);",
          https_server()->GetURL("c.test", "/register_source_headers.html"))));

  if (!data_host) {
    loop.Run();
  }

  data_host->WaitForSourceData(/*num_source_data=*/1);
  EXPECT_EQ(data_host->source_data().size(), 1u);
}

class AttributionSrcCrossAppWebEnabledBrowserTest
    : public AttributionSrcBrowserTest {
 public:
  AttributionSrcCrossAppWebEnabledBrowserTest() = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list{
      blink::features::kAttributionReportingCrossAppWeb};
};

IN_PROC_BROWSER_TEST_F(AttributionSrcCrossAppWebEnabledBrowserTest,
                       Img_SetsSupportHeader) {
  // Create a separate server as we cannot register a `ControllableHttpResponse`
  // after the server starts.
  auto https_server = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  net::test_server::RegisterDefaultHandlers(https_server.get());
  https_server->ServeFilesFromSourceDirectory(
      "content/test/data/attribution_reporting");
  https_server->ServeFilesFromSourceDirectory("content/test/data");

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
  ASSERT_EQ(register_response1->http_request()->headers.at(
                "Attribution-Reporting-Support"),
            "web");

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader("Location", "/register_source2");
  register_response1->Send(http_response->ToResponseString());
  register_response1->Done();

  // Ensure that redirect requests also contain the headers.
  register_response2->WaitForRequest();
  ASSERT_EQ(register_response2->http_request()->headers.at(
                "Attribution-Reporting-Support"),
            "web");
}

#if BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(
    AttributionSrcCrossAppWebEnabledBrowserTest,
    OsLevelEnabledPriorToRendererInitialization_SetsSupportHeader) {
  // Create a separate server as we cannot register a `ControllableHttpResponse`
  // after the server starts.
  auto https_server = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  net::test_server::RegisterDefaultHandlers(https_server.get());
  https_server->ServeFilesFromSourceDirectory(
      "content/test/data/attribution_reporting");
  https_server->ServeFilesFromSourceDirectory("content/test/data");

  auto register_response1 =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/register_source1");
  auto register_response2 =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/register_source2");
  ASSERT_TRUE(https_server->Start());

  AttributionOsLevelManagerAndroid::ScopedOsSupportForTesting
      scoped_os_support_setting(
          attribution_reporting::mojom::OsSupport::kEnabled);

  GURL page_url =
      https_server->GetURL("b.test", "/page_with_impression_creator.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), page_url));

  GURL register_url = https_server->GetURL("d.test", "/register_source1");
  ASSERT_TRUE(ExecJs(web_contents(),
                     JsReplace("createAttributionSrcImg($1);", register_url)));

  register_response1->WaitForRequest();
  ASSERT_EQ(register_response1->http_request()->headers.at(
                "Attribution-Reporting-Support"),
            "web, os");

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader("Location", "/register_source2");
  register_response1->Send(http_response->ToResponseString());
  register_response1->Done();

  // Ensure that redirect requests also contain the header.
  register_response2->WaitForRequest();
  ASSERT_EQ(register_response2->http_request()->headers.at(
                "Attribution-Reporting-Support"),
            "web, os");
}

IN_PROC_BROWSER_TEST_F(
    AttributionSrcCrossAppWebEnabledBrowserTest,
    OsLevelEnabledPostRendererInitialization_SetsSupportHeader) {
  // Create a separate server as we cannot register a `ControllableHttpResponse`
  // after the server starts.
  auto https_server = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  net::test_server::RegisterDefaultHandlers(https_server.get());
  https_server->ServeFilesFromSourceDirectory(
      "content/test/data/attribution_reporting");
  https_server->ServeFilesFromSourceDirectory("content/test/data");

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

  AttributionOsLevelManagerAndroid::ScopedOsSupportForTesting
      scoped_os_support_setting(
          attribution_reporting::mojom::OsSupport::kEnabled);

  GURL register_url = https_server->GetURL("d.test", "/register_source1");
  ASSERT_TRUE(ExecJs(web_contents(),
                     JsReplace("createAttributionSrcImg($1);", register_url)));

  register_response1->WaitForRequest();
  ASSERT_EQ(register_response1->http_request()->headers.at(
                "Attribution-Reporting-Support"),
            "web, os");

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader("Location", "/register_source2");
  register_response1->Send(http_response->ToResponseString());
  register_response1->Done();

  // Ensure that redirect requests also contain the header.
  register_response2->WaitForRequest();
  ASSERT_EQ(register_response2->http_request()->headers.at(
                "Attribution-Reporting-Support"),
            "web, os");
}

struct OsRegistrationTestCase {
  const char* name;
  const char* header;
  std::vector<GURL> expected_os_sources;
  std::vector<GURL> expected_os_triggers;
};

class AttributionSrcCrossAppWebEnabledOsRegistrationBrowserTest
    : public AttributionSrcCrossAppWebEnabledBrowserTest,
      public ::testing::WithParamInterface<OsRegistrationTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    AttributionSrcCrossAppWebEnabledOsRegistrationBrowserTest,
    ::testing::Values(
        OsRegistrationTestCase{
            .name = "source",
            .header = "Attribution-Reporting-Register-OS-Source",
            .expected_os_sources = {GURL("https://r.test/x")},
        },
        OsRegistrationTestCase{
            .name = "trigger",
            .header = "Attribution-Reporting-Register-OS-Trigger",
            .expected_os_triggers = {GURL("https://r.test/x")},
        }),
    [](const auto& info) { return info.param.name; });  // test name generator

IN_PROC_BROWSER_TEST_P(
    AttributionSrcCrossAppWebEnabledOsRegistrationBrowserTest,
    Register) {
  const auto& test_case = GetParam();

  // Create a separate server as we cannot register a `ControllableHttpResponse`
  // after the server starts.
  auto https_server = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  net::test_server::RegisterDefaultHandlers(https_server.get());
  https_server->ServeFilesFromSourceDirectory(
      "content/test/data/attribution_reporting");
  https_server->ServeFilesFromSourceDirectory("content/test/data");

  std::unique_ptr<MockDataHost> data_host;
  base::RunLoop loop;
  EXPECT_CALL(mock_attribution_host(), RegisterDataHost)
      .WillOnce(
          [&](mojo::PendingReceiver<blink::mojom::AttributionDataHost> host,
              RegistrationType) {
            data_host = GetRegisteredDataHost(std::move(host));
            loop.Quit();
          });

  auto register_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/register");
  ASSERT_TRUE(https_server->Start());

  AttributionOsLevelManagerAndroid::ScopedOsSupportForTesting
      scoped_os_support_setting(
          attribution_reporting::mojom::OsSupport::kEnabled);

  GURL page_url =
      https_server->GetURL("b.test", "/page_with_impression_creator.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), page_url));

  GURL register_url = https_server->GetURL("d.test", "/register");
  ASSERT_TRUE(ExecJs(web_contents(),
                     JsReplace("createAttributionSrcImg($1);", register_url)));

  register_response->WaitForRequest();

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->AddCustomHeader(test_case.header, R"("https://r.test/x")");
  register_response->Send(http_response->ToResponseString());
  register_response->Done();

  if (!data_host) {
    loop.Run();
  }
  data_host->WaitForOsSources(test_case.expected_os_sources.size());
  data_host->WaitForOsTriggers(test_case.expected_os_triggers.size());

  EXPECT_EQ(data_host->os_sources(), test_case.expected_os_sources);
  EXPECT_EQ(data_host->os_triggers(), test_case.expected_os_triggers);
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace content
