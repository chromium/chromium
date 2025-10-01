// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/mini_map/coordinator/mini_map_coordinator.h"

#import "base/ios/block_types.h"
#import "base/ios/ios_util.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "components/test/ios/test_utils.h"
#import "ios/chrome/browser/mini_map/coordinator/mini_map_mediator.h"
#import "ios/chrome/browser/mini_map/coordinator/mini_map_mediator_delegate.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/mini_map_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/test/providers/mini_map/test_mini_map.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/common/features.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

typedef void (^BlockWithViewController)(UIViewController*);

// Expose mediator to test coordinator.
@interface MiniMapCoordinator (Testing) <MiniMapMediatorDelegate>
// Override the mediator property
@property(nonatomic, strong) MiniMapMediator* mediator;
@end

// A Mini map factory tat return a mock version of the controller
@interface TestMiniMapControllerFactory : NSObject <MiniMapControllerFactory>

// The controller the factory will return.
@property(nonatomic, weak) id<MiniMapController> controller;
@end

@implementation TestMiniMapControllerFactory

- (id<MiniMapController>)createMiniMapController {
  return _controller;
}

- (BOOL)canHandleURL:(NSURL*)url {
  return YES;
}

@end

// Tests the MiniMapCoordinator logic and its links to the MiniMapController.
class MiniMapCoordinatorTest : public PlatformTest {
 protected:
  MiniMapCoordinatorTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    builder.SetPrefService(CreatePrefService());
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    mock_application_command_handler_ =
        OCMStrictProtocolMock(@protocol(ApplicationCommands));
    mock_application_settings_command_handler_ =
        OCMStrictProtocolMock(@protocol(SettingsCommands));
    mock_mini_map_command_handler_ =
        OCMStrictProtocolMock(@protocol(MiniMapCommands));
    mock_snackbar_command_handler_ =
        OCMStrictProtocolMock(@protocol(SnackbarCommands));

    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    [dispatcher startDispatchingToTarget:mock_application_command_handler_
                             forProtocol:@protocol(ApplicationCommands)];
    [dispatcher
        startDispatchingToTarget:mock_application_settings_command_handler_
                     forProtocol:@protocol(SettingsCommands)];
    [dispatcher startDispatchingToTarget:mock_mini_map_command_handler_
                             forProtocol:@protocol(MiniMapCommands)];
    [dispatcher startDispatchingToTarget:mock_snackbar_command_handler_
                             forProtocol:@protocol(SnackbarCommands)];

    root_view_controller_ = [[UIViewController alloc] init];
    scoped_window_.Get().rootViewController = root_view_controller_;

    factory_ = [[TestMiniMapControllerFactory alloc] init];
    ios::provider::test::SetMiniMapControllerFactory(factory_);
  }

  void TearDown() override {
    [coordinator_ stop];
    EXPECT_OCMOCK_VERIFY(mock_application_command_handler_);
    EXPECT_OCMOCK_VERIFY(mock_application_settings_command_handler_);
    EXPECT_OCMOCK_VERIFY(mock_mini_map_command_handler_);
    EXPECT_OCMOCK_VERIFY(mock_snackbar_command_handler_);
    ios::provider::test::SetMiniMapControllerFactory(nil);
    PlatformTest::TearDown();
  }

  void SetupCoordinator(BOOL iph,
                        MiniMapMode type,
                        MiniMapQueryType query_type) {
    NSString* text = query_type == MiniMapQueryType::kText ? @"Address" : nil;
    NSURL* url = query_type == MiniMapQueryType::kURL
                     ? [NSURL URLWithString:@"https://www.test.test"]
                     : nil;
    coordinator_ = [[MiniMapCoordinator alloc]
        initWithBaseViewController:root_view_controller_
                           browser:browser_.get()
                              text:text
                               url:url
                           withIPH:iph
                              mode:type];
    [coordinator_ start];
  }

  std::unique_ptr<sync_preferences::PrefServiceSyncable> CreatePrefService() {
    auto prefs =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    user_prefs::PrefRegistrySyncable* registry = prefs->registry();
    RegisterProfilePrefs(registry);
    return prefs;
  }

 protected:
  web::WebTaskEnvironment environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  id<MiniMapMediatorDelegate> delegate_;
  std::unique_ptr<Browser> browser_;
  MiniMapCoordinator* coordinator_;
  TestMiniMapControllerFactory* factory_;
  id mock_application_command_handler_;
  id mock_application_settings_command_handler_;
  id mock_mini_map_command_handler_;
  id mock_snackbar_command_handler_;
  ScopedKeyWindow scoped_window_;
  UIViewController* root_view_controller_ = nil;
};

