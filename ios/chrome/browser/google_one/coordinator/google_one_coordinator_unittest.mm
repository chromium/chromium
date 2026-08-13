// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/google_one/coordinator/google_one_coordinator.h"

#import "base/test/metrics/histogram_tester.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/google_one_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/public/provider/chrome/browser/google_one/google_one_api.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

@interface CoordinatorTestFakeGoogleOneController
    : NSObject <GoogleOneController>
@property(nonatomic, assign) BOOL launched;
@property(nonatomic, assign) BOOL stopped;
@end

@implementation CoordinatorTestFakeGoogleOneController
- (void)launchWithViewController:(UIViewController*)baseViewController
                      completion:(void (^)(NSError*))completion {
  self.launched = YES;
}

- (void)launchWithViewController:(UIViewController*)baseViewController
                             URL:(NSURL*)URL
                      completion:(void (^)(NSError*))completion {
  self.launched = YES;
}

- (void)stop {
  self.stopped = YES;
}
@end

@interface CoordinatorTestFakeGoogleOneControllerFactory
    : NSObject <GoogleOneControllerFactory>
@property(nonatomic, strong)
    CoordinatorTestFakeGoogleOneController* lastCreatedController;
@end

@implementation CoordinatorTestFakeGoogleOneControllerFactory
- (id<GoogleOneController>)createControllerWithConfiguration:
    (GoogleOneConfiguration*)configuration {
  self.lastCreatedController =
      [[CoordinatorTestFakeGoogleOneController alloc] init];
  return self.lastCreatedController;
}
@end

namespace {

// Value of
// GoogleOneOutcomeMetrics::kInvalidParametersFallbackToOpeningURLInNewTab.
constexpr int kInvalidParametersFallbackToOpeningURLInNewTab = 11;

const char kSettingsOutcomeHistogram[] = "IOS.GoogleOne.Outcome.Settings";

class GoogleOneCoordinatorTest : public PlatformTest {
 protected:
  GoogleOneCoordinatorTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegateForTesting(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    profile_ = std::move(builder).Build();

    app_state_ = [[AppState alloc] initWithStartupInformation:nil];
    profile_state_ = [[ProfileState alloc] initWithAppState:app_state_];
    scene_state_ = [[FakeSceneState alloc] initWithProfile:profile_.get()];
    scene_state_.profileState = profile_state_;

    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);
    base_view_controller_ = [[UIViewController alloc] init];

    mock_scene_commands_ = OCMStrictProtocolMock(@protocol(SceneCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_scene_commands_
                     forProtocol:@protocol(SceneCommands)];

    mock_google_one_commands_ =
        OCMStrictProtocolMock(@protocol(GoogleOneCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_google_one_commands_
                     forProtocol:@protocol(GoogleOneCommands)];
  }

  void TearDown() override {
    [scene_state_ shutdown];
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  AppState* app_state_;
  ProfileState* profile_state_;
  FakeSceneState* scene_state_;
  std::unique_ptr<TestBrowser> browser_;
  UIViewController* base_view_controller_;
  id mock_scene_commands_;
  id mock_google_one_commands_;
  base::HistogramTester histogram_tester_;
};

// Tests that when starting GoogleOneCoordinator signed-out with a valid input
// URL, it opens the input URL in a new tab, logs the correct histogram metric,
// and calls hideGoogleOne.
TEST_F(GoogleOneCoordinatorTest, StartSignedOutWithInputURL) {
  GURL input_url("https://one.google.com/deeplink");
  GoogleOneCoordinator* coordinator = [[GoogleOneCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()
                      entryPoint:GoogleOneEntryPoint::kSettings
                        inputURL:input_url];

  OCMExpect([mock_scene_commands_ openURLInNewTab:[OCMArg any]]);
  OCMExpect([mock_google_one_commands_ hideGoogleOne]);

  [coordinator start];

  EXPECT_OCMOCK_VERIFY(mock_scene_commands_);
  EXPECT_OCMOCK_VERIFY(mock_google_one_commands_);

  histogram_tester_.ExpectUniqueSample(
      kSettingsOutcomeHistogram, kInvalidParametersFallbackToOpeningURLInNewTab,
      1);

  [coordinator stop];
}

// Tests that when starting GoogleOneCoordinator signed-in with an identity,
// it creates and launches the GoogleOneController and stops it on coordinator
// stop.
TEST_F(GoogleOneCoordinatorTest, StartSignedInWithIdentity) {
  CoordinatorTestFakeGoogleOneControllerFactory* factory =
      [[CoordinatorTestFakeGoogleOneControllerFactory alloc] init];
  ios::provider::SetGoogleOneControllerFactory(factory);

  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentityManager* fake_system_identity_manager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  fake_system_identity_manager->AddIdentity(identity);

  GoogleOneCoordinator* coordinator = [[GoogleOneCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()
                      entryPoint:GoogleOneEntryPoint::kSettings
                        identity:identity];

  [coordinator start];
  EXPECT_TRUE(factory.lastCreatedController.launched);

  [coordinator stop];
  EXPECT_TRUE(factory.lastCreatedController.stopped);

  ios::provider::SetGoogleOneControllerFactory(nil);
}

}  // namespace
