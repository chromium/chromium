// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/mini_map/ui_bundled/mini_map_coordinator.h"

#import "base/ios/block_types.h"
#import "base/ios/ios_util.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/mini_map/ui_bundled/mini_map_mediator.h"
#import "ios/chrome/browser/mini_map/ui_bundled/mini_map_mediator_delegate.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/mini_map_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/test/providers/mini_map/test_mini_map.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/common/features.h"
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

// Records the last address that has been passed to the factory
@property(nonatomic, copy) NSString* lastAddress;

// Records the last completion that has been passed to the factory
@property(nonatomic, copy) MiniMapControllerCompletion lastCompletion;

// The controller the factory will return.
@property(nonatomic, weak) id<MiniMapController> controller;
@end

@implementation TestMiniMapControllerFactory

- (id<MiniMapController>)
    createMiniMapControllerForString:(NSString*)address
                          completion:(MiniMapControllerCompletion)completion {
  _lastAddress = address;
  _lastCompletion = completion;
  return _controller;
}

@end

// Tests the MiniMapCoordinator logic and its links to the MiniMapController.
class MiniMapCoordinatorTest : public PlatformTest {
 protected:
  MiniMapCoordinatorTest() {
    TestProfileIOS::Builder builder;
    builder.SetPrefService(CreatePrefService());
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    mock_application_command_handler_ =
        OCMStrictProtocolMock(@protocol(ApplicationCommands));
    mock_application_settings_command_handler_ =
        OCMStrictProtocolMock(@protocol(SettingsCommands));
    mock_mini_map_command_handler_ =
        OCMStrictProtocolMock(@protocol(MiniMapCommands));

    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    [dispatcher startDispatchingToTarget:mock_application_command_handler_
                             forProtocol:@protocol(ApplicationCommands)];
    [dispatcher
        startDispatchingToTarget:mock_application_settings_command_handler_
                     forProtocol:@protocol(SettingsCommands)];
    [dispatcher startDispatchingToTarget:mock_mini_map_command_handler_
                             forProtocol:@protocol(MiniMapCommands)];

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
    ios::provider::test::SetMiniMapControllerFactory(nil);
    PlatformTest::TearDown();
  }