// Tests that consent screen is not triggered, but IPH is configured.
TEST_F(MiniMapCoordinatorTest, TestIPH) {
  profile_->GetPrefs()->SetBoolean(prefs::kDetectAddressesAccepted, false);
  profile_->GetPrefs()->SetBoolean(prefs::kDetectAddressesEnabled, true);
  id mini_map_controller = OCMStrictProtocolMock(@protocol(MiniMapController));
  factory_.controller = mini_map_controller;

  OCMExpect([mini_map_controller configureAddress:[OCMArg any]]);
  OCMExpect([mini_map_controller configureCompletion:[OCMArg any]]);
  OCMExpect(
      [mini_map_controller configureCompletionWithSearchQuery:[OCMArg any]]);
  OCMExpect([mini_map_controller configureFooterWithTitle:[OCMArg any]
                                       leadingButtonTitle:[OCMArg any]
                                      trailingButtonTitle:[OCMArg any]
                                      leadingButtonAction:[OCMArg any]
                                     trailingButtonAction:[OCMArg any]]);

  OCMExpect([mini_map_controller configureDisclaimerWithTitle:[OCMArg any]
                                                     subtitle:[OCMArg any]
                                                actionHandler:[OCMArg any]]);

  OCMExpect([mini_map_controller
      presentMapsWithPresentingViewController:[OCMArg any]]);

  SetupCoordinator(YES, MiniMapMode::kMap, MiniMapQueryType::kText);
  environment_.RunUntilIdle();
  EXPECT_TRUE(
      profile_->GetPrefs()->GetBoolean(prefs::kDetectAddressesAccepted));
  EXPECT_OCMOCK_VERIFY(mini_map_controller);
}

// Tests IPH is not displayed on second trigger
TEST_F(MiniMapCoordinatorTest, TestIPHSecondLaunch) {
  profile_->GetPrefs()->SetBoolean(prefs::kDetectAddressesAccepted, true);
  profile_->GetPrefs()->SetBoolean(prefs::kDetectAddressesEnabled, true);
  id mini_map_controller = OCMStrictProtocolMock(@protocol(MiniMapController));
  factory_.controller = mini_map_controller;
  OCMExpect([mini_map_controller configureAddress:[OCMArg any]]);
  OCMExpect([mini_map_controller configureCompletion:[OCMArg any]]);
  OCMExpect(
      [mini_map_controller configureCompletionWithSearchQuery:[OCMArg any]]);
  OCMExpect([mini_map_controller configureFooterWithTitle:[OCMArg any]
                                       leadingButtonTitle:[OCMArg any]
                                      trailingButtonTitle:[OCMArg any]
                                      leadingButtonAction:[OCMArg any]
                                     trailingButtonAction:[OCMArg any]]);

  OCMExpect([mini_map_controller
      presentMapsWithPresentingViewController:[OCMArg any]]);
  SetupCoordinator(YES, MiniMapMode::kMap, MiniMapQueryType::kText);
  EXPECT_OCMOCK_VERIFY(mini_map_controller);
}

// Tests that correct metrics are logged on dismiss.
TEST_F(MiniMapCoordinatorTest, TestDismissMap) {
  base::HistogramTester histogram_tester;
  profile_->GetPrefs()->SetBoolean(prefs::kDetectAddressesAccepted, false);
  profile_->GetPrefs()->SetBoolean(prefs::kDetectAddressesEnabled, true);
  id mini_map_controller = OCMStrictProtocolMock(@protocol(MiniMapController));
  factory_.controller = mini_map_controller;

  __block MiniMapControllerCompletionWithURL completion_block;

  OCMExpect([mini_map_controller configureAddress:[OCMArg any]]);
  OCMExpect([mini_map_controller
      configureCompletion:AssignValueToVariable(completion_block)]);
  OCMExpect(
      [mini_map_controller configureCompletionWithSearchQuery:[OCMArg any]]);
  OCMExpect([mini_map_controller configureFooterWithTitle:[OCMArg any]
                                       leadingButtonTitle:[OCMArg any]
                                      trailingButtonTitle:[OCMArg any]
                                      leadingButtonAction:[OCMArg any]
                                     trailingButtonAction:[OCMArg any]]);

  OCMExpect([mini_map_controller
      presentMapsWithPresentingViewController:[OCMArg any]]);
  SetupCoordinator(NO, MiniMapMode::kMap, MiniMapQueryType::kText);

  OCMExpect([mock_mini_map_command_handler_ hideMiniMap]);
  ASSERT_NE(nil, completion_block);
  completion_block(nil);
  // Expect normal outcome.
  histogram_tester.ExpectTotalCount("IOS.MiniMap.Link.Outcome", 0);
  histogram_tester.ExpectBucketCount("IOS.MiniMap.Outcome", 0, 1);
  EXPECT_OCMOCK_VERIFY(mini_map_controller);
}

