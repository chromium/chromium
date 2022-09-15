// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/attribution_reporting/attribution_manager_impl.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
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
#include "url/gurl.h"

namespace content {

namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

MATCHER_P(AggregationKeyPieceIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.key_piece, result_listener);
}

MATCHER_P(SourceKeysAre, matcher, "") {
  return ExplainMatchResult(matcher, arg.source_keys, result_listener);
}

MATCHER_P(FiltersAre, matcher, "") {
  return ExplainMatchResult(matcher, arg.filters, result_listener);
}

MATCHER_P(NotFiltersAre, matcher, "") {
  return ExplainMatchResult(matcher, arg.not_filters, result_listener);
}

MATCHER_P(FilterValuesAre, matcher, "") {
  return ExplainMatchResult(matcher, arg.filter_values, result_listener);
}

}  // namespace

class AttributionSrcBrowserTest : public ContentBrowserTest {
 public:
  AttributionSrcBrowserTest() {
    AttributionManagerImpl::RunInMemoryForTesting();
  }

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

    mock_attribution_host_ = MockAttributionHost::Override(web_contents());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Sets up the blink runtime feature for ConversionMeasurement.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  WebContents* web_contents() { return shell()->web_contents(); }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  MockAttributionHost& mock_attribution_host() {
    return *mock_attribution_host_;
  }

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  base::raw_ptr<MockAttributionHost, DanglingUntriaged> mock_attribution_host_;
};