  void SetupCoordinator(BOOL consent_required, MiniMapMode type) {
    coordinator_ = [[MiniMapCoordinator alloc]
        initWithBaseViewController:root_view_controller_
                           browser:browser_.get()
                          webState:nullptr
                              text:@"Address"
                   consentRequired:consent_required
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
  base::test::TaskEnvironment environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  id<MiniMapMediatorDelegate> delegate_;
  std::unique_ptr<Browser> browser_;
  MiniMapCoordinator* coordinator_;
  TestMiniMapControllerFactory* factory_;
  id mock_application_command_handler_;
  id mock_application_settings_command_handler_;
  id mock_mini_map_command_handler_;
  ScopedKeyWindow scoped_window_;
  UIViewController* root_view_controller_ = nil;
};

// Tests the map controller is start immediately if no consent is needed.
TEST_F(MiniMapCoordinatorTest, TestNoConsentNeededMap) {
  if (!base::ios::IsRunningOnOrLater(16, 4, 0)) {
    GTEST_SKIP() << "Feature only available on iOS16.4+";
  }
  id mini_map_controller = OCMStrictProtocolMock(@protocol(MiniMapController));
  factory_.controller = mini_map_controller;

  OCMExpect([mini_map_controller configureFooterWithTitle:[OCMArg any]
                                       leadingButtonTitle:[OCMArg any]
                                      trailingButtonTitle:[OCMArg any]
                                      leadingButtonAction:[OCMArg any]
                                     trailingButtonAction:[OCMArg any]]);

  OCMExpect([mini_map_controller
      presentMapsWithPresentingViewController:[OCMArg any]]);
  SetupCoordinator(NO, MiniMapMode::kMap);
  EXPECT_OCMOCK_VERIFY(mini_map_controller);
}

// Tests the directions controller is start immediately if no consent is needed.
TEST_F(MiniMapCoordinatorTest, TestNoConsentNeededDirections) {
  if (!base::ios::IsRunningOnOrLater(16, 4, 0)) {
    GTEST_SKIP() << "Feature only available on iOS16.4+";
  }
  id mini_map_controller = OCMStrictProtocolMock(@protocol(MiniMapController));
  factory_.controller = mini_map_controller;

  OCMExpect([mini_map_controller configureFooterWithTitle:[OCMArg any]
                                       leadingButtonTitle:[OCMArg any]
                                      trailingButtonTitle:[OCMArg any]
                                      leadingButtonAction:[OCMArg any]
                                     trailingButtonAction:[OCMArg any]]);

  OCMExpect([mini_map_controller
      presentDirectionsWithPresentingViewController:[OCMArg any]]);
  SetupCoordinator(NO, MiniMapMode::kDirections);
  EXPECT_OCMOCK_VERIFY(mini_map_controller);
}

// Tests that consent screen is triggered, then the map on consent.
TEST_F(MiniMapCoordinatorTest, TestShowMapAfterConsent) {
  if (!base::ios::IsRunningOnOrLater(16, 4, 0)) {
    GTEST_SKIP() << "Feature only available on iOS16.4+";
  }
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(web::features::kOneTapForMaps);
  profile_->GetPrefs()->SetBoolean(prefs::kDetectAddressesAccepted, false);
  profile_->GetPrefs()->SetBoolean(prefs::kDetectAddressesEnabled, true);
  id mini_map_controller = OCMStrictProtocolMock(@protocol(MiniMapController));
  factory_.controller = mini_map_controller;
  SetupCoordinator(YES, MiniMapMode::kMap);

  OCMExpect([mini_map_controller configureFooterWithTitle:[OCMArg any]
                                       leadingButtonTitle:[OCMArg any]
                                      trailingButtonTitle:[OCMArg any]
                                      leadingButtonAction:[OCMArg any]
                                     trailingButtonAction:[OCMArg any]]);

  __block BOOL called = NO;
  OCMExpect([mini_map_controller
      presentMapsWithPresentingViewController:[OCMArg checkWithBlock:^BOOL(
                                                          UIViewController*
                                                              view_controller) {
        called = YES;
        return YES;
      }]]);
  [coordinator_.mediator userConsented];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        return called;
      }));

  EXPECT_OCMOCK_VERIFY(mini_map_controller);
}

// Tests that consent screen is not triggered after consent was given.
TEST_F(MiniMapCoordinatorTest, TestShowMapAfterConsentGiven) {
  if (!base::ios::IsRunningOnOrLater(16, 4, 0)) {
    GTEST_SKIP() << "Feature only available on iOS16.4+";
  }
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(web::features::kOneTapForMaps);
  profile_->GetPrefs()->SetBoolean(prefs::kDetectAddressesAccepted, true);
  profile_->GetPrefs()->SetBoolean(prefs::kDetectAddressesEnabled, true);
  id mini_map_controller = OCMStrictProtocolMock(@protocol(MiniMapController));
  factory_.controller = mini_map_controller;

  OCMExpect([mini_map_controller configureFooterWithTitle:[OCMArg any]
                                       leadingButtonTitle:[OCMArg any]
                                      trailingButtonTitle:[OCMArg any]
                                      leadingButtonAction:[OCMArg any]
                                     trailingButtonAction:[OCMArg any]]);

  OCMExpect([mini_map_controller
      presentMapsWithPresentingViewController:[OCMArg any]]);
  SetupCoordinator(YES, MiniMapMode::kMap);
  EXPECT_OCMOCK_VERIFY(mini_map_controller);
}

