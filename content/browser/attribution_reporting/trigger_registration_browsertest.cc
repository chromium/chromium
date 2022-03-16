// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "content/browser/attribution_reporting/attribution_manager_impl.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/resource_load_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/conversions/conversions.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::Pointee;

// Well known path for registering conversions.
const std::string kWellKnownUrl =
    ".well-known/attribution-reporting/trigger-attribution";

}  // namespace

class AttributionTriggerDisabledBrowserTest : public ContentBrowserTest {
 public:
  AttributionTriggerDisabledBrowserTest() {
    AttributionManagerImpl::RunInMemoryForTesting();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "content/test/data/attribution_reporting");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    net::test_server::RegisterDefaultHandlers(https_server_.get());
    https_server_->ServeFilesFromSourceDirectory(
        "content/test/data/attribution_reporting");
    SetupCrossSiteRedirector(https_server_.get());
    ASSERT_TRUE(https_server_->Start());
  }

  WebContents* web_contents() { return shell()->web_contents(); }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

IN_PROC_BROWSER_TEST_F(
    AttributionTriggerDisabledBrowserTest,
    ConversionRegisteredWithoutOTEnabled_NoConversionDataReceived) {
  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("/page_with_conversion_redirect.html")));
  MockAttributionHost host(web_contents());
  EXPECT_CALL(host, RegisterConversion).Times(0);

  EXPECT_TRUE(ExecJs(web_contents(), "registerConversion({data: 123})"));

  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
}

class AttributionTriggerRegistrationBrowserTest
    : public AttributionTriggerDisabledBrowserTest {
 public:
  AttributionTriggerRegistrationBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Sets up the blink runtime feature for ConversionMeasurement.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }
};

// Test that full conversion path does not cause any failure when a conversion
// registration mojo is received.
IN_PROC_BROWSER_TEST_F(AttributionTriggerRegistrationBrowserTest,
                       ConversionRegistration_NoCrash) {
  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("/page_with_conversion_redirect.html")));
  EXPECT_TRUE(ExecJs(web_contents(), "createTrackingPixel(\"" + kWellKnownUrl +
                                         "?trigger-data=100\");"));

  ASSERT_NO_FATAL_FAILURE(
      EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank"))));
}

IN_PROC_BROWSER_TEST_F(AttributionTriggerRegistrationBrowserTest,
                       ConversionRegistered_ConversionDataReceived) {
  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("/page_with_conversion_redirect.html")));

  base::RunLoop loop;
  MockAttributionHost host(web_contents());
  EXPECT_CALL(
      host,
      RegisterConversion(Pointee(AllOf(
          Field(&blink::mojom::Conversion::conversion_data, 123UL),
          Field(&blink::mojom::Conversion::event_source_trigger_data, 0UL),
          Field(&blink::mojom::Conversion::priority, 0)))))
      .WillOnce([&]() { loop.Quit(); });

  EXPECT_TRUE(ExecJs(web_contents(), "registerConversion({data: 123})"));
  loop.Run();
}

IN_PROC_BROWSER_TEST_F(AttributionTriggerRegistrationBrowserTest,
                       ConversionRegistered_EventSourceTriggerDataReceived) {
  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("/page_with_conversion_redirect.html")));

  base::RunLoop loop;
  MockAttributionHost host(web_contents());
  EXPECT_CALL(
      host,
      RegisterConversion(Pointee(AllOf(
          Field(&blink::mojom::Conversion::conversion_data, 123UL),
          Field(&blink::mojom::Conversion::event_source_trigger_data, 456UL)))))
      .WillOnce([&]() { loop.Quit(); });

  EXPECT_TRUE(
      ExecJs(web_contents(),
             "registerConversion({data: 123, eventSourceTriggerData: 456})"));
  loop.Run();
}

IN_PROC_BROWSER_TEST_F(AttributionTriggerRegistrationBrowserTest,
                       ConversionRegistered_PriorityReceived) {
  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("/page_with_conversion_redirect.html")));

  base::RunLoop loop;
  MockAttributionHost host(web_contents());
  EXPECT_CALL(
      host, RegisterConversion(Pointee(
                AllOf(Field(&blink::mojom::Conversion::conversion_data, 123UL),
                      Field(&blink::mojom::Conversion::priority, 456)))))
      .WillOnce([&]() { loop.Quit(); });

  EXPECT_TRUE(
      ExecJs(web_contents(), "registerConversion({data: 123, priority: 456})"));
  loop.Run();
}

