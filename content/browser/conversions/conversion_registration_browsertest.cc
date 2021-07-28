// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <memory>

#include "base/bind.h"
#include "content/browser/conversions/conversion_host.h"
#include "content/browser/conversions/conversion_manager_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/resource_load_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/mojom/conversions/conversions.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

// Well known path for registering conversions.
const std::string kWellKnownUrl =
    ".well-known/attribution-reporting/trigger-attribution";

}  // namespace

// A mock conversion host which waits until a conversion registration
// mojo message is received. Tracks the last seen conversion data.
class TestConversionHost : public ConversionHost {
 public:
  explicit TestConversionHost(WebContents* contents)
      : ConversionHost(contents) {
    SetReceiverImplForTesting(this);
  }

  ~TestConversionHost() override { SetReceiverImplForTesting(nullptr); }

  void RegisterConversion(blink::mojom::ConversionPtr conversion) override {
    last_conversion_ = std::move(conversion);
    num_conversions_++;

    // Don't quit the run loop if we have not seen the expected number of
    // conversions.
    if (num_conversions_ < expected_num_conversions_)
      return;
    conversion_waiter_.Quit();
  }

  // Returns the last conversion data after |expected_num_conversions| have been
  // observed.
  uint64_t WaitForNumConversions(size_t expected_num_conversions) {
    if (expected_num_conversions == num_conversions_)
      return last_conversion_->conversion_data;
    expected_num_conversions_ = expected_num_conversions;
    conversion_waiter_.Run();
    return last_conversion_->conversion_data;
  }

  size_t num_conversions() { return num_conversions_; }

  const blink::mojom::ConversionPtr& last_conversion() const {
    return last_conversion_;
  }

 private:
  blink::mojom::ConversionPtr last_conversion_ = nullptr;
  size_t num_conversions_ = 0;
  size_t expected_num_conversions_ = 0;
  base::RunLoop conversion_waiter_;
};

class ConversionDisabledBrowserTest : public ContentBrowserTest {
 public:
  ConversionDisabledBrowserTest() {
    ConversionManagerImpl::RunInMemoryForTesting();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "content/test/data/conversions");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    net::test_server::RegisterDefaultHandlers(https_server_.get());
    https_server_->ServeFilesFromSourceDirectory(
        "content/test/data/conversions");
    SetupCrossSiteRedirector(https_server_.get());
    ASSERT_TRUE(https_server_->Start());
  }

  WebContents* web_contents() { return shell()->web_contents(); }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

IN_PROC_BROWSER_TEST_F(
    ConversionDisabledBrowserTest,
    ConversionRegisteredWithoutOTEnabled_NoConversionDataReceived) {
  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("/page_with_conversion_redirect.html")));
  TestConversionHost host(web_contents());

  EXPECT_TRUE(ExecJs(web_contents(), "registerConversion({data: 123})"));

  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
  EXPECT_EQ(0u, host.num_conversions());
}

class ConversionRegistrationBrowserTest : public ConversionDisabledBrowserTest {
 public:
  ConversionRegistrationBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Sets up the blink runtime feature for ConversionMeasurement.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }
};

// Test that full conversion path does not cause any failure when a conversion
// registration mojo is received.
IN_PROC_BROWSER_TEST_F(ConversionRegistrationBrowserTest,
                       ConversionRegistration_NoCrash) {
  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("/page_with_conversion_redirect.html")));
  EXPECT_TRUE(ExecJs(web_contents(), "createTrackingPixel(\"" + kWellKnownUrl +
                                         "?trigger-data=100\");"));

  ASSERT_NO_FATAL_FAILURE(
      EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank"))));
}

