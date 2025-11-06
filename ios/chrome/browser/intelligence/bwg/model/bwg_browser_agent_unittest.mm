// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/bwg_browser_agent.h"

#import "base/test/task_environment.h"
#import "components/favicon/core/favicon_service.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "ios/chrome/browser/favicon/model/favicon_service_factory.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_tab_helper.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Test fixture for BwgBrowserAgent.
class BwgBrowserAgentTest : public PlatformTest {
 protected:
  BwgBrowserAgentTest() {
    TestProfileIOS::Builder profile_builder;
    profile_builder.AddTestingFactory(
        OptimizationGuideServiceFactory::GetInstance(),
        OptimizationGuideServiceFactory::GetDefaultFactory());
    profile_ = std::move(profile_builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    BwgBrowserAgent::CreateForBrowser(browser_.get());
    bwg_browser_agent_ = BwgBrowserAgent::FromBrowser(browser_.get());

    optimization_guide_service_ =
        OptimizationGuideServiceFactory::GetForProfile(profile_.get());

    mock_settings_handler_ = OCMProtocolMock(@protocol(SettingsCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_settings_handler_
                     forProtocol:@protocol(SettingsCommands)];
    mock_bwg_handler_ = OCMProtocolMock(@protocol(BWGCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_bwg_handler_
                     forProtocol:@protocol(BWGCommands)];

    std::unique_ptr<web::FakeWebState> web_state =
        std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(profile_.get());
    BwgTabHelper::CreateForWebState(web_state.get());
    bwg_tab_helper_ = BwgTabHelper::FromWebState(web_state.get());

    favicon::WebFaviconDriver::CreateForWebState(
        web_state.get(),
        ios::FaviconServiceFactory::GetForProfile(
            profile_.get(), ServiceAccessType::IMPLICIT_ACCESS));

    browser_->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate(true));
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<BwgBrowserAgent> bwg_browser_agent_;
  raw_ptr<BwgTabHelper> bwg_tab_helper_;
  raw_ptr<OptimizationGuideService> optimization_guide_service_;
  id mock_settings_handler_;
  id mock_bwg_handler_;
};

// Tests that the BwgBrowserAgent can be instantiated.
TEST_F(BwgBrowserAgentTest, TestBwgBrowserAgentInstantiation) {
  EXPECT_NE(nullptr, bwg_browser_agent_);
}

// Tests the presentation of the BWG overlay and state of tab helper side
// effects.
TEST_F(BwgBrowserAgentTest, TestBwgBrowserAgentPresentBwgOverlay) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  std::unique_ptr<optimization_guide::proto::PageContext> page_context =
      std::make_unique<optimization_guide::proto::PageContext>();
  PageContextWrapperCallbackResponse response =
      base::ok(std::move(page_context));

  // Set the BWG tab helper as backgrounded and assert.
  bwg_tab_helper_->PrepareBwgFreBackgrounding();
  ASSERT_TRUE(bwg_tab_helper_->GetIsBwgSessionActiveInBackground());

  bwg_browser_agent_->PresentBwgOverlay(base_view_controller,
                                        std::move(response));

  // Assert the BWG tab helper was set as foregrounded.
  ASSERT_FALSE(bwg_tab_helper_->GetIsBwgSessionActiveInBackground());
}

TEST_F(BwgBrowserAgentTest, TestBwgBrowserAgentPresentPendingBwgOverlay) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  std::unique_ptr<optimization_guide::proto::PageContext> page_context =
      std::make_unique<optimization_guide::proto::PageContext>();

  // Set the BWG tab helper as backgrounded and assert.
  bwg_tab_helper_->PrepareBwgFreBackgrounding();
  ASSERT_TRUE(bwg_tab_helper_->GetIsBwgSessionActiveInBackground());

  bwg_browser_agent_->PresentPendingBwgOverlay(base_view_controller,
                                               std::move(page_context));

  // Assert the BWG tab helper was set as foregrounded.
  ASSERT_FALSE(bwg_tab_helper_->GetIsBwgSessionActiveInBackground());
}
