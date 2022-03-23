// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "content/browser/attribution_reporting/attribution_manager_impl.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/navigation/impression.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::SizeIs;

MATCHER_P(AggregatableKeyIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.key, result_listener);
}

MATCHER_P(AggregatableKeyHighBitsIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.high_bits, result_listener);
}

MATCHER_P(AggregatableKeyLowBitsIs, matcher, "") {
  return ExplainMatchResult(matcher, arg.low_bits, result_listener);
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
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "content/test/data/attribution_reporting");
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    net::test_server::RegisterDefaultHandlers(https_server_.get());
    https_server_->ServeFilesFromSourceDirectory(
        "content/test/data/attribution_reporting");
    https_server_->ServeFilesFromSourceDirectory("content/test/data");
    SetupCrossSiteRedirector(https_server_.get());
    ASSERT_TRUE(https_server_->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Sets up the blink runtime feature for ConversionMeasurement.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  WebContents* web_contents() { return shell()->web_contents(); }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

class AttributionSrcBasicSourceRegisteredBrowserTest
    : public AttributionSrcBrowserTest,
      public ::testing::WithParamInterface<base::StringPiece> {};

IN_PROC_BROWSER_TEST_P(AttributionSrcBasicSourceRegisteredBrowserTest,
                       SourceRegistered) {
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  MockAttributionHost host(web_contents());
  std::unique_ptr<MockDataHost> data_host;
  base::RunLoop loop;
  EXPECT_CALL(host, RegisterDataHost)
      .WillOnce(
          [&](mojo::PendingReceiver<blink::mojom::AttributionDataHost> host) {
            data_host = GetRegisteredDataHost(std::move(host));
            loop.Quit();
          });

  GURL register_url =
      https_server()->GetURL("c.test", "/register_source_headers.html");

  base::StringPiece create_source_js = GetParam();
  EXPECT_TRUE(
      ExecJs(web_contents(), JsReplace(create_source_js, register_url)));
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
  EXPECT_THAT(source_data.front()->aggregatable_source->keys, IsEmpty());
}

// Ensure that basic source registration works with both the img attributionsrc
// attribute and the registerSource JS call.
INSTANTIATE_TEST_SUITE_P(
    AttributionSrcSourceRegistrations,
    AttributionSrcBasicSourceRegisteredBrowserTest,
    ::testing::Values("createAttributionSrcImg($1);",
                      "window.attributionReporting.registerSource($1);"));

IN_PROC_BROWSER_TEST_F(AttributionSrcBrowserTest,
                       AttributionSrcAnchor_SourceRegistered) {
  SourceObserver source_observer(web_contents());
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  std::unique_ptr<MockDataHost> data_host;
  blink::AttributionSrcToken expected_token;
  MockAttributionHost host(web_contents());
  EXPECT_CALL(host, RegisterNavigationDataHost)
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
  EXPECT_TRUE(last_impression.attribution_src_token);
  EXPECT_EQ(*last_impression.attribution_src_token, expected_token);

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
  MockAttributionHost host(web_contents());
  EXPECT_CALL(host, RegisterNavigationDataHost)
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
  EXPECT_TRUE(last_impression.attribution_src_token);
  EXPECT_EQ(*last_impression.attribution_src_token, expected_token);

  // Verify the attributionsrc data was registered with the browser process.
  EXPECT_TRUE(data_host);

  // TODO(johnidel): Verify that the data host receives the correct callback.
  // Direct use of MockDataHost flakes rarely. See
  // AttributionSrcNavigationSourceAndTrigger_ReportSent in
  // AttributionsBrowserTest.
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
  MockAttributionHost host(web_contents());
  EXPECT_CALL(host, RegisterNavigationDataHost)
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

  MockAttributionHost host(web_contents());
  std::unique_ptr<MockDataHost> data_host;
  base::RunLoop loop;
  EXPECT_CALL(host, RegisterDataHost)
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

  MockAttributionHost host(web_contents());
  std::unique_ptr<MockDataHost> data_host;
  base::RunLoop loop;
  EXPECT_CALL(host, RegisterDataHost)
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
  EXPECT_THAT(
      source_data.front()->aggregatable_source->keys,
      UnorderedElementsAre(
          Pair("key1",
               Pointee(AllOf(
                   Field(&blink::mojom::AttributionAggregatableKey::high_bits,
                         0),
                   Field(&blink::mojom::AttributionAggregatableKey::low_bits,
                         5)))),
          Pair("key2",
               Pointee(AllOf(
                   Field(&blink::mojom::AttributionAggregatableKey::high_bits,
                         0),
                   Field(&blink::mojom::AttributionAggregatableKey::low_bits,
                         345))))));
}

IN_PROC_BROWSER_TEST_F(AttributionSrcBrowserTest,
                       AttributionSrcImgRedirect_MultipleSourcesRegistered) {
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  MockAttributionHost host(web_contents());
  std::unique_ptr<MockDataHost> data_host;
  base::RunLoop loop;
  EXPECT_CALL(host, RegisterDataHost)
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

  MockAttributionHost host(web_contents());
  std::unique_ptr<MockDataHost> data_host;
  base::RunLoop loop;
  EXPECT_CALL(host, RegisterDataHost)
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
  SetupCrossSiteRedirector(https_server.get());

  auto register_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/register_source");
  ASSERT_TRUE(https_server->Start());

  GURL page_url =
      https_server->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  MockAttributionHost host(web_contents());
  std::unique_ptr<MockDataHost> data_host;
  base::RunLoop loop;
  EXPECT_CALL(host, RegisterDataHost)
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
                       AttributionSrcImg_TriggerRegistered) {
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  MockAttributionHost host(web_contents());
  std::unique_ptr<MockDataHost> data_host;
  base::RunLoop loop;
  EXPECT_CALL(host, RegisterDataHost)
      .WillOnce(
          [&](mojo::PendingReceiver<blink::mojom::AttributionDataHost> host) {
            data_host = GetRegisteredDataHost(std::move(host));
            loop.Quit();
          });

  GURL register_url =
      https_server()->GetURL("c.test", "/register_trigger_headers.html");

  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("createAttributionSrcImg($1);", register_url)));
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
  EXPECT_EQ(trigger_data.front()->event_triggers.front()->data, 10u);
  EXPECT_THAT(
      trigger_data.front()->event_triggers.front()->filters->filter_values,
      IsEmpty());
  EXPECT_THAT(
      trigger_data.front()->event_triggers.front()->not_filters->filter_values,
      IsEmpty());
  EXPECT_THAT(trigger_data.front()->aggregatable_trigger->trigger_data,
              IsEmpty());
  EXPECT_THAT(trigger_data.front()->aggregatable_trigger->values, IsEmpty());
}