IN_PROC_BROWSER_TEST_F(AttributionSrcBrowserTest, SourceRegistered) {
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  std::unique_ptr<MockDataHost> data_host;
  base::RunLoop loop;
  EXPECT_CALL(mock_attribution_host(), RegisterDataHost)
      .WillOnce(
          [&](mojo::PendingReceiver<blink::mojom::AttributionDataHost> host) {
            data_host = GetRegisteredDataHost(std::move(host));
            loop.Quit();
          });

  GURL register_url =
      https_server()->GetURL("c.test", "/register_source_headers.html");

  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("createAttributionSrcImg($1);", register_url)));
  if (!data_host)
    loop.Run();
  data_host->WaitForSourceData(/*num_source_data=*/1);
  const auto& source_data = data_host->source_data();

  EXPECT_EQ(source_data.size(), 1u);
  EXPECT_EQ(source_data.front()->source_event_id, 5UL);
  EXPECT_EQ(source_data.front()->destination,
            url::Origin::Create(GURL("https://d.test")));
  EXPECT_EQ(source_data.front()->priority, 0);
  EXPECT_EQ(source_data.front()->expiry, absl::nullopt);
  EXPECT_FALSE(source_data.front()->debug_key);
  EXPECT_THAT(source_data.front()->filter_data->filter_values, IsEmpty());
  EXPECT_THAT(source_data.front()->aggregation_keys, IsEmpty());
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
            [&](mojo::PendingReceiver<blink::mojom::AttributionDataHost> host) {
              data_host = GetRegisteredDataHost(std::move(host));
              data_host->receiver().set_disconnect_handler(
                  disconnect_loop.QuitClosure());
              loop.Quit();
            });

    GURL register_url =
        https_server()->GetURL("c.test", "/register_source_headers.html");

    EXPECT_TRUE(
        ExecJs(web_contents(), JsReplace(registration_js, register_url)));
    if (!data_host)
      loop.Run();
    data_host->WaitForSourceData(/*num_source_data=*/1);
    const auto& source_data = data_host->source_data();
    // Regression test for crbug.com/1336797. This will timeout flakily if the
    // data host isn't disconnected promptly.
    disconnect_loop.Run();

    EXPECT_EQ(source_data.size(), 1u);
    EXPECT_EQ(source_data.front()->source_event_id, 5UL);
    EXPECT_EQ(source_data.front()->destination,
              url::Origin::Create(GURL("https://d.test")));
    EXPECT_EQ(source_data.front()->priority, 0);
    EXPECT_EQ(source_data.front()->expiry, absl::nullopt);
    EXPECT_FALSE(source_data.front()->debug_key);
    EXPECT_THAT(source_data.front()->filter_data->filter_values, IsEmpty());
    EXPECT_THAT(source_data.front()->aggregation_keys, IsEmpty());
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
          [&](mojo::PendingReceiver<blink::mojom::AttributionDataHost> host) {
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
  if (!source_data_host)
    source_loop.Run();
  source_data_host->WaitForSourceData(/*num_source_data=*/1);

  if (!trigger_data_host)
    trigger_loop.Run();
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

  // Verify the attributionsrc data was registered with the browser process.
  EXPECT_TRUE(data_host);

  // TODO(johnidel): Verify that the data host receives the correct callback.
  // Direct use of MockDataHost flakes rarely. See
  // AttributionSrcNavigationSourceAndTrigger_ReportSent in
  // AttributionsBrowserTest.
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
  blink::Impression last_impression = source_observer.Wait();
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
  blink::Impression last_impression = source_observer.Wait();
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

  TestNavigationObserver observer(web_contents());

  // This attributionsrc will only be handled properly if the value is
  // URL-decoded before being passed to the attributionsrc loader.
  EXPECT_TRUE(ExecJs(web_contents(), R"(
  window.open("page_with_conversion_redirect.html", "_top",
  "attributionsrc=register_source%3Fa%3Db%26c%3Dd");)"));

  register_response->WaitForRequest();
  register_response->Done();

  // TODO(crbug.com/1322525): Remove this once we use a pure mock.
  observer.Wait();

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

  TestNavigationObserver observer(web_contents());

  // This attributionsrc will only be handled properly if the URL's original
  // case is retained before being passed to the attributionsrc loader.
  EXPECT_TRUE(ExecJs(web_contents(), R"(
  window.open("page_with_conversion_redirect.html", "_top",
  "attributionsrc=register_source%3Fa%3DB%26C%3Dd");)"));

  register_response->WaitForRequest();
  register_response->Done();

  // TODO(crbug.com/1322525): Remove this once we use a pure mock.
  observer.Wait();

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

  TestNavigationObserver observer(web_contents());

  // Ensure that the special handling of the original case for attributionsrc
  // features works with non-ASCII characters.
  EXPECT_TRUE(ExecJs(web_contents(), R"(
  window.open("page_with_conversion_redirect.html", "_top",
  "attributionsrc=😀");)"));

  register_response->WaitForRequest();
  register_response->Done();

  // TODO(crbug.com/1322525): Remove this once we use a pure mock.
  observer.Wait();

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
                       AttributionSrcImg_SourceRegisteredWithOptionalParams) {
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  std::unique_ptr<MockDataHost> data_host;
  base::RunLoop loop;
  EXPECT_CALL(mock_attribution_host(), RegisterDataHost)
      .WillOnce(
          [&](mojo::PendingReceiver<blink::mojom::AttributionDataHost> host) {
            data_host = GetRegisteredDataHost(std::move(host));
            loop.Quit();
          });

  GURL register_url = https_server()->GetURL(
      "c.test", "/register_source_headers_all_params.html");

  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("createAttributionSrcImg($1);", register_url)));
  if (!data_host)
    loop.Run();
  data_host->WaitForSourceData(/*num_source_data=*/1);
  const auto& source_data = data_host->source_data();

  EXPECT_EQ(source_data.size(), 1u);
  EXPECT_EQ(source_data.front()->source_event_id, 5UL);
  EXPECT_EQ(source_data.front()->destination,
            url::Origin::Create(GURL("https://d.test")));
  EXPECT_EQ(source_data.front()->priority, 10);
  EXPECT_EQ(source_data.front()->expiry, base::Seconds(1000));
  EXPECT_EQ(source_data.front()->debug_key,
            blink::mojom::AttributionDebugKey::New(789));
  EXPECT_THAT(source_data.front()->filter_data->filter_values,
              UnorderedElementsAre(Pair("a", IsEmpty()),
                                   Pair("b", ElementsAre("1", "2"))));
}

IN_PROC_BROWSER_TEST_F(
    AttributionSrcBrowserTest,
    AttributionSrcImg_SourceRegisteredWithAggregatableSource) {
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  std::unique_ptr<MockDataHost> data_host;
  base::RunLoop loop;
  EXPECT_CALL(mock_attribution_host(), RegisterDataHost)
      .WillOnce(
          [&](mojo::PendingReceiver<blink::mojom::AttributionDataHost> host) {
            data_host = GetRegisteredDataHost(std::move(host));
            loop.Quit();
          });

  GURL register_url = https_server()->GetURL(
      "c.test", "/register_aggregatable_source_headers.html");

  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("createAttributionSrcImg($1);", register_url)));
  if (!data_host)
    loop.Run();
  data_host->WaitForSourceData(/*num_source_data=*/1);
  const auto& source_data = data_host->source_data();

  EXPECT_EQ(source_data.size(), 1u);
  EXPECT_EQ(source_data.front()->source_event_id, 5UL);
  EXPECT_EQ(source_data.front()->destination,
            url::Origin::Create(GURL("https://d.test")));
  EXPECT_EQ(source_data.front()->priority, 0);
  EXPECT_EQ(source_data.front()->expiry, absl::nullopt);
  EXPECT_FALSE(source_data.front()->debug_key);
  EXPECT_THAT(source_data.front()->aggregation_keys,
              UnorderedElementsAre(
                  Pair("key1", absl::MakeUint128(/*high=*/0, /*low=*/5)),
                  Pair("key2", absl::MakeUint128(/*high=*/0, /*low=*/345))));
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
          [&](mojo::PendingReceiver<blink::mojom::AttributionDataHost> host) {
            data_host = GetRegisteredDataHost(std::move(host));
            loop.Quit();
          });

  GURL register_url = https_server()->GetURL(
      "c.test", "/register_source_headers_and_redirect.html");

  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("createAttributionSrcImg($1);", register_url)));
  if (!data_host)
    loop.Run();
  data_host->WaitForSourceData(/*num_source_data=*/2);
  const auto& source_data = data_host->source_data();

  EXPECT_EQ(source_data.size(), 2u);
  EXPECT_EQ(source_data.front()->source_event_id, 1UL);
  EXPECT_EQ(source_data.front()->destination,
            url::Origin::Create(GURL("https://d.test")));
  EXPECT_EQ(source_data.back()->source_event_id, 5UL);
  EXPECT_EQ(source_data.back()->destination,
            url::Origin::Create(GURL("https://d.test")));
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
          [&](mojo::PendingReceiver<blink::mojom::AttributionDataHost> host) {
            data_host = GetRegisteredDataHost(std::move(host));
            loop.Quit();
          });

  GURL register_url = https_server()->GetURL(
      "c.test", "/register_source_headers_and_redirect_invalid.html");

  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("createAttributionSrcImg($1);", register_url)));
  if (!data_host)
    loop.Run();
  data_host->WaitForSourceData(/*num_source_data=*/1);
  const auto& source_data = data_host->source_data();

  // Only the second source is registered.
  EXPECT_EQ(source_data.size(), 1u);
  EXPECT_EQ(source_data.back()->source_event_id, 5UL);
  EXPECT_EQ(source_data.back()->destination,
            url::Origin::Create(GURL("https://d.test")));
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
          [&](mojo::PendingReceiver<blink::mojom::AttributionDataHost> host) {
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
      "Attribution-Reporting-Register-Source",
      R"({"source_event_id":"5", "destination":"https://d.test"})");
  register_response->Send(http_response->ToResponseString());
  register_response->Done();

  if (!data_host)
    loop.Run();
  data_host->WaitForSourceData(/*num_source_data=*/1);
  const auto& source_data = data_host->source_data();

  // Only the second source is registered.
  EXPECT_EQ(source_data.size(), 1u);
  EXPECT_EQ(source_data.back()->source_event_id, 5UL);
  EXPECT_EQ(source_data.back()->destination,
            url::Origin::Create(GURL("https://d.test")));
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

  GURL register_url2 = http_server->GetURL("d.test", "/register_source2");
  ASSERT_TRUE(ExecJs(web_contents(), JsReplace(R"(
    window.open($1, '_blank', 'attributionsrc=');)",
                                               register_url2)));

  response2->WaitForRequest();
  ASSERT_FALSE(base::Contains(response2->http_request()->headers,
                              "Attribution-Reporting-Eligible"));
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
          [&](mojo::PendingReceiver<blink::mojom::AttributionDataHost> host) {
            data_host = GetRegisteredDataHost(std::move(host));
            loop.Quit();
          });

  GURL register_url =
      https_server()->GetURL("c.test", "/register_trigger_headers.html");

  const std::string& js_template = GetParam().second;
  EXPECT_TRUE(ExecJs(web_contents(), JsReplace(js_template, register_url)));
  if (!data_host)
    loop.Run();
  data_host->WaitForTriggerData(/*num_trigger_data=*/1);
  const auto& trigger_data = data_host->trigger_data();

  EXPECT_EQ(trigger_data.size(), 1u);
  EXPECT_EQ(trigger_data.front()->reporting_origin,
            url::Origin::Create(register_url));
  EXPECT_THAT(trigger_data.front()->filters->filter_values, IsEmpty());
  EXPECT_FALSE(trigger_data.front()->debug_key);
  EXPECT_EQ(trigger_data.front()->event_triggers.size(), 1u);
  EXPECT_EQ(trigger_data.front()->event_triggers.front()->data, 7u);
  EXPECT_THAT(
      trigger_data.front()->event_triggers.front()->filters->filter_values,
      IsEmpty());
  EXPECT_THAT(
      trigger_data.front()->event_triggers.front()->not_filters->filter_values,
      IsEmpty());
  EXPECT_THAT(trigger_data.front()->aggregatable_trigger_data, IsEmpty());
  EXPECT_THAT(trigger_data.front()->aggregatable_values, IsEmpty());
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
          [&](mojo::PendingReceiver<blink::mojom::AttributionDataHost> host) {
            data_host = GetRegisteredDataHost(std::move(host));
            loop.Quit();
          });

  GURL register_url = https_server()->GetURL(
      "c.test", "/register_trigger_headers_all_params.html");

  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("createAttributionSrcImg($1);", register_url)));
  if (!data_host)
    loop.Run();
  data_host->WaitForTriggerData(/*num_trigger_data=*/1);
  const auto& trigger_data = data_host->trigger_data();

  EXPECT_EQ(trigger_data.size(), 1u);
  EXPECT_EQ(trigger_data.front()->reporting_origin,
            url::Origin::Create(register_url));
  EXPECT_THAT(
      trigger_data.front()->filters->filter_values,
      ElementsAre(Pair("w", IsEmpty()), Pair("x", ElementsAre("y", "z"))));
  EXPECT_EQ(trigger_data.front()->debug_key,
            blink::mojom::AttributionDebugKey::New(789));
  EXPECT_EQ(trigger_data.front()->event_triggers.size(), 2u);

  // Verify first trigger.
  const auto& event_trigger_datas = trigger_data.front()->event_triggers;
  EXPECT_EQ(event_trigger_datas.front()->data, 1u);
  EXPECT_EQ(event_trigger_datas.front()->priority, 5);
  EXPECT_EQ(event_trigger_datas.front()->dedup_key->value, 1024u);
  EXPECT_THAT(event_trigger_datas.front()->filters->filter_values,
              ElementsAre(Pair("a", ElementsAre("b"))));
  EXPECT_THAT(event_trigger_datas.front()->not_filters->filter_values,
              ElementsAre(Pair("c", IsEmpty())));

  // Verify second trigger.
  EXPECT_EQ(event_trigger_datas.back()->data, 2u);
  EXPECT_EQ(event_trigger_datas.back()->priority, 10);
  EXPECT_FALSE(event_trigger_datas.back()->dedup_key);
  EXPECT_THAT(event_trigger_datas.back()->filters->filter_values, IsEmpty());
  EXPECT_THAT(
      event_trigger_datas.back()->not_filters->filter_values,
      ElementsAre(Pair("d", ElementsAre("e", "f")), Pair("g", IsEmpty())));

  EXPECT_THAT(trigger_data.front()->aggregatable_trigger_data,
              ElementsAre(Pointee(AllOf(AggregationKeyPieceIs(absl::MakeUint128(
                                            /*high=*/0, /*low=*/1)),
                                        SourceKeysAre(ElementsAre("key"))))));

  EXPECT_THAT(trigger_data.front()->aggregatable_values,
              ElementsAre(Pair("key", 123)));
}

