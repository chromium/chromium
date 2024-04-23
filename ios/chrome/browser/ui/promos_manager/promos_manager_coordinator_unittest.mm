// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/promos_manager/promos_manager_coordinator.h"

#import <Foundation/Foundation.h>

#import "base/apple/foundation_util.h"
#import "base/ios/ios_util.h"
#import "base/test/scoped_feature_list.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/app/application_delegate/app_state+Testing.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/fake_startup_information.h"
#import "ios/chrome/browser/promos_manager/model/features.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/commands/credential_provider_promo_commands.h"
#import "ios/chrome/browser/shared/public/commands/docking_promo_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/promos_manager/bannered_promo_view_provider.h"
#import "ios/chrome/browser/ui/promos_manager/promos_manager_coordinator+Testing.h"
#import "ios/chrome/browser/ui/promos_manager/standard_promo_action_handler.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

class PromosManagerCoordinatorTest : public PlatformTest {
 public:
  void SetUp() override {
    auto testing_prefs =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterBrowserStatePrefs(testing_prefs->registry());

    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              SyncServiceFactory::GetDefaultFactory());
    builder.SetPrefService(std::move(testing_prefs));
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    browser_state_ = builder.Build();

    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());

    view_controller_ = [[UIViewController alloc] init];
    [scoped_key_window_.Get() setRootViewController:view_controller_];
    TestingApplicationContext::GetGlobal()->SetLastShutdownClean(true);

    startup_information_ = [[FakeStartupInformation alloc] init];
    [startup_information_ setIsColdStart:YES];

    AppState* app_state = OCMClassMock([AppState class]);
    OCMStub([(AppState*)app_state initStage]).andReturn(InitStageFinal);
    scene_state_ =
        [[FakeSceneState alloc] initWithAppState:app_state
                                    browserState:browser_state_.get()];
    scene_state_.scene = static_cast<UIWindowScene*>(
        [[[UIApplication sharedApplication] connectedScenes] anyObject]);
    browser_ =
        std::make_unique<TestBrowser>(browser_state_.get(), scene_state_);
  }

  // Initializes a new `PromosManagerCoordinator` for testing.
  void CreatePromosManagerCoordinator() {
    coordinator_ = [[PromosManagerCoordinator alloc]
            initWithBaseViewController:view_controller_
                               browser:browser_.get()
        credentialProviderPromoHandler:OCMStrictProtocolMock(@protocol(
                                           CredentialProviderPromoCommands))
                   dockingPromoHandler:OCMStrictProtocolMock(
                                           @protocol(DockingPromoCommands))];
  }

  // Forces the test promo for display.
  void ForceTestPromo() {
    [[NSUserDefaults standardUserDefaults]
        setObject:@"promos_manager::Promo::Test"
           forKey:@"NextPromoForDisplayOverride"];
  }

  // Sets up the UI to be ready for promo display.
  void SetupUIForPromoDisplay() {
    browser_.get()->GetSceneState().activationLevel =
        SceneActivationLevelForegroundActive;
  }

 protected:
  IOSChromeScopedTestingLocalState local_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
  PromosManagerCoordinator* coordinator_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  ScopedKeyWindow scoped_key_window_;
  UIViewController* view_controller_;
  FakeStartupInformation* startup_information_;
  FakeSceneState* scene_state_;
};

}  // namespace

// Tests a provider's standardPromoDismissAction is called when a
// viewController's dismiss button is pressed.
TEST_F(PromosManagerCoordinatorTest,
       ViewControllerDismissesUsingDismissButton) {
  CreatePromosManagerCoordinator();

  id provider = OCMProtocolMock(@protocol(StandardPromoViewProvider));
  coordinator_.provider = provider;

  OCMExpect([provider standardPromoDismissAction]);

  [coordinator_ confirmationAlertDismissAction];

  [provider verify];
}

// Tests a banneredProvider's standardPromoDismissAction is called when a
// banneredViewController's dismiss button is pressed.
TEST_F(PromosManagerCoordinatorTest,
       BanneredViewControllerDismissesUsingSecondaryButton) {
  CreatePromosManagerCoordinator();

  id banneredProvider = OCMProtocolMock(@protocol(BanneredPromoViewProvider));
  coordinator_.banneredProvider = banneredProvider;

  OCMExpect([banneredProvider standardPromoDismissAction]);

  [coordinator_ confirmationAlertDismissAction];

  [banneredProvider verify];
}

// Tests standardPromoDismissSwipe is called when a viewController is
// dismissed via swipe.
TEST_F(PromosManagerCoordinatorTest, ViewControllerDismissesViaSwipe) {
  CreatePromosManagerCoordinator();

  id provider = OCMProtocolMock(@protocol(StandardPromoViewProvider));
  coordinator_.provider = provider;

  OCMExpect([provider standardPromoDismissSwipe]);

  [coordinator_ presentationControllerDidDismiss:nil];

  [provider verify];
}

// Tests standardPromoDismissSwipe is called when a banneredViewController is
// dismissed via swipe.
TEST_F(PromosManagerCoordinatorTest, BanneredViewControllerDismissesViaSwipe) {
  CreatePromosManagerCoordinator();

  id banneredProvider = OCMProtocolMock(@protocol(BanneredPromoViewProvider));
  coordinator_.banneredProvider = banneredProvider;

  OCMExpect([banneredProvider standardPromoDismissSwipe]);

  [coordinator_ presentationControllerDidDismiss:nil];

  [banneredProvider verify];
}

// Tests that promo will be displayed if UI is available.
TEST_F(PromosManagerCoordinatorTest, DisplayPromoCallbackTest) {
  // Prepare UI for promo display.
  SetupUIForPromoDisplay();

  CreatePromosManagerCoordinator();
  id mockCoordinator = OCMPartialMock(coordinator_);

  // Force test promos so there at least one promo to display.
  ForceTestPromo();

  // Check that test promo will be displayed.
  PromoDisplayData promoDisplayData = PromoDisplayData{
      .promo = promos_manager::Promo::Test, .was_forced = true};
  OCMExpect([mockCoordinator displayPromo:promoDisplayData]);

  [mockCoordinator displayPromoCallback:true];

  [mockCoordinator verify];
}

// Tests that promo will not be displayed if UI is not available.
TEST_F(PromosManagerCoordinatorTest, DisplayPromoCallbackUINotAvailableTest) {
  // Prepare UI for promo display.
  SetupUIForPromoDisplay();
  CreatePromosManagerCoordinator();

  id mockCoordinator = OCMPartialMock(coordinator_);

  // Force test promos so there at least one promo to display.
  ForceTestPromo();

  // Check that test promo will not be displayed.
  PromoDisplayData promoDisplayData = PromoDisplayData{
      .promo = promos_manager::Promo::Test, .was_forced = true};
  OCMReject([mockCoordinator displayPromo:promoDisplayData]);

  // Set UI not available for promo display before calling
  // ```displayPromoCallback```
  browser_.get()->GetSceneState().activationLevel =
      SceneActivationLevelBackground;
  [mockCoordinator displayPromoCallback:true];

  [mockCoordinator verify];
}

// TODO(crbug.com/40241101): Add unit tests for promoWasDisplayed being
// called when promo is displayed.