IN_PROC_BROWSER_TEST_F(AttributionSrcBrowserTest,
                       AttributionSrcImg_TriggerRegisteredAllParams) {
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  MockAttributionHost host(web_contents());
  std::unique_ptr<MockDataHost> data_host;
  base::RunLoop loop;
  EXPECT_CALL(host, RegisterDataHost)
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

  EXPECT_THAT(
      trigger_data.front()->aggregatable_trigger->trigger_data,
      ElementsAre(Pointee(
          AllOf(AggregatableKeyIs(Pointee(AllOf(AggregatableKeyHighBitsIs(0),
                                                AggregatableKeyLowBitsIs(1)))),
                SourceKeysAre(ElementsAre("key"))))));

  EXPECT_THAT(trigger_data.front()->aggregatable_trigger->values,
              ElementsAre(Pair("key", 123)));
}

IN_PROC_BROWSER_TEST_F(
    AttributionSrcBrowserTest,
    AttributionSrcImg_TriggerRegisteredWithAggregatableTrigger) {
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  MockAttributionHost host(web_contents());
  std::unique_ptr<MockDataHost> data_host;
  base::RunLoop loop;
  EXPECT_CALL(host, RegisterDataHost)
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
      trigger_data.front()->aggregatable_trigger->trigger_data,
      ElementsAre(
          Pointee(AllOf(
              AggregatableKeyIs(Pointee(AllOf(AggregatableKeyHighBitsIs(0),
                                              AggregatableKeyLowBitsIs(1)))),
              SourceKeysAre(ElementsAre("key1")),
              FiltersAre(Pointee(
                  FilterValuesAre(ElementsAre(Pair("a", ElementsAre("b")))))),
              NotFiltersAre(Pointee(
                  FilterValuesAre(ElementsAre(Pair("c", IsEmpty()))))))),
          Pointee(AllOf(
              AggregatableKeyIs(Pointee(AllOf(AggregatableKeyHighBitsIs(0),
                                              AggregatableKeyLowBitsIs(0)))),
              SourceKeysAre(IsEmpty()),
              FiltersAre(Pointee(FilterValuesAre(IsEmpty()))),
              NotFiltersAre(Pointee(
                  FilterValuesAre(ElementsAre(Pair("d", ElementsAre("e", "f")),
                                              Pair("g", IsEmpty())))))))));

  EXPECT_THAT(trigger_data.front()->aggregatable_trigger->values,
              ElementsAre(Pair("key1", 123), Pair("key2", 456)));
}

