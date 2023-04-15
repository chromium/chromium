// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/registration_type.mojom.h"
#include "components/attribution_reporting/test_utils.h"
#include "content/browser/attribution_reporting/attribution_manager_impl.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/test/mock_attribution_host.h"
#include "content/browser/attribution_reporting/test/mock_data_host.h"
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
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

using ::attribution_reporting::mojom::RegistrationType;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::Pointee;

}  // namespace

class AttributionTriggerRegistrationBrowserTest : public ContentBrowserTest {
 public:
  AttributionTriggerRegistrationBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Sets up the blink runtime feature for ConversionMeasurement.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "content/test/data/attribution_reporting");
    ASSERT_TRUE(embedded_test_server()->Start());

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    net::test_server::RegisterDefaultHandlers(https_server_.get());
    https_server_->ServeFilesFromSourceDirectory(
        "content/test/data/attribution_reporting");
    ASSERT_TRUE(https_server_->Start());

    MockAttributionHost::Override(web_contents());
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

IN_PROC_BROWSER_TEST_F(
    AttributionTriggerRegistrationBrowserTest,
    NonAttributionSrcImgRedirect_MultipleTriggersRegistered) {
  EXPECT_TRUE(NavigateToURL(
      shell(),
      https_server()->GetURL("c.test", "/page_with_conversion_redirect.html")));

  std::vector<std::unique_ptr<MockDataHost>> data_hosts;
  base::RunLoop loop;
  EXPECT_CALL(mock_attribution_host(), RegisterDataHost)
      .WillRepeatedly(
          [&](mojo::PendingReceiver<blink::mojom::AttributionDataHost> host,
              RegistrationType) {
            data_hosts.push_back(GetRegisteredDataHost(std::move(host)));
            if (data_hosts.size() == 2) {
              loop.Quit();
            }
          });

  GURL register_url = https_server()->GetURL(
      "c.test", "/register_trigger_headers_and_redirect.html");

  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("createTrackingPixel($1);", register_url)));

  if (data_hosts.size() != 2) {
    loop.Run();
  }

  data_hosts.front()->WaitForTriggerData(/*num_trigger_data=*/1);
  const auto& trigger_data1 = data_hosts.front()->trigger_data();

  EXPECT_EQ(trigger_data1.size(), 1u);
  EXPECT_THAT(trigger_data1.front().event_triggers,
              ElementsAre(EventTriggerDataMatches(
                  EventTriggerDataMatcherConfig(/*data=*/5))));

  data_hosts.back()->WaitForTriggerData(/*num_trigger_data=*/1);
  const auto& trigger_data2 = data_hosts.back()->trigger_data();

  EXPECT_EQ(trigger_data2.size(), 1u);
  EXPECT_THAT(trigger_data2.front().event_triggers,
              ElementsAre(EventTriggerDataMatches(
                  EventTriggerDataMatcherConfig(/*data=*/7))));
}

}  // namespace content