// Tests that correct metrics are logged on dismiss.
TEST_F(MiniMapCoordinatorTest, TestLinkDismissMap) {
  base::HistogramTester histogram_tester;
  profile_->GetPrefs()->SetBoolean(prefs::kDetectAddressesAccepted, false);
  profile_->GetPrefs()->SetBoolean(prefs::kDetectAddressesEnabled, true);
  id mini_map_controller = OCMStrictProtocolMock(@protocol(MiniMapController));
  factory_.controller = mini_map_controller;

  __block MiniMapControllerCompletionWithURL completion_block;

  OCMExpect([mini_map_controller configureURL:[OCMArg any]]);
  OCMExpect([mini_map_controller
      configureCompletion:AssignValueToVariable(completion_block)]);
  OCMExpect(
      [mini_map_controller configureCompletionWithSearchQuery:[OCMArg any]]);
  OCMExpect([mini_map_controller configureFooterWithTitle:[OCMArg any]
                                       leadingButtonTitle:[OCMArg any]
                                      trailingButtonTitle:[OCMArg any]
                                      leadingButtonAction:[OCMArg any]
                                     trailingButtonAction:[OCMArg any]]);

  OCMExpect([mini_map_controller
      presentMapsWithPresentingViewController:[OCMArg any]]);
  SetupCoordinator(NO, MiniMapMode::kMap, MiniMapQueryType::kURL);

  OCMExpect([mock_mini_map_command_handler_ hideMiniMap]);
  ASSERT_NE(nil, completion_block);
  completion_block(nil);
  // Expect normal outcome.
  histogram_tester.ExpectTotalCount("IOS.MiniMap.Outcome", 0);
  histogram_tester.ExpectBucketCount("IOS.MiniMap.Link.Outcome", 0, 1);
  EXPECT_OCMOCK_VERIFY(mini_map_controller);
}

// Tests that URL is opened if requested on dismiss.
TEST_F(MiniMapCoordinatorTest, TestOpenURL) {
  base::HistogramTester histogram_tester;
  profile_->GetPrefs()->SetBoolean(prefs::kDetectAddressesAccepted, false);
  profile_->GetPrefs()->SetBoolean(prefs::kDetectAddressesEnabled, true);
  id mini_map_controller = OCMStrictProtocolMock(@protocol(MiniMapController));
  factory_.controller = mini_map_controller;

  __block MiniMapControllerCompletionWithURL completion_block;

  OCMExpect([mini_map_controller configureAddress:[OCMArg any]]);
  OCMExpect([mini_map_controller
      configureCompletion:AssignValueToVariable(completion_block)]);
  OCMExpect(
      [mini_map_controller configureCompletionWithSearchQuery:[OCMArg any]]);
  OCMExpect([mini_map_controller configureFooterWithTitle:[OCMArg any]
                                       leadingButtonTitle:[OCMArg any]
                                      trailingButtonTitle:[OCMArg any]
                                      leadingButtonAction:[OCMArg any]
                                     trailingButtonAction:[OCMArg any]]);

  OCMExpect([mini_map_controller
      presentMapsWithPresentingViewController:[OCMArg any]]);
  SetupCoordinator(NO, MiniMapMode::kMap, MiniMapQueryType::kText);
  OCMExpect([mock_mini_map_command_handler_ hideMiniMap]);
  OCMExpect([mock_application_command_handler_ openURLInNewTab:[OCMArg any]]);

  ASSERT_NE(nil, completion_block);
  completion_block([NSURL URLWithString:@"https://www.example.org"]);
  // Expect url outcome.
  histogram_tester.ExpectBucketCount("IOS.MiniMap.Outcome", 1, 1);
  EXPECT_OCMOCK_VERIFY(mini_map_controller);
}