IN_PROC_BROWSER_TEST_F(ConversionRegistrationBrowserTest,
                       ConversionRegistered_ConversionDataReceived) {
  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("/page_with_conversion_redirect.html")));
  TestConversionHost host(web_contents());

  EXPECT_TRUE(ExecJs(web_contents(), "registerConversion({data: 123})"));
  EXPECT_EQ(123UL, host.WaitForNumConversions(1));
  EXPECT_EQ(0UL, host.last_conversion()->event_source_trigger_data);
  EXPECT_EQ(0, host.last_conversion()->priority);
}

IN_PROC_BROWSER_TEST_F(ConversionRegistrationBrowserTest,
                       ConversionRegistered_EventSourceTriggerDataReceived) {
  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("/page_with_conversion_redirect.html")));
  TestConversionHost host(web_contents());

  EXPECT_TRUE(
      ExecJs(web_contents(),
             "registerConversion({data: 123, eventSourceTriggerData: 456})"));
  EXPECT_EQ(123UL, host.WaitForNumConversions(1));
  EXPECT_EQ(456UL, host.last_conversion()->event_source_trigger_data);
}

IN_PROC_BROWSER_TEST_F(ConversionRegistrationBrowserTest,
                       ConversionRegistered_PriorityReceived) {
  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("/page_with_conversion_redirect.html")));
  TestConversionHost host(web_contents());

  EXPECT_TRUE(
      ExecJs(web_contents(), "registerConversion({data: 123, priority: 456})"));
  EXPECT_EQ(123UL, host.WaitForNumConversions(1));
  EXPECT_EQ(456, host.last_conversion()->priority);
}

IN_PROC_BROWSER_TEST_F(ConversionRegistrationBrowserTest,
                       PermissionsPolicyDisabled_ConversionNotRegistered) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "/page_with_conversion_measurement_disabled.html")));
  TestConversionHost host(web_contents());

  GURL redirect_url = embedded_test_server()->GetURL(
      "/server-redirect?" + kWellKnownUrl + "trigger-data=200");
  ResourceLoadObserver load_observer(shell());
  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("createTrackingPixel($1);", redirect_url)));
  load_observer.WaitForResourceCompletion(redirect_url);

  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
  EXPECT_EQ(0u, host.num_conversions());
}

IN_PROC_BROWSER_TEST_F(ConversionRegistrationBrowserTest,
                       ConversionRegistrationNotRedirect_NotReceived) {
  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("/page_with_conversion_redirect.html")));
  TestConversionHost host(web_contents());

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
  EXPECT_EQ(0u, host.num_conversions());
}

IN_PROC_BROWSER_TEST_F(
    ConversionRegistrationBrowserTest,
    ConversionRegistrationNotSameOriginRedirect_NotReceived) {
  EXPECT_TRUE(NavigateToURL(
      shell(),
      https_server()->GetURL("c.test", "/page_with_conversion_redirect.html")));
  TestConversionHost host(web_contents());

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
  EXPECT_EQ(0u, host.num_conversions());
}

IN_PROC_BROWSER_TEST_F(ConversionRegistrationBrowserTest,
                       ConversionRegistrationIsSameOriginRedirect_Received) {
  EXPECT_TRUE(NavigateToURL(
      shell(),
      https_server()->GetURL("c.test", "/page_with_conversion_redirect.html")));
  TestConversionHost host(web_contents());

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
  EXPECT_EQ(200UL, host.WaitForNumConversions(1));
  EXPECT_EQ(0UL, host.last_conversion()->event_source_trigger_data);
}

IN_PROC_BROWSER_TEST_F(ConversionRegistrationBrowserTest,
                       ConversionRegistrationInPreload_NotReceived) {
  TestConversionHost host(web_contents());
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(
                                 "/page_with_preload_conversion_ping.html")));

  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
  EXPECT_EQ(0u, host.num_conversions());
}

IN_PROC_BROWSER_TEST_F(ConversionRegistrationBrowserTest,
                       ConversionRegistrationNoData_ReceivedZero) {
  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("/page_with_conversion_redirect.html")));
  TestConversionHost host(web_contents());

  EXPECT_TRUE(ExecJs(web_contents(), "createTrackingPixel(\"server-redirect?" +
                                         kWellKnownUrl + "\");"));

  // Conversion data and event source trigger data should be defaulted to 0.
  EXPECT_EQ(0UL, host.WaitForNumConversions(1));
  EXPECT_EQ(0UL, host.last_conversion()->event_source_trigger_data);
}