IN_PROC_BROWSER_TEST_F(AttributionSrcBrowserTest,
                       AttributionSrcImg_InvalidTriggerJsonIgnored) {
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  MockAttributionHost host(web_contents());
  std::unique_ptr<MockDataHost> data_host;
  base::RunLoop loop;
  EXPECT_CALL(host, RegisterDataHost)
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
  EXPECT_EQ(trigger_data.front()->event_triggers.front()->data, 10u);
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
  SetupCrossSiteRedirector(https_server.get());

  auto register_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/register_trigger");
  ASSERT_TRUE(https_server->Start());

  GURL page_url =
      https_server->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  MockAttributionHost host(web_contents());
  std::unique_ptr<MockDataHost> data_host;
  base::RunLoop loop;
  EXPECT_CALL(host, RegisterDataHost)
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
  EXPECT_THAT(trigger_data.front()->aggregatable_trigger->trigger_data,
              SizeIs(2));
  EXPECT_THAT(trigger_data.front()->aggregatable_trigger->values, SizeIs(2));
}

IN_PROC_BROWSER_TEST_F(AttributionSrcBrowserTest,
                       AttributionSrcImgTriggerThenSource_SourceIgnored) {
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  MockAttributionHost host(web_contents());
  std::unique_ptr<MockDataHost> data_host;
  base::RunLoop loop;
  EXPECT_CALL(host, RegisterDataHost)
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
  EXPECT_EQ(trigger_data.back()->event_triggers.front()->data, 10u);

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
  SetupCrossSiteRedirector(https_server.get());

  auto register_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/register_source");
  ASSERT_TRUE(https_server->Start());

  GURL page_url =
      https_server->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  MockAttributionHost host(web_contents());
  std::unique_ptr<MockDataHost> data_host;
  base::RunLoop loop;
  EXPECT_CALL(host, RegisterDataHost)
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
      R"({"source_event_id":"9", "destination":"https://d.test"})");
  http_response->AddCustomHeader(
      "Attribution-Reporting-Register-Aggregatable-Source", "");
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
  EXPECT_THAT(source_data.back()->aggregatable_source->keys, SizeIs(2));
}