IN_PROC_BROWSER_TEST_F(
    AttributionSrcBrowserTest,
    AttributionSrcImg_TriggerRegisteredWithAggregatableTrigger) {
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  std::unique_ptr<MockDataHost> data_host;
  base::RunLoop loop;
  EXPECT_CALL(mock_attribution_host(), RegisterDataHost)
      .WillOnce(
          [&](mojo::PendingReceiver<blink::mojom::AttributionDataHost> host) {
            data_host = GetRegisteredDataHost(std::move(host));
            loop.Quit();
          });

  GURL register_url = https_server()->GetURL(
      "c.test", "/register_aggregatable_trigger_data_headers.html");

  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("createAttributionSrcImg($1);", register_url)));
  if (!data_host)
    loop.Run();
  data_host->WaitForTriggerData(/*num_trigger_data=*/1);
  const auto& trigger_data = data_host->trigger_data();

  EXPECT_EQ(trigger_data.size(), 1u);
  EXPECT_EQ(trigger_data.front()->reporting_origin,
            url::Origin::Create(register_url));
  EXPECT_THAT(trigger_data.front()->event_triggers, IsEmpty());

  EXPECT_THAT(
      trigger_data.front()->aggregatable_trigger_data,
      ElementsAre(
          Pointee(AllOf(
              AggregationKeyPieceIs(absl::MakeUint128(/*high=*/0, /*low=*/1)),
              SourceKeysAre(ElementsAre("key1")),
              FiltersAre(Pointee(
                  FilterValuesAre(ElementsAre(Pair("a", ElementsAre("b")))))),
              NotFiltersAre(Pointee(
                  FilterValuesAre(ElementsAre(Pair("c", IsEmpty()))))))),
          Pointee(AllOf(
              AggregationKeyPieceIs(absl::MakeUint128(/*high=*/0, /*low=*/0)),
              SourceKeysAre(IsEmpty()),
              FiltersAre(Pointee(FilterValuesAre(IsEmpty()))),
              NotFiltersAre(Pointee(
                  FilterValuesAre(ElementsAre(Pair("d", ElementsAre("e", "f")),
                                              Pair("g", IsEmpty())))))))));

  EXPECT_THAT(trigger_data.front()->aggregatable_values,
              ElementsAre(Pair("key1", 123), Pair("key2", 456)));
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
          [&](mojo::PendingReceiver<blink::mojom::AttributionDataHost> host) {
            data_host = GetRegisteredDataHost(std::move(host));
            loop.Quit();
          });

  GURL register_url = https_server()->GetURL(
      "c.test", "/register_trigger_headers_then_redirect_invalid.html");

  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("createAttributionSrcImg($1);", register_url)));
  if (!data_host)
    loop.Run();
  data_host->WaitForTriggerData(/*num_trigger_data=*/1);
  const auto& trigger_data = data_host->trigger_data();

  EXPECT_EQ(trigger_data.size(), 1u);
  EXPECT_EQ(trigger_data.front()->reporting_origin,
            url::Origin::Create(register_url));
  EXPECT_EQ(trigger_data.front()->event_triggers.size(), 1u);
  EXPECT_EQ(trigger_data.front()->event_triggers.front()->data, 7u);
}