// Tests that consent screen is not triggered, but IPH is configured.
TEST_F(MiniMapCoordinatorTest, TestIPH) {
  if (!base::ios::IsRunningOnOrLater(16, 4, 0)) {
    GTEST_SKIP() << "Feature only available on iOS16.4+";
  }
  base::test::ScopedFeatureList scoped_feature_list;
  base::FieldTrialParams feature_parameters{
      {web::features::kOneTapForMapsConsentModeParamTitle,
       web::features::kOneTapForMapsConsentModeIPHParam}};
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      web::features::kOneTapForMaps, feature_parameters);
  profile_->GetPrefs()->SetBoolean(prefs::kDetectAddressesAccepted, false);
  profile_->GetPrefs()->SetBoolean(prefs::kDetectAddressesEnabled, true);
  id mini_map_controller = OCMStrictProtocolMock(@protocol(MiniMapController));
  factory_.controller = mini_map_controller;

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
  SetupCoordinator(YES, MiniMapMode::kMap);
  environment_.RunUntilIdle();
  EXPECT_TRUE(
      profile_->GetPrefs()->GetBoolean(prefs::kDetectAddressesAccepted));
  EXPECT_OCMOCK_VERIFY(mini_map_controller);
}

// Tests IPH is not displayed on second trigger
TEST_F(MiniMapCoordinatorTest, TestIPHSecondLaunch) {
  if (!base::ios::IsRunningOnOrLater(16, 4, 0)) {
    GTEST_SKIP() << "Feature only available on iOS16.4+";
  }
  base::test::ScopedFeatureList scoped_feature_list;
  base::FieldTrialParams feature_parameters{
      {web::features::kOneTapForMapsConsentModeParamTitle,
       web::features::kOneTapForMapsConsentModeIPHParam}};
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      web::features::kOneTapForMaps, feature_parameters);
  profile_->GetPrefs()->SetBoolean(prefs::kDetectAddressesAccepted, true);
  profile_->GetPrefs()->SetBoolean(prefs::kDetectAddressesEnabled, true);
  id mini_map_controller = OCMStrictProtocolMock(@protocol(MiniMapController));
  factory_.controller = mini_map_controller;

  OCMExpect([mini_map_controller configureFooterWithTitle:[OCMArg any]
                                       leadingButtonTitle:[OCMArg any]
                                      trailingButtonTitle:[OCMArg any]
                                      leadingButtonAction:[OCMArg any]
                                     trailingButtonAction:[OCMArg any]]);

  OCMExpect([mini_map_controller
      presentMapsWithPresentingViewController:[OCMArg any]]);
  SetupCoordinator(YES, MiniMapMode::kMap);
  EXPECT_OCMOCK_VERIFY(mini_map_controller);
}

// Tests that correct metrics are logged on dismiss.
TEST_F(MiniMapCoordinatorTest, TestDismissMap) {
  if (!base::ios::IsRunningOnOrLater(16, 4, 0)) {
    GTEST_SKIP() << "Feature only available on iOS16.4+";
  }
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(web::features::kOneTapForMaps);
  profile_->GetPrefs()->SetBoolean(prefs::kDetectAddressesAccepted, false);
  profile_->GetPrefs()->SetBoolean(prefs::kDetectAddressesEnabled, true);
  id mini_map_controller = OCMStrictProtocolMock(@protocol(MiniMapController));
  factory_.controller = mini_map_controller;

  OCMExpect([mini_map_controller configureFooterWithTitle:[OCMArg any]
                                       leadingButtonTitle:[OCMArg any]
                                      trailingButtonTitle:[OCMArg any]
                                      leadingButtonAction:[OCMArg any]
                                     trailingButtonAction:[OCMArg any]]);

  OCMExpect([mini_map_controller
      presentMapsWithPresentingViewController:[OCMArg any]]);
  SetupCoordinator(NO, MiniMapMode::kMap);

  OCMExpect([mock_mini_map_command_handler_ hideMiniMap]);
  factory_.lastCompletion(nil);
  // Expect normal outcome.
  histogram_tester.ExpectBucketCount("IOS.MiniMap.Outcome", 0, 1);
  EXPECT_OCMOCK_VERIFY(mini_map_controller);
}