IN_PROC_BROWSER_TEST_F(AttributionTriggerRegistrationBrowserTest,
                       PermissionsPolicyDisabled_ConversionNotRegistered) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "/page_with_conversion_measurement_disabled.html")));
  MockAttributionHost host(web_contents());
  EXPECT_CALL(host, RegisterConversion).Times(0);

  GURL redirect_url = embedded_test_server()->GetURL(
      "/server-redirect?" + kWellKnownUrl + "trigger-data=200");
  ResourceLoadObserver load_observer(shell());
  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("createTrackingPixel($1);", redirect_url)));
  load_observer.WaitForResourceCompletion(redirect_url);

  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
}

IN_PROC_BROWSER_TEST_F(AttributionTriggerRegistrationBrowserTest,
                       ConversionRegistrationNotRedirect_NotReceived) {
  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("/page_with_conversion_redirect.html")));
  MockAttributionHost host(web_contents());
  EXPECT_CALL(host, RegisterConversion).Times(0);

  GURL registration_url =
      embedded_test_server()->GetURL("/" + kWellKnownUrl + "?trigger-data=200");

  // Create a load observer that will wait for the redirect to complete. If a
  // conversion was registered, this redirect would never complete.
  ResourceLoadObserver load_observer(shell());
  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("createTrackingPixel($1);", registration_url)));
  load_observer.WaitForResourceCompletion(registration_url);

  // Conversion mojo messages are sent on the same message pipe as navigation
  // messages. Because the conversion would have been sequenced prior to the
  // navigation message, it would be observed before the NavigateToURL() call
  // finishes.
  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
}

IN_PROC_BROWSER_TEST_F(
    AttributionTriggerRegistrationBrowserTest,
    ConversionRegistrationNotSameOriginRedirect_NotReceived) {
  EXPECT_TRUE(NavigateToURL(
      shell(),
      https_server()->GetURL("c.test", "/page_with_conversion_redirect.html")));
  MockAttributionHost host(web_contents());
  EXPECT_CALL(host, RegisterConversion).Times(0);

  // Create a url that does the following redirect chain b.test ->
  // a.test/.well-known/...; this conversion registration should not be allowed,
  // a.test did not initiate the redirect to the reporting endpoint.
  GURL redirect_url = https_server()->GetURL(
      "a.test", "/" + kWellKnownUrl + "?trigger-data=200");
  GURL registration_url = https_server()->GetURL(
      "b.test", "/server-redirect?" + redirect_url.spec());

  // Create a load observer that will wait for the redirect to complete. If a
  // conversion was registered, this redirect would never complete.
  ResourceLoadObserver load_observer(shell());
  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("createTrackingPixel($1);", registration_url)));
  load_observer.WaitForResourceCompletion(registration_url);

  // Conversion mojo messages are sent on the same message pipe as navigation
  // messages. Because the conversion would have been sequenced prior to the
  // navigation message, it would be observed before the NavigateToURL() call
  // finishes.
  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
}

IN_PROC_BROWSER_TEST_F(AttributionTriggerRegistrationBrowserTest,
                       ConversionRegistrationIsSameOriginRedirect_Received) {
  EXPECT_TRUE(NavigateToURL(
      shell(),
      https_server()->GetURL("c.test", "/page_with_conversion_redirect.html")));

  base::RunLoop loop;
  MockAttributionHost host(web_contents());
  EXPECT_CALL(
      host,
      RegisterConversion(Pointee(AllOf(
          Field(&blink::mojom::Conversion::conversion_data, 200UL),
          Field(&blink::mojom::Conversion::event_source_trigger_data, 0UL)))))
      .WillOnce([&]() { loop.Quit(); });

  // Create a url that does the following redirect chain b.test -> a.test ->
  // a.test/.well-known/...; this conversion registration should be allowed.
  GURL well_known_url = https_server()->GetURL(
      "a.test", "/" + kWellKnownUrl + "?trigger-data=200");
  GURL redirect_url = https_server()->GetURL(
      "a.test", "/server-redirect?" + well_known_url.spec());
  GURL registration_url = https_server()->GetURL(
      "b.test", "/server-redirect?" + redirect_url.spec());

  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("createTrackingPixel($1);", registration_url)));
  loop.Run();
}