IN_PROC_BROWSER_TEST_F(AttributionSrcBrowserTest,
                       AttributionSrcRegisterSourceJS_TriggerIgnored) {
  GURL page_url =
      https_server()->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  MockAttributionHost host(web_contents());
  std::unique_ptr<MockDataHost> data_host;
  base::RunLoop loop;
  EXPECT_CALL(host, RegisterDataHost)
      .WillOnce(
          [&](mojo::PendingReceiver<blink::mojom::AttributionDataHost> host) {
            data_host = GetRegisteredDataHost(std::move(host));
            loop.Quit();
          });

  GURL register_url =
      https_server()->GetURL("c.test", "/register_trigger_source_trigger.html");

  EXPECT_TRUE(
      ExecJs(web_contents(),
             JsReplace("window.attributionReporting.registerSource($1);",
                       register_url)));
  if (!data_host)
    loop.Run();
  data_host->WaitForSourceData(/*num_source_data=*/1);
  const auto& source_data = data_host->source_data();

  EXPECT_EQ(source_data.size(), 1u);

  // Only the source should be processed.
  EXPECT_EQ(source_data.front()->source_event_id, 5u);

  // The triggers should be ignored.
  EXPECT_EQ(data_host->trigger_data().size(), 0u);
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
  MockAttributionHost host(web_contents());
  EXPECT_CALL(host, RegisterDataHost).Times(0);

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
  MockAttributionHost host(web_contents());
  std::unique_ptr<MockDataHost> data_host;
  base::RunLoop loop;
  EXPECT_CALL(host, RegisterDataHost)
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

class AttributionSrcInvalidFiltersBrowserTest
    : public AttributionSrcBrowserTest,
      public ::testing::WithParamInterface<const char*> {};

IN_PROC_BROWSER_TEST_P(AttributionSrcInvalidFiltersBrowserTest,
                       AttributionSrcImgFiltersInvalid_SourceDropped) {
  // Create a separate server as we cannot register a `ControllableHttpResponse`
  // after the server starts.
  auto https_server = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  net::test_server::RegisterDefaultHandlers(https_server.get());
  https_server->ServeFilesFromSourceDirectory(
      "content/test/data/attribution_reporting");
  https_server->ServeFilesFromSourceDirectory("content/test/data");
  SetupCrossSiteRedirector(https_server.get());

  auto register_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/register_source");
  ASSERT_TRUE(https_server->Start());

  GURL page_url =
      https_server->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  MockAttributionHost host(web_contents());
  std::unique_ptr<MockDataHost> data_host;
  base::RunLoop loop;
  EXPECT_CALL(host, RegisterDataHost)
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
      base::StrCat(
          {R"({"source_event_id":"9", "destination":"https://d.test", "filter_data":)",
           GetParam(), "}"}));
  http_response->AddCustomHeader("Location", "/register_source_headers.html");
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

IN_PROC_BROWSER_TEST_P(AttributionSrcInvalidFiltersBrowserTest,
                       AttributionSrcImgFiltersInvalid_TriggerDropped) {
  // Create a separate server as we cannot register a `ControllableHttpResponse`
  // after the server starts.
  auto https_server = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  net::test_server::RegisterDefaultHandlers(https_server.get());
  https_server->ServeFilesFromSourceDirectory(
      "content/test/data/attribution_reporting");
  https_server->ServeFilesFromSourceDirectory("content/test/data");
  SetupCrossSiteRedirector(https_server.get());

  auto register_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/register_trigger");
  ASSERT_TRUE(https_server->Start());

  GURL page_url =
      https_server->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  MockAttributionHost host(web_contents());
  std::unique_ptr<MockDataHost> data_host;
  base::RunLoop loop;
  EXPECT_CALL(host, RegisterDataHost)
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
      "Attribution-Reporting-Register-Event-Trigger",
      base::StrCat({R"([{"trigger_data":"9","filters":)", GetParam(), "}]"}));
  http_response->AddCustomHeader("Location", "/register_trigger_headers.html");
  register_response->Send(http_response->ToResponseString());
  register_response->Done();

  if (!data_host)
    loop.Run();
  data_host->WaitForTriggerData(/*num_trigger_data=*/1);
  const auto& trigger_data = data_host->trigger_data();

  // Only the second trigger is registered.
  EXPECT_EQ(trigger_data.size(), 1u);
  EXPECT_EQ(trigger_data.front()->event_triggers.size(), 1u);
  EXPECT_EQ(trigger_data.front()->event_triggers.front()->data, 10u);
}

INSTANTIATE_TEST_SUITE_P(
    AttributionSrcInvalidFilters,
    AttributionSrcInvalidFiltersBrowserTest,
    ::testing::Values(R"("x")",        // not a dictionary
                      R"({"a":"y"})",  // dictionary value isn't an array
                      R"({"b":[8]})"   // array value isn't a string
                      ));

class AttributionSrcFilterSizeBrowserTest
    : public AttributionSrcBrowserTest,
      public ::testing::WithParamInterface<AttributionFilterSizeTestCase> {};