IN_PROC_BROWSER_TEST_F(AttributionSrcBrowserTest,
                       AttributionSrcImg_InvalidAggregatableTriggerDropped) {
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
          https_server.get(), "/register_trigger");
  ASSERT_TRUE(https_server->Start());

  GURL page_url =
      https_server->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  std::unique_ptr<MockDataHost> data_host;
  base::RunLoop loop;
  EXPECT_CALL(mock_attribution_host(), RegisterDataHost)
      .WillOnce(
          [&](mojo::PendingReceiver<blink::mojom::AttributionDataHost> host) {
            data_host = GetRegisteredDataHost(std::move(host));
            loop.Quit();
          });

  GURL register_url = https_server->GetURL("d.test", "/register_trigger");
  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("createAttributionSrcImg($1);", register_url)));

  register_response->WaitForRequest();
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader(
      "Attribution-Reporting-Register-Aggregatable-Trigger-Data", "");
  http_response->AddCustomHeader(
      "Attribution-Reporting-Register-Aggregatable-Values", "");
  http_response->AddCustomHeader(
      "Location", "/register_aggregatable_trigger_data_headers.html");
  register_response->Send(http_response->ToResponseString());
  register_response->Done();

  if (!data_host)
    loop.Run();
  data_host->WaitForTriggerData(/*num_trigger_data=*/1);
  const auto& trigger_data = data_host->trigger_data();

  // Only the second trigger is registered.
  EXPECT_EQ(trigger_data.size(), 1u);
  EXPECT_THAT(trigger_data.front()->aggregatable_trigger_data, SizeIs(2));
  EXPECT_THAT(trigger_data.front()->aggregatable_values, SizeIs(2));
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
          [&](mojo::PendingReceiver<blink::mojom::AttributionDataHost> host) {
            data_host = GetRegisteredDataHost(std::move(host));
            loop.Quit();
          });

  GURL register_url =
      https_server()->GetURL("c.test", "/register_trigger_source_trigger.html");

  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("createAttributionSrcImg($1);", register_url)));
  if (!data_host)
    loop.Run();
  data_host->WaitForTriggerData(/*num_trigger_data=*/2);
  const auto& trigger_data = data_host->trigger_data();

  EXPECT_EQ(trigger_data.size(), 2u);
  EXPECT_EQ(trigger_data.front()->reporting_origin,
            url::Origin::Create(register_url));

  // Both triggers should be processed.
  EXPECT_EQ(trigger_data.front()->event_triggers.front()->data, 5u);
  EXPECT_EQ(trigger_data.back()->event_triggers.front()->data, 7u);

  // Middle redirect source should be ignored.
  EXPECT_EQ(data_host->source_data().size(), 0u);
}