IN_PROC_BROWSER_TEST_F(AttributionTriggerRegistrationBrowserTest,
                       ConversionRegistrationInPreload_NotReceived) {
  MockAttributionHost host(web_contents());
  EXPECT_CALL(host, RegisterConversion).Times(0);

  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(
                                 "/page_with_preload_conversion_ping.html")));

  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
}

IN_PROC_BROWSER_TEST_F(AttributionTriggerRegistrationBrowserTest,
                       ConversionRegistrationNoData_ReceivedZero) {
  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("/page_with_conversion_redirect.html")));

  base::RunLoop loop;
  MockAttributionHost host(web_contents());
  // Conversion data and event source trigger data should be defaulted to 0.
  EXPECT_CALL(
      host,
      RegisterConversion(Pointee(AllOf(
          Field(&blink::mojom::Conversion::conversion_data, 0UL),
          Field(&blink::mojom::Conversion::event_source_trigger_data, 0UL)))))
      .WillOnce([&]() { loop.Quit(); });

  EXPECT_TRUE(ExecJs(web_contents(), "createTrackingPixel(\"server-redirect?" +
                                         kWellKnownUrl + "\");"));
  loop.Run();
}

IN_PROC_BROWSER_TEST_F(AttributionTriggerRegistrationBrowserTest,
                       ConversionRegisteredFromChildFrame_Received) {
  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("/page_with_subframe_conversion.html")));

  base::RunLoop loop;
  MockAttributionHost host(web_contents());
  EXPECT_CALL(
      host,
      RegisterConversion(Pointee(AllOf(
          Field(&blink::mojom::Conversion::conversion_data, 200u),
          Field(&blink::mojom::Conversion::event_source_trigger_data, 0u)))))
      .WillOnce([&]() { loop.Quit(); });

  GURL redirect_url = embedded_test_server()->GetURL(
      "/server-redirect?" + kWellKnownUrl + "?trigger-data=200");
  ResourceLoadObserver load_observer(shell());
  EXPECT_TRUE(ExecJs(ChildFrameAt(web_contents()->GetMainFrame(), 0),
                     JsReplace("createTrackingPixel($1);", redirect_url)));
  loop.Run();

  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
}

