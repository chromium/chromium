// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_gateway_manager.h"

#import "base/test/scoped_feature_list.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_link_opening_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_page_state_change_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_session_handler.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/gemini_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/public/provider/chrome/browser/bwg/bwg_gateway_protocol.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

class GeminiGatewayManagerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        OptimizationGuideServiceFactory::GetInstance(),
        OptimizationGuideServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        feature_engagement::TrackerFactory::GetInstance(),
        base::BindOnce(&GeminiGatewayManagerTest::CreateMockTracker));
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    mock_settings_handler_ = OCMProtocolMock(@protocol(SettingsCommands));
    [dispatcher startDispatchingToTarget:mock_settings_handler_
                             forProtocol:@protocol(SettingsCommands)];
    mock_gemini_handler_ = OCMProtocolMock(@protocol(GeminiCommands));
    [dispatcher startDispatchingToTarget:mock_gemini_handler_
                             forProtocol:@protocol(GeminiCommands)];
  }

  static std::unique_ptr<KeyedService> CreateMockTracker(ProfileIOS* context) {
    return std::make_unique<feature_engagement::test::MockTracker>();
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  id mock_settings_handler_;
  id mock_gemini_handler_;
};

// Tests that all core handlers and gateway are properly initialized by the
// manager.
TEST_F(GeminiGatewayManagerTest, TestHandlersInitialization) {
  GeminiGatewayManager* manager =
      [[GeminiGatewayManager alloc] initWithBrowser:browser_.get()
                                  viewStateDelegate:nil];
  EXPECT_NE(nil, manager.gateway);
  EXPECT_NE(nil, manager.linkOpeningHandler);
  EXPECT_NE(nil, manager.pageStateChangeHandler);
  EXPECT_NE(nil, manager.sessionHandler);
  EXPECT_NE(nil, manager.consentProviderHandler);
  EXPECT_NE(nil, manager.suggestionHandler);

  [manager disconnect];
}