IN_PROC_BROWSER_TEST_P(AttributionSrcFilterSizeBrowserTest,
                       AttributionSrcImgExcessiveFilterSize_SourceDropped) {
  const AttributionFilterSizeTestCase& test_case = GetParam();

  // Create a separate server as we cannot register a `ControllableHttpResponse`
  // after the server starts.
  auto https_server = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  net::test_server::RegisterDefaultHandlers(https_server.get());
  https_server->ServeFilesFromSourceDirectory(
      "content/test/data/attribution_reporting");
  https_server->ServeFilesFromSourceDirectory("content/test/data");
  SetupCrossSiteRedirector(https_server.get());

  auto register_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/register_source");
  ASSERT_TRUE(https_server->Start());

  GURL page_url =
      https_server->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  MockAttributionHost host(web_contents());
  std::unique_ptr<MockDataHost> data_host;
  base::RunLoop loop;
  EXPECT_CALL(host, RegisterDataHost)
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

  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("source_event_id", "9");
  dict.SetStringKey("destination", "https://d.test");

  base::Value filter_data(base::Value::Type::DICTIONARY);
  for (auto [filter, values] : test_case.AsMap()) {
    base::Value list(base::Value::Type::LIST);
    for (auto value : values) {
      list.Append(std::move(value));
    }
    filter_data.SetKey(std::move(filter), std::move(list));
  }
  dict.SetKey("filter_data", std::move(filter_data));

  std::string json;
  EXPECT_TRUE(base::JSONWriter::Write(dict, &json));

  http_response->AddCustomHeader("Attribution-Reporting-Register-Source",
                                 std::move(json));
  http_response->AddCustomHeader("Location", "/register_source_headers.html");
  register_response->Send(http_response->ToResponseString());
  register_response->Done();

  if (!data_host)
    loop.Run();

  const size_t expected_sources = test_case.valid ? 2 : 1;
  data_host->WaitForSourceData(/*num_source_data=*/expected_sources);
  const auto& source_data = data_host->source_data();

  EXPECT_EQ(source_data.size(), expected_sources);
  EXPECT_EQ(source_data.back()->source_event_id, 5UL);
  EXPECT_EQ(source_data.back()->destination,
            url::Origin::Create(GURL("https://d.test")));
}

IN_PROC_BROWSER_TEST_P(AttributionSrcFilterSizeBrowserTest,
                       AttributionSrcImgExcessiveFilterSize_TriggerDropped) {
  const AttributionFilterSizeTestCase& test_case = GetParam();

  // Create a separate server as we cannot register a `ControllableHttpResponse`
  // after the server starts.
  auto https_server = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  net::test_server::RegisterDefaultHandlers(https_server.get());
  https_server->ServeFilesFromSourceDirectory(
      "content/test/data/attribution_reporting");
  https_server->ServeFilesFromSourceDirectory("content/test/data");
  SetupCrossSiteRedirector(https_server.get());

  auto register_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server.get(), "/register_trigger");
  ASSERT_TRUE(https_server->Start());

  GURL page_url =
      https_server->GetURL("b.test", "/page_with_impression_creator.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  MockAttributionHost host(web_contents());
  std::unique_ptr<MockDataHost> data_host;
  base::RunLoop loop;
  EXPECT_CALL(host, RegisterDataHost)
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

  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("trigger_data", "9");

  base::Value filter_data(base::Value::Type::DICTIONARY);
  for (auto [filter, values] : test_case.AsMap()) {
    base::Value list(base::Value::Type::LIST);
    for (auto value : values) {
      list.Append(std::move(value));
    }
    filter_data.SetKey(std::move(filter), std::move(list));
  }
  dict.SetKey("filters", std::move(filter_data));

  base::Value list(base::Value::Type::LIST);
  list.Append(std::move(dict));

  std::string json;
  EXPECT_TRUE(base::JSONWriter::Write(list, &json));

  http_response->AddCustomHeader("Attribution-Reporting-Register-Event-Trigger",
                                 std::move(json));
  http_response->AddCustomHeader("Location", "/register_trigger_headers.html");
  register_response->Send(http_response->ToResponseString());
  register_response->Done();

  if (!data_host)
    loop.Run();

  const size_t expected_triggers = test_case.valid ? 2 : 1;
  data_host->WaitForTriggerData(/*num_trigger_data=*/expected_triggers);
  const auto& trigger_data = data_host->trigger_data();

  EXPECT_EQ(trigger_data.size(), expected_triggers);
  EXPECT_EQ(trigger_data.back()->event_triggers.size(), 1u);
  EXPECT_EQ(trigger_data.back()->event_triggers.front()->data, 10u);
}

INSTANTIATE_TEST_SUITE_P(
    AttributionSrcFilterSizes,
    AttributionSrcFilterSizeBrowserTest,
    ::testing::ValuesIn(kAttributionFilterSizeTestCases),
    /*name_generator=*/
    [](const ::testing::TestParamInfo<AttributionFilterSizeTestCase>& info) {
      return info.param.description;
    });

// TODO(apaseltiner): Add tests for overlong filters.

}  // namespace content
