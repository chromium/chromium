// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/registration_eligibility.mojom.h"
#include "components/attribution_reporting/test_utils.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "content/browser/attribution_reporting/attribution_data_host_manager_impl.h"
#include "content/browser/attribution_reporting/attribution_manager_impl.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/test/mock_attribution_manager.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/resource_load_observer.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace content {

namespace {

using ::attribution_reporting::EventTriggerData;
using ::attribution_reporting::TriggerRegistration;
using ::attribution_reporting::mojom::RegistrationEligibility;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::StrictMock;

}  // namespace

class AttributionTriggerRegistrationBrowserTest
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  AttributionTriggerRegistrationBrowserTest() {
    const bool enable_in_browser_migration = GetParam();
    if (enable_in_browser_migration) {
      scoped_feature_list_.InitWithFeatures(
          {blink::features::kKeepAliveInBrowserMigration,
           blink::features::kAttributionReportingInBrowserMigration},
          {});
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          {blink::features::kKeepAliveInBrowserMigration});
    }
  }

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

    auto mock_manager = std::make_unique<StrictMock<MockAttributionManager>>();
    auto data_host_manager =
        std::make_unique<AttributionDataHostManagerImpl>(mock_manager.get());
    mock_manager->SetDataHostManager(std::move(data_host_manager));
    static_cast<StoragePartitionImpl*>(
        web_contents()->GetBrowserContext()->GetDefaultStoragePartition())
        ->OverrideAttributionManagerForTesting(std::move(mock_manager));
  }

  WebContents* web_contents() { return shell()->web_contents(); }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  StrictMock<MockAttributionManager>& mock_attribution_manager() {
    return *static_cast<StrictMock<MockAttributionManager>*>(
        AttributionManager::FromWebContents(web_contents()));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  AttributionManagerImpl::ScopedUseInMemoryStorageForTesting
      attribution_manager_in_memory_setting_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         AttributionTriggerRegistrationBrowserTest,
                         ::testing::Bool());

IN_PROC_BROWSER_TEST_P(
    AttributionTriggerRegistrationBrowserTest,
    NonAttributionSrcImgRedirect_MultipleTriggersRegistered) {
  EXPECT_TRUE(NavigateToURL(
      shell(),
      https_server()->GetURL("c.test", "/page_with_conversion_redirect.html")));

  base::RunLoop run_loop;
  const auto on_trigger = base::BarrierClosure(2, run_loop.QuitClosure());
  EXPECT_CALL(
      mock_attribution_manager(),
      HandleTrigger(
          Property(&AttributionTrigger::registration,
                   Field(&TriggerRegistration::event_triggers,
                         ElementsAre(Field(&EventTriggerData::data, 5u)))),
          _))
      .Times(1)
      .WillOnce([&on_trigger]() { on_trigger.Run(); });
  EXPECT_CALL(
      mock_attribution_manager(),
      HandleTrigger(
          Property(&AttributionTrigger::registration,
                   Field(&TriggerRegistration::event_triggers,
                         ElementsAre(Field(&EventTriggerData::data, 7u)))),
          _))
      .Times(1)
      .WillOnce([&on_trigger]() { on_trigger.Run(); });

  GURL register_url = https_server()->GetURL(
      "c.test", "/register_trigger_headers_and_redirect.html");

  EXPECT_TRUE(ExecJs(web_contents(),
                     JsReplace("createTrackingPixel($1);", register_url)));

  run_loop.Run();
}

}  // namespace content