IN_PROC_BROWSER_TEST_F(ConversionRegistrationBrowserTest,
                       ConversionRegisteredFromChildFrame_Received) {
  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("/page_with_subframe_conversion.html")));
  TestConversionHost host(web_contents());

  GURL redirect_url = embedded_test_server()->GetURL(
      "/server-redirect?" + kWellKnownUrl + "?trigger-data=200");
  ResourceLoadObserver load_observer(shell());
  EXPECT_TRUE(ExecJs(ChildFrameAt(web_contents()->GetMainFrame(), 0),
                     JsReplace("createTrackingPixel($1);", redirect_url)));
  EXPECT_EQ(200u, host.WaitForNumConversions(1));
  EXPECT_EQ(0u, host.last_conversion()->event_source_trigger_data);

  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
  EXPECT_EQ(1u, host.num_conversions());
}

IN_PROC_BROWSER_TEST_F(
    ConversionRegistrationBrowserTest,
    ConversionRegisteredFromChildFrameWithoutPermissionPolicy_NotReceived) {
  GURL page_url = embedded_test_server()->GetURL("/page_with_iframe.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  GURL subframe_url =
      https_server()->GetURL("b.test", "/page_with_conversion_redirect.html");
  NavigateIframeToURL(web_contents(), "test_iframe", subframe_url);

  TestConversionHost host(web_contents());

  GURL redirect_url = https_server()->GetURL(
      "b.test", "/server-redirect?" + kWellKnownUrl + "?trigger-data=200");

  ResourceLoadObserver load_observer(shell());
  EXPECT_TRUE(ExecJs(ChildFrameAt(web_contents()->GetMainFrame(), 0),
                     JsReplace("createTrackingPixel($1);", redirect_url)));
  load_observer.WaitForResourceCompletion(redirect_url);

  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
  EXPECT_EQ(0u, host.num_conversions());
}

IN_PROC_BROWSER_TEST_F(
    ConversionRegistrationBrowserTest,
    ConversionRegisteredFromChildFrameWithPermissionPolicy_Received) {
  GURL page_url = embedded_test_server()->GetURL("/page_with_iframe.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), page_url));

  EXPECT_TRUE(ExecJs(shell(), R"(
      let frame = document.getElementById('test_iframe');
      frame.setAttribute('allow', 'attribution-reporting');)"));

  GURL subframe_url =
      https_server()->GetURL("b.test", "/page_with_conversion_redirect.html");
  NavigateIframeToURL(web_contents(), "test_iframe", subframe_url);

  TestConversionHost host(web_contents());

  GURL redirect_url = https_server()->GetURL(
      "b.test", "/server-redirect?" + kWellKnownUrl + "?trigger-data=200");

  ResourceLoadObserver load_observer(shell());
  EXPECT_TRUE(ExecJs(ChildFrameAt(web_contents()->GetMainFrame(), 0),
                     JsReplace("createTrackingPixel($1);", redirect_url)));
  EXPECT_EQ(200u, host.WaitForNumConversions(1));
  EXPECT_EQ(0u, host.last_conversion()->event_source_trigger_data);

  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
  EXPECT_EQ(1u, host.num_conversions());
}

IN_PROC_BROWSER_TEST_F(
    ConversionRegistrationBrowserTest,
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
    TestConversionHost host(web_contents());

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
      EXPECT_EQ(200UL, host.WaitForNumConversions(1));

    // Navigate the page. By the time the navigation finishes, we will have
    // received any conversion mojo messages.
    EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
    EXPECT_EQ(test_case.expected_conversion, host.num_conversions());
  }
}

}  // namespace content