// Tests that the query is opened if requested on dismiss.
TEST_F(MiniMapCoordinatorTest, TestOpenQuery) {
  base::HistogramTester histogram_tester;
  profile_->GetPrefs()->SetBoolean(prefs::kDetectAddressesAccepted, false);
  profile_->GetPrefs()->SetBoolean(prefs::kDetectAddressesEnabled, true);
  id mini_map_controller = OCMStrictProtocolMock(@protocol(MiniMapController));
  factory_.controller = mini_map_controller;

  __block MiniMapControllerCompletionWithString completion_block;

  OCMExpect([mini_map_controller configureAddress:[OCMArg any]]);
  OCMExpect([mini_map_controller configureCompletion:[OCMArg any]]);
  OCMExpect([mini_map_controller
      configureCompletionWithSearchQuery:AssignValueToVariable(
                                             completion_block)]);
  OCMExpect([mini_map_controller configureFooterWithTitle:[OCMArg any]
                                       leadingButtonTitle:[OCMArg any]
                                      trailingButtonTitle:[OCMArg any]
                                      leadingButtonAction:[OCMArg any]
                                     trailingButtonAction:[OCMArg any]]);

  OCMExpect([mini_map_controller
      presentMapsWithPresentingViewController:[OCMArg any]]);
  SetupCoordinator(NO, MiniMapMode::kMap, MiniMapQueryType::kText);
  OCMExpect([mock_mini_map_command_handler_ hideMiniMap]);
  OCMExpect([mock_application_command_handler_ openURLInNewTab:[OCMArg any]]);

  ASSERT_NE(nil, completion_block);
  completion_block(@"Query test");
  // Expect url outcome.
  histogram_tester.ExpectBucketCount("IOS.MiniMap.Outcome", 4 /*kOpenedQuery*/,
                                     1);
  EXPECT_OCMOCK_VERIFY(mini_map_controller);
}

// Tests the footer buttons.
TEST_F(MiniMapCoordinatorTest, TestFooterButtons) {
  base::HistogramTester histogram_tester;
  profile_->GetPrefs()->SetBoolean(prefs::kDetectAddressesAccepted, false);
  profile_->GetPrefs()->SetBoolean(prefs::kDetectAddressesEnabled, true);
  id mini_map_controller = OCMStrictProtocolMock(@protocol(MiniMapController));
  factory_.controller = mini_map_controller;

  __block BlockWithViewController left_button_block;
  __block BlockWithViewController right_button_block;
  __block MiniMapControllerCompletionWithURL completion_block;

  OCMExpect([mini_map_controller configureAddress:[OCMArg any]]);
  OCMExpect([mini_map_controller
      configureCompletion:AssignValueToVariable(completion_block)]);
  OCMExpect(
      [mini_map_controller configureCompletionWithSearchQuery:[OCMArg any]]);
  OCMExpect([mini_map_controller
      configureFooterWithTitle:[OCMArg any]
            leadingButtonTitle:[OCMArg any]
           trailingButtonTitle:[OCMArg any]
           leadingButtonAction:AssignValueToVariable(left_button_block)
          trailingButtonAction:AssignValueToVariable(right_button_block)]);

  OCMExpect([mini_map_controller
      presentMapsWithPresentingViewController:[OCMArg any]]);
  SetupCoordinator(NO, MiniMapMode::kMap, MiniMapQueryType::kText);

  OCMExpect([mock_snackbar_command_handler_
      showSnackbarWithMessage:[OCMArg any]
                   buttonText:[OCMArg any]
                messageAction:[OCMArg any]
             completionAction:[OCMArg any]]);

  histogram_tester.ExpectBucketCount("IOS.MiniMap.Outcome", 5, 0);
  left_button_block(nil);
  histogram_tester.ExpectBucketCount("IOS.MiniMap.Outcome", 5, 1);
  EXPECT_FALSE(
      profile_->GetPrefs()->GetBoolean(prefs::kDetectAddressesEnabled));

  OCMExpect([mock_application_command_handler_
      showReportAnIssueFromViewController:[OCMArg any]
                                   sender:UserFeedbackSender::MiniMap]);
  histogram_tester.ExpectBucketCount("IOS.MiniMap.Outcome", 2, 0);
  right_button_block(nil);
  histogram_tester.ExpectBucketCount("IOS.MiniMap.Outcome", 2, 1);

  OCMExpect([mock_mini_map_command_handler_ hideMiniMap]);

  ASSERT_NE(nil, completion_block);
  completion_block(nil);
  // Expect normal outcome.
  histogram_tester.ExpectBucketCount("IOS.MiniMap.Outcome", 0, 1);
  EXPECT_OCMOCK_VERIFY(mini_map_controller);
}
