// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>

#include "content/browser/conversions/conversion_manager_impl.h"
#include "content/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/resource_load_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class DevToolsConversionBrowserTest : public DevToolsProtocolTest {
 public:
  DevToolsConversionBrowserTest() {
    ConversionManagerImpl::RunInMemoryForTesting();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Sets up the blink runtime feature for ConversionMeasurement.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
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

IN_PROC_BROWSER_TEST_F(DevToolsConversionBrowserTest,
                       ConversionRedirectIsReportedAsLoadingFinished) {
  // 1) Navigate to a site that causes 'the' conversion redirect.
  EXPECT_TRUE(NavigateToURL(
      shell(),
      https_server()->GetURL("a.test", "/page_with_conversion_redirect.html")));

  // 2) Open DevTools and enable Network domain.
  Attach();
  SendCommand("Network.enable", std::make_unique<base::DictionaryValue>());

  // Make sure there are no existing DevTools events in the queue.
  EXPECT_EQ(notifications_.size(), 0ul);

  // 3) Trigger the conversion redirect.
  EXPECT_TRUE(ExecJs(web_contents(), "registerConversion({data: 123})"));

  // 4) Verify the request is marked as successful and not as failed.
  WaitForNotification("Network.loadingFinished", true);
}

}  // namespace content