// Tests that URL is opened if requested on dismiss.
TEST_F(MiniMapCoordinatorTest, TestOpenURL) {
  if (!base::ios::IsRunningOnOrLater(16, 4, 0)) {
    GTEST_SKIP() << "Feature only available on iOS16.4+";
  }
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(web::features::kOneTapForMaps);
  profile_->GetPrefs()->SetBoolean(prefs::kDetectAddressesAccepted, false);
  profile_->GetPrefs()->SetBoolean(prefs::kDetectAddressesEnabled, true);
  id mini_map_controller = OCMStrictProtocolMock(@protocol(MiniMapController));
  factory_.controller = mini_map_controller;

  OCMExpect([mini_map_controller configureFooterWithTitle:[OCMArg any]
                                       leadingButtonTitle:[OCMArg any]
                                      trailingButtonTitle:[OCMArg any]
                                      leadingButtonAction:[OCMArg any]
                                     trailingButtonAction:[OCMArg any]]);

  OCMExpect([mini_map_controller
      presentMapsWithPresentingViewController:[OCMArg any]]);
  SetupCoordinator(NO, MiniMapMode::kMap);
  OCMExpect([mock_mini_map_command_handler_ hideMiniMap]);
  OCMExpect([mock_application_command_handler_ openURLInNewTab:[OCMArg any]]);

  factory_.lastCompletion([NSURL URLWithString:@"https://www.example.org"]);
  // Expect url outcome.
  histogram_tester.ExpectBucketCount("IOS.MiniMap.Outcome", 1, 1);
  EXPECT_OCMOCK_VERIFY(mini_map_controller);
}

// Tests the footer buttons.
TEST_F(MiniMapCoordinatorTest, TestFooterButtons) {
  if (!base::ios::IsRunningOnOrLater(16, 4, 0)) {
    GTEST_SKIP() << "Feature only available on iOS16.4+";
  }
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(web::features::kOneTapForMaps);
  profile_->GetPrefs()->SetBoolean(prefs::kDetectAddressesAccepted, false);
  profile_->GetPrefs()->SetBoolean(prefs::kDetectAddressesEnabled, true);
  id mini_map_controller = OCMStrictProtocolMock(@protocol(MiniMapController));
  factory_.controller = mini_map_controller;

  __block BlockWithViewController left_button_block;
  __block BlockWithViewController right_button_block;

  OCMExpect([mini_map_controller
      configureFooterWithTitle:[OCMArg any]
            leadingButtonTitle:[OCMArg any]
           trailingButtonTitle:[OCMArg any]
           leadingButtonAction:[OCMArg checkWithBlock:^BOOL(
                                           BlockWithViewController block) {
             left_button_block = block;
             return YES;
           }]
          trailingButtonAction:[OCMArg checkWithBlock:^BOOL(
                                           BlockWithViewController block) {
            right_button_block = block;
            return YES;
          }]]);

  OCMExpect([mini_map_controller
      presentMapsWithPresentingViewController:[OCMArg any]]);
  SetupCoordinator(NO, MiniMapMode::kMap);

  OCMExpect([mock_application_settings_command_handler_
      showContentsSettingsFromViewController:[OCMArg any]]);
  histogram_tester.ExpectBucketCount("IOS.MiniMap.Outcome", 3, 0);
  left_button_block(nil);
  histogram_tester.ExpectBucketCount("IOS.MiniMap.Outcome", 3, 1);

  OCMExpect([mock_application_command_handler_
      showReportAnIssueFromViewController:[OCMArg any]
                                   sender:UserFeedbackSender::MiniMap]);
  histogram_tester.ExpectBucketCount("IOS.MiniMap.Outcome", 2, 0);
  right_button_block(nil);
  histogram_tester.ExpectBucketCount("IOS.MiniMap.Outcome", 2, 1);

  OCMExpect([mock_mini_map_command_handler_ hideMiniMap]);

  factory_.lastCompletion(nil);
  // Expect normal outcome.
  histogram_tester.ExpectBucketCount("IOS.MiniMap.Outcome", 0, 1);
  EXPECT_OCMOCK_VERIFY(mini_map_controller);
}