IN_PROC_BROWSER_TEST_F(
    AttributionTriggerRegistrationBrowserTest,
    ConversionRegisteredFromChildFrameWithoutPermissionPolicy_NotReceived) {
  GURL page_url = embedded_test_server()->GetURL("/page_with_iframe.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  GURL subframe_url =
      https_server()->GetURL("b.test", "/page_with_conversion_redirect.html");
  NavigateIframeToURL(web_contents(), "test_iframe", subframe_url);

  MockAttributionHost host(web_contents());
  EXPECT_CALL(host, RegisterConversion).Times(0);

  GURL redirect_url = https_server()->GetURL(
      "b.test", "/server-redirect?" + kWellKnownUrl + "?trigger-data=200");

  ResourceLoadObserver load_observer(shell());
  EXPECT_TRUE(ExecJs(ChildFrameAt(web_contents()->GetMainFrame(), 0),
                     JsReplace("createTrackingPixel($1);", redirect_url)));
  load_observer.WaitForResourceCompletion(redirect_url);

  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
}

IN_PROC_BROWSER_TEST_F(
    AttributionTriggerRegistrationBrowserTest,
    ConversionRegisteredFromChildFrameWithPermissionPolicy_Received) {
  GURL page_url = embedded_test_server()->GetURL("/page_with_iframe.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  EXPECT_TRUE(ExecJs(shell(), R"(
      let frame = document.getElementById('test_iframe');
      frame.setAttribute('allow', 'attribution-reporting');)"));

  GURL subframe_url =
      https_server()->GetURL("b.test", "/page_with_conversion_redirect.html");
  NavigateIframeToURL(web_contents(), "test_iframe", subframe_url);

  base::RunLoop loop;
  MockAttributionHost host(web_contents());
  EXPECT_CALL(
      host,
      RegisterConversion(Pointee(AllOf(
          Field(&blink::mojom::Conversion::conversion_data, 200u),
          Field(&blink::mojom::Conversion::event_source_trigger_data, 0u)))))
      .WillOnce([&]() { loop.Quit(); });

  GURL redirect_url = https_server()->GetURL(
      "b.test", "/server-redirect?" + kWellKnownUrl + "?trigger-data=200");

  ResourceLoadObserver load_observer(shell());
  EXPECT_TRUE(ExecJs(ChildFrameAt(web_contents()->GetMainFrame(), 0),
                     JsReplace("createTrackingPixel($1);", redirect_url)));
  loop.Run();

  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
}

IN_PROC_BROWSER_TEST_F(
    AttributionTriggerRegistrationBrowserTest,
    RegisterWithDifferentUrlTypes_ConversionReceivedOrIgnored) {
  const char kSecureHost[] = "a.test";
  struct {
    std::string page_host;
    std::string redirect_host;
    bool expected_conversion;
  } kTestCases[] = {{.page_host = "localhost",
                     .redirect_host = "localhost",
                     .expected_conversion = true},
                    {.page_host = "127.0.0.1",
                     .redirect_host = "127.0.0.1",
                     .expected_conversion = true},
                    {.page_host = "insecure.com",
                     .redirect_host = "insecure.com",
                     .expected_conversion = false},
                    {.page_host = kSecureHost,
                     .redirect_host = kSecureHost,
                     .expected_conversion = true},
                    {.page_host = "insecure.com",
                     .redirect_host = kSecureHost,
                     .expected_conversion = false},
                    {.page_host = kSecureHost,
                     .redirect_host = "insecure.com",
                     .expected_conversion = false}};

  for (const auto& test_case : kTestCases) {
    base::RunLoop loop;
    MockAttributionHost host(web_contents());
    if (test_case.expected_conversion) {
      EXPECT_CALL(
          host, RegisterConversion(Pointee(
                    Field(&blink::mojom::Conversion::conversion_data, 200UL))))
          .WillOnce([&]() { loop.Quit(); });
    } else {
      EXPECT_CALL(host, RegisterConversion).Times(0);
    }

    // Secure hosts must be served from the https server.
    net::EmbeddedTestServer* page_server = (test_case.page_host == kSecureHost)
                                               ? https_server()
                                               : embedded_test_server();
    EXPECT_TRUE(NavigateToURL(
        shell(), page_server->GetURL(test_case.page_host,
                                     "/page_with_conversion_redirect.html")));

    net::EmbeddedTestServer* redirect_server =
        (test_case.redirect_host == kSecureHost) ? https_server()
                                                 : embedded_test_server();
    GURL redirect_url = redirect_server->GetURL(
        test_case.redirect_host,
        "/server-redirect?" + kWellKnownUrl + "?trigger-data=200");
    EXPECT_TRUE(ExecJs(
        web_contents(),
        JsReplace("window.fetch($1, {mode: 'no-cors'}).catch(console.log);",
                  redirect_url)));

    if (test_case.expected_conversion)
      loop.Run();

    // Navigate the page. By the time the navigation finishes, we will have
    // received any conversion mojo messages.
    EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
  }
}

IN_PROC_BROWSER_TEST_F(
    AttributionTriggerRegistrationBrowserTest,
    ConversionRegisteredFromChildFrameInInsecureContext_NotReceived) {
  // Start with localhost(secure) iframing a.test (insecure) iframing
  // localhost(secure). This context is insecure since the middle iframe in the
  // ancestor chain is insecure.

  GURL main_frame_url =
      embedded_test_server()->GetURL("/page_with_iframe.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), main_frame_url));

  EXPECT_TRUE(ExecJs(shell(), R"(
        let frame = document.getElementById('test_iframe');
        frame.setAttribute('allow', 'attribution-reporting');)"));

  GURL middle_iframe_url = embedded_test_server()->GetURL(
      "insecure.example", "/page_with_iframe.html");
  NavigateIframeToURL(web_contents(), "test_iframe", middle_iframe_url);

  RenderFrameHost* middle_iframe =
      ChildFrameAt(web_contents()->GetMainFrame(), 0);

  GURL innermost_iframe_url(
      embedded_test_server()->GetURL("/page_with_conversion_redirect.html"));
  EXPECT_TRUE(ExecJs(middle_iframe, JsReplace(R"(
      let frame = document.getElementById('test_iframe');
      frame.setAttribute('allow', 'attribution-reporting');
      frame.src = $1;)",
                                              innermost_iframe_url)));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  MockAttributionHost host(web_contents());
  EXPECT_CALL(host, RegisterConversion).Times(0);

  GURL redirect_url = embedded_test_server()->GetURL(
      "/server-redirect?" + kWellKnownUrl + "?trigger-data=200");

  ResourceLoadObserver load_observer(shell());
  EXPECT_TRUE(ExecJs(ChildFrameAt(middle_iframe, 0),
                     JsReplace("createTrackingPixel($1);", redirect_url)));
  load_observer.WaitForResourceCompletion(redirect_url);

  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
}

IN_PROC_BROWSER_TEST_F(AttributionTriggerRegistrationBrowserTest,
                       NonAttributionSrcImg_TriggerRegistered) {
  EXPECT_TRUE(NavigateToURL(
      shell(),
      https_server()->GetURL("c.test", "/page_with_conversion_redirect.html")));

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
                     JsReplace("createTrackingPixel($1);", register_url)));

  if (!data_host)
    loop.Run();

  data_host->WaitForTriggerData(/*num_trigger_data=*/1);
  const auto& trigger_data = data_host->trigger_data();

  EXPECT_EQ(trigger_data.size(), 1u);
  EXPECT_EQ(trigger_data.front()->reporting_origin,
            url::Origin::Create(register_url));
  EXPECT_THAT(
      trigger_data.front()->event_triggers,
      ElementsAre(Pointee(Field(&blink::mojom::EventTriggerData::data, 1)),
                  Pointee(Field(&blink::mojom::EventTriggerData::data, 2))));
}

IN_PROC_BROWSER_TEST_F(
    AttributionTriggerRegistrationBrowserTest,
    NonAttributionSrcImgRedirect_MultipleTriggersRegistered) {
  EXPECT_TRUE(NavigateToURL(
      shell(),
      https_server()->GetURL("c.test", "/page_with_conversion_redirect.html")));

  MockAttributionHost host(web_contents());
  std::vector<std::unique_ptr<MockDataHost>> data_hosts;
  base::RunLoop loop;
  EXPECT_CALL(host, RegisterDataHost)
      .WillRepeatedly(
          [&](mojo::PendingReceiver<blink::mojom::AttributionDataHost> host) {
            data_hosts.push_back(GetRegisteredDataHost(std::move(host)));
            if (data_hosts.size() == 2)
              loop.Quit();
          });

  GURL register_url = https_server()->GetURL(
      "c.test", "/register_trigger_headers_and_redirect.html");

  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("createTrackingPixel($1);", register_url)));

  if (data_hosts.size() != 2)
    loop.Run();

  data_hosts.front()->WaitForTriggerData(/*num_trigger_data=*/1);
  const auto& trigger_data1 = data_hosts.front()->trigger_data();

  EXPECT_EQ(trigger_data1.size(), 1u);
  EXPECT_EQ(trigger_data1.front()->reporting_origin,
            url::Origin::Create(register_url));
  EXPECT_THAT(
      trigger_data1.front()->event_triggers,
      ElementsAre(Pointee(Field(&blink::mojom::EventTriggerData::data, 5))));

  data_hosts.back()->WaitForTriggerData(/*num_trigger_data=*/1);
  const auto& trigger_data2 = data_hosts.back()->trigger_data();

  EXPECT_EQ(trigger_data2.size(), 1u);
  EXPECT_EQ(trigger_data2.front()->reporting_origin,
            url::Origin::Create(register_url));
  EXPECT_THAT(
      trigger_data2.front()->event_triggers,
      ElementsAre(Pointee(Field(&blink::mojom::EventTriggerData::data, 10))));
}

}  // namespace content