IN_PROC_BROWSER_TEST_F(AttributionSrcBrowserTest,
                       AttributionSrcImg_InvalidAggregatableSourceDropped) {
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
          [&](mojo::PendingReceiver<blink::mojom::AttributionDataHost> host) {
            data_host = GetRegisteredDataHost(std::move(host));
            loop.Quit();
          });

  GURL register_url = https_server->GetURL("d.test", "/register_source");
  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("createAttributionSrcImg($1);", register_url)));

  register_response->WaitForRequest();
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader(
      "Attribution-Reporting-Register-Source",
      R"({"source_event_id":"9", "destination":"https://d.test", "aggregation_keys":""})");
  http_response->AddCustomHeader("Location",
                                 "/register_aggregatable_source_headers.html");
  register_response->Send(http_response->ToResponseString());
  register_response->Done();

  if (!data_host)
    loop.Run();
  data_host->WaitForSourceData(/*num_source_data=*/1);
  const auto& source_data = data_host->source_data();

  // Only the second source is registered.
  EXPECT_EQ(source_data.size(), 1u);
  EXPECT_EQ(source_data.back()->source_event_id, 5UL);
  EXPECT_EQ(source_data.back()->destination,
            url::Origin::Create(GURL("https://d.test")));
  EXPECT_THAT(source_data.back()->aggregation_keys, SizeIs(2));
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
          [&](mojo::PendingReceiver<blink::mojom::AttributionDataHost> host) {
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

  if (!data_host)
    loop.Run();
  data_host->WaitForSourceData(/*num_source_data=*/1);
  const auto& source_data = data_host->source_data();

  EXPECT_EQ(source_data.size(), 1u);
  EXPECT_EQ(source_data.front()->source_event_id, 5UL);
}

class AttributionSrcFencedFrameBrowserTest
    : public AttributionSrcBrowserTest,
      public ::testing::WithParamInterface<
          blink::features::FencedFramesImplementationType> {
 public:
  AttributionSrcFencedFrameBrowserTest() {
    switch (GetParam()) {
      case blink::features::FencedFramesImplementationType::kMPArch:
        fenced_frame_helper_ = std::make_unique<test::FencedFrameTestHelper>();
        break;
      case blink::features::FencedFramesImplementationType::kShadowDOM:
        feature_list_.InitWithFeaturesAndParameters(
            {{blink::features::kFencedFrames,
              {{"implementation_type", "shadow_dom"}}},
             {features::kPrivacySandboxAdsAPIsOverride, {}}},
            /*disabled_features=*/{});
        break;
    }
  }

  ~AttributionSrcFencedFrameBrowserTest() override = default;

 protected:
  std::unique_ptr<test::FencedFrameTestHelper> fenced_frame_helper_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    AttributionSrcFencedFrameBrowserTest,
    ::testing::Values(
        blink::features::FencedFramesImplementationType::kShadowDOM,
        blink::features::FencedFramesImplementationType::kMPArch));

// TODO(crbug.com/1329240): Add test for opaque-ads mode fenced frames.

IN_PROC_BROWSER_TEST_P(AttributionSrcFencedFrameBrowserTest,
                       DefaultMode_SourceNotRegistered) {
  GURL main_url = https_server()->GetURL("b.test", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  GURL fenced_frame_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");

  RenderFrameHost* parent = web_contents()->GetPrimaryMainFrame();

  RenderFrameHost* fenced_frame_host;
  if (fenced_frame_helper_) {
    fenced_frame_host =
        fenced_frame_helper_->CreateFencedFrame(parent, fenced_frame_url);
  } else {
    EXPECT_TRUE(ExecJs(parent, R"(
      var fenced_frame = document.createElement('fencedframe');
      document.body.appendChild(fenced_frame);
    )"));

    fenced_frame_host = ChildFrameAt(parent, 0);

    TestFrameNavigationObserver observer(fenced_frame_host);
    EXPECT_TRUE(
        ExecJs(parent, JsReplace("fenced_frame.src = $1;", fenced_frame_url)));
    observer.Wait();
  }

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

}  // namespace content
