// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_extension/ui_bundled/web_extension_error_scene_agent.h"

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "components/prefs/pref_service.h"
#import "components/universal_optout/features.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/fake_startup_information.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/web_extension/model/extension_service_factory.h"
#import "ios/chrome/browser/web_extension/model/fake_extension_service.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

std::unique_ptr<KeyedService> BuildFakeExtensionService(ProfileIOS* profile) {
  return std::make_unique<FakeExtensionService>();
}

class WebExtensionErrorSceneAgentTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(ExtensionServiceFactory::GetInstance(),
                              base::BindOnce(&BuildFakeExtensionService));
    profile_ = std::move(builder).Build();

    fake_startup_information_ = [[FakeStartupInformation alloc] init];
    app_state_ =
        [[AppState alloc] initWithStartupInformation:fake_startup_information_];

    profile_state_ = [[ProfileState alloc] initWithAppState:app_state_];
    profile_state_.profile = profile_.get();

    scene_state_ = [[FakeSceneState alloc] initWithProfile:profile_.get()];
    scene_state_.profileState = profile_state_;

    Browser* browser =
        scene_state_.browserProviderInterface.currentBrowserProvider.browser;
    CommandDispatcher* dispatcher = browser->GetCommandDispatcher();

    mock_snackbar_commands_ = OCMProtocolMock(@protocol(SnackbarCommands));
    [dispatcher startDispatchingToTarget:mock_snackbar_commands_
                             forProtocol:@protocol(SnackbarCommands)];

    mock_settings_commands_ = OCMProtocolMock(@protocol(SettingsCommands));
    [dispatcher startDispatchingToTarget:mock_settings_commands_
                             forProtocol:@protocol(SettingsCommands)];

    agent_ = [[WebExtensionErrorSceneAgent alloc] init];
    [scene_state_ addAgent:agent_];
  }

  void TearDown() override {
    [scene_state_ shutdown];
    agent_ = nil;
    scene_state_ = nil;
    profile_state_ = nil;
    app_state_ = nil;
    fake_startup_information_ = nil;
    PlatformTest::TearDown();
  }

  void ResetMockSnackbarCommands() {
    Browser* browser =
        scene_state_.browserProviderInterface.currentBrowserProvider.browser;
    CommandDispatcher* dispatcher = browser->GetCommandDispatcher();
    [dispatcher stopDispatchingForProtocol:@protocol(SnackbarCommands)];
    mock_snackbar_commands_ = OCMProtocolMock(@protocol(SnackbarCommands));
    [dispatcher startDispatchingToTarget:mock_snackbar_commands_
                             forProtocol:@protocol(SnackbarCommands)];
  }

  FakeExtensionService* fake_extension_service() {
    return static_cast<FakeExtensionService*>(
        ExtensionServiceFactory::GetForProfile(profile_.get()));
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  FakeStartupInformation* fake_startup_information_;
  AppState* app_state_;
  ProfileState* profile_state_;
  FakeSceneState* scene_state_;
  WebExtensionErrorSceneAgent* agent_;
  id mock_snackbar_commands_;
  id mock_settings_commands_;
};

// Tests that when there is no error, activating the scene does not show a
// snackbar.
TEST_F(WebExtensionErrorSceneAgentTest, NoErrorDoesNotShowSnackbar) {
  scene_state_.UIEnabled = YES;

  OCMReject([mock_snackbar_commands_ showSnackbarWithMessage:[OCMArg any]
                                                  buttonText:[OCMArg any]
                                               messageAction:[OCMArg any]
                                            completionAction:[OCMArg any]]);

  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  EXPECT_OCMOCK_VERIFY(mock_snackbar_commands_);
}

// Tests that a startup error (occurring before the scene is foreground active)
// triggers the snackbar when the scene transitions to foreground active, and
// does not trigger again on subsequent foreground activations.
TEST_F(WebExtensionErrorSceneAgentTest,
       StartupErrorShowsSnackbarOnForegroundActive) {
  scene_state_.UIEnabled = YES;
  scene_state_.activationLevel = SceneActivationLevelForegroundInactive;

  // Extension load error occurs at startup before scene becomes active.
  fake_extension_service()->SetHasGPCLoadError(true);

  NSString* expected_message =
      l10n_util::GetNSString(IDS_IOS_UNIVERSAL_OPT_OUT_SNACKBAR_MESSAGE);
  NSString* expected_button_text =
      l10n_util::GetNSString(IDS_IOS_UNIVERSAL_OPT_OUT_SNACKBAR_ACTION);

  OCMExpect([mock_snackbar_commands_
      showSnackbarWithMessage:expected_message
                   buttonText:expected_button_text
                messageAction:[OCMArg any]
             completionAction:[OCMArg any]]);

  // Transition to active.
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  EXPECT_OCMOCK_VERIFY(mock_snackbar_commands_);

  // Transition to background and foreground again should not show another
  // snackbar for the same error.
  OCMReject([mock_snackbar_commands_ showSnackbarWithMessage:[OCMArg any]
                                                  buttonText:[OCMArg any]
                                               messageAction:[OCMArg any]
                                            completionAction:[OCMArg any]]);
  scene_state_.activationLevel = SceneActivationLevelBackground;
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  EXPECT_OCMOCK_VERIFY(mock_snackbar_commands_);
}

// Tests that a dynamic error occurring while the scene is already foreground
// active triggers the snackbar immediately.
TEST_F(WebExtensionErrorSceneAgentTest,
       DynamicErrorWhileForegroundActiveShowsSnackbar) {
  scene_state_.UIEnabled = YES;
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  NSString* expected_message =
      l10n_util::GetNSString(IDS_IOS_UNIVERSAL_OPT_OUT_SNACKBAR_MESSAGE);
  NSString* expected_button_text =
      l10n_util::GetNSString(IDS_IOS_UNIVERSAL_OPT_OUT_SNACKBAR_ACTION);

  OCMExpect([mock_snackbar_commands_
      showSnackbarWithMessage:expected_message
                   buttonText:expected_button_text
                messageAction:[OCMArg any]
             completionAction:[OCMArg any]]);

  fake_extension_service()->SetHasGPCLoadError(true);

  EXPECT_OCMOCK_VERIFY(mock_snackbar_commands_);
}

// Tests that an error occurring while backgrounded does not show a snackbar,
// but triggers it when transitioning to foreground active, and re-backgrounding
// and re-foregrounding does not display the snackbar a second time.
TEST_F(WebExtensionErrorSceneAgentTest,
       BackgroundErrorShowsSnackbarWhenForegrounded) {
  scene_state_.UIEnabled = YES;
  scene_state_.activationLevel = SceneActivationLevelBackground;

  // An error occurring while backgrounded must not show the snackbar.
  OCMReject([mock_snackbar_commands_ showSnackbarWithMessage:[OCMArg any]
                                                  buttonText:[OCMArg any]
                                               messageAction:[OCMArg any]
                                            completionAction:[OCMArg any]]);

  fake_extension_service()->SetHasGPCLoadError(true);
  EXPECT_OCMOCK_VERIFY(mock_snackbar_commands_);
  EXPECT_TRUE(fake_extension_service()->HasGPCLoadError());

  // Reset the mock so the previous rejection does not prevent the expectation.
  ResetMockSnackbarCommands();

  // Transitioning to foreground active should display the snackbar once.
  NSString* expected_message =
      l10n_util::GetNSString(IDS_IOS_UNIVERSAL_OPT_OUT_SNACKBAR_MESSAGE);
  NSString* expected_button_text =
      l10n_util::GetNSString(IDS_IOS_UNIVERSAL_OPT_OUT_SNACKBAR_ACTION);

  OCMExpect([mock_snackbar_commands_
      showSnackbarWithMessage:expected_message
                   buttonText:expected_button_text
                messageAction:[OCMArg any]
             completionAction:[OCMArg any]]);

  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  EXPECT_OCMOCK_VERIFY(mock_snackbar_commands_);

  // Re-backgrounding and re-foregrounding must not display the snackbar again.
  OCMReject([mock_snackbar_commands_ showSnackbarWithMessage:[OCMArg any]
                                                  buttonText:[OCMArg any]
                                               messageAction:[OCMArg any]
                                            completionAction:[OCMArg any]]);

  scene_state_.activationLevel = SceneActivationLevelBackground;
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  EXPECT_OCMOCK_VERIFY(mock_snackbar_commands_);
}

// Tests that transitioning back to no load error does not show a snackbar.
TEST_F(WebExtensionErrorSceneAgentTest, NoErrorChangeDoesNotShowSnackbar) {
  scene_state_.UIEnabled = YES;
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  OCMReject([mock_snackbar_commands_ showSnackbarWithMessage:[OCMArg any]
                                                  buttonText:[OCMArg any]
                                               messageAction:[OCMArg any]
                                            completionAction:[OCMArg any]]);

  fake_extension_service()->SetHasGPCLoadError(false);
  EXPECT_OCMOCK_VERIFY(mock_snackbar_commands_);
}

// Tests that an error occurring while backgrounded that fixes itself before the
// scene becomes foreground active still displays the snackbar when
// foregrounded.
TEST_F(WebExtensionErrorSceneAgentTest,
       BackgroundErrorThatClearsStillShowsSnackbarWhenForegrounded) {
  scene_state_.UIEnabled = YES;
  scene_state_.activationLevel = SceneActivationLevelBackground;

  // An error occurring while backgrounded must not show the snackbar.
  OCMReject([mock_snackbar_commands_ showSnackbarWithMessage:[OCMArg any]
                                                  buttonText:[OCMArg any]
                                               messageAction:[OCMArg any]
                                            completionAction:[OCMArg any]]);

  fake_extension_service()->SetHasGPCLoadError(true);
  EXPECT_OCMOCK_VERIFY(mock_snackbar_commands_);

  // Error fixes itself while still backgrounded.
  fake_extension_service()->SetHasGPCLoadError(false);
  EXPECT_OCMOCK_VERIFY(mock_snackbar_commands_);

  ResetMockSnackbarCommands();

  // Transitioning to foreground active should still display the snackbar.
  NSString* expected_message =
      l10n_util::GetNSString(IDS_IOS_UNIVERSAL_OPT_OUT_SNACKBAR_MESSAGE);
  NSString* expected_button_text =
      l10n_util::GetNSString(IDS_IOS_UNIVERSAL_OPT_OUT_SNACKBAR_ACTION);

  OCMExpect([mock_snackbar_commands_
      showSnackbarWithMessage:expected_message
                   buttonText:expected_button_text
                messageAction:[OCMArg any]
             completionAction:[OCMArg any]]);

  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  EXPECT_OCMOCK_VERIFY(mock_snackbar_commands_);
}

// Tests that tapping the snackbar action button navigates to Universal Opt Out
// settings.
TEST_F(WebExtensionErrorSceneAgentTest,
       SnackbarActionOpensUniversalOptOutSettings) {
  scene_state_.UIEnabled = YES;
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  __block void (^captured_action)(void) = nil;
  OCMExpect([mock_snackbar_commands_
      showSnackbarWithMessage:[OCMArg any]
                   buttonText:[OCMArg any]
                messageAction:[OCMArg checkWithBlock:^BOOL(id value) {
                  captured_action = [value copy];
                  return YES;
                }]
             completionAction:[OCMArg any]]);

  fake_extension_service()->SetHasGPCLoadError(true);
  EXPECT_OCMOCK_VERIFY(mock_snackbar_commands_);

  ASSERT_TRUE(captured_action);
  OCMExpect([mock_settings_commands_ showUniversalOptOutSettings]);
  captured_action();
  EXPECT_OCMOCK_VERIFY(mock_settings_commands_);
}

// Tests that an error occurring before UIEnabled shows the snackbar once
// UIEnabled is set.
TEST_F(WebExtensionErrorSceneAgentTest,
       ErrorBeforeUIEnabledShowsSnackbarWhenUIEnabled) {
  scene_state_.UIEnabled = NO;
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  fake_extension_service()->SetHasGPCLoadError(true);
  EXPECT_TRUE(fake_extension_service()->HasGPCLoadError());

  NSString* expected_message =
      l10n_util::GetNSString(IDS_IOS_UNIVERSAL_OPT_OUT_SNACKBAR_MESSAGE);
  NSString* expected_button_text =
      l10n_util::GetNSString(IDS_IOS_UNIVERSAL_OPT_OUT_SNACKBAR_ACTION);

  OCMExpect([mock_snackbar_commands_
      showSnackbarWithMessage:expected_message
                   buttonText:expected_button_text
                messageAction:[OCMArg any]
             completionAction:[OCMArg any]]);

  scene_state_.UIEnabled = YES;

  EXPECT_OCMOCK_VERIFY(mock_snackbar_commands_);
}

// Tests that when extension loading is in silent mode, an error does not show a
// snackbar.
TEST_F(WebExtensionErrorSceneAgentTest, SilentModeDoesNotShowSnackbar) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      universal_optout::features::kUniversalOptOutExtensionSilent);

  scene_state_.UIEnabled = YES;
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  OCMReject([mock_snackbar_commands_ showSnackbarWithMessage:[OCMArg any]
                                                  buttonText:[OCMArg any]
                                               messageAction:[OCMArg any]
                                            completionAction:[OCMArg any]]);

  fake_extension_service()->SetHasGPCLoadError(true);
  EXPECT_OCMOCK_VERIFY(mock_snackbar_commands_);
}

// Tests that the snackbar is shown only once and sets the profile pref,
// and subsequent errors do not show the snackbar again.
TEST_F(WebExtensionErrorSceneAgentTest,
       ToastOnlyDisplayedOnceAcrossMultipleErrors) {
  scene_state_.UIEnabled = YES;
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  PrefService* prefs = profile_->GetPrefs();
  ASSERT_TRUE(prefs);
  EXPECT_FALSE(prefs->GetBoolean(prefs::kUniversalOptOutErrorToastShown));

  // First error shows snackbar.
  OCMExpect([mock_snackbar_commands_ showSnackbarWithMessage:[OCMArg any]
                                                  buttonText:[OCMArg any]
                                               messageAction:[OCMArg any]
                                            completionAction:[OCMArg any]]);
  fake_extension_service()->SetHasGPCLoadError(true);
  EXPECT_OCMOCK_VERIFY(mock_snackbar_commands_);

  EXPECT_TRUE(prefs->GetBoolean(prefs::kUniversalOptOutErrorToastShown));

  // Error is resolved.
  fake_extension_service()->SetHasGPCLoadError(false);

  // Second error occurs: snackbar must NOT be shown because pref is true.
  OCMReject([mock_snackbar_commands_ showSnackbarWithMessage:[OCMArg any]
                                                  buttonText:[OCMArg any]
                                               messageAction:[OCMArg any]
                                            completionAction:[OCMArg any]]);
  fake_extension_service()->SetHasGPCLoadError(true);
  EXPECT_OCMOCK_VERIFY(mock_snackbar_commands_);
}

// Tests that if the profile pref is reset (e.g. user toggled setting off
// then on), a subsequent error shows the snackbar again.
TEST_F(WebExtensionErrorSceneAgentTest, ToastDisplayedAgainWhenPrefIsReset) {
  scene_state_.UIEnabled = YES;
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  PrefService* prefs = profile_->GetPrefs();
  ASSERT_TRUE(prefs);

  // First error shows snackbar.
  OCMExpect([mock_snackbar_commands_ showSnackbarWithMessage:[OCMArg any]
                                                  buttonText:[OCMArg any]
                                               messageAction:[OCMArg any]
                                            completionAction:[OCMArg any]]);
  fake_extension_service()->SetHasGPCLoadError(true);
  EXPECT_OCMOCK_VERIFY(mock_snackbar_commands_);

  EXPECT_TRUE(prefs->GetBoolean(prefs::kUniversalOptOutErrorToastShown));

  // Reset pref to false.
  prefs->SetBoolean(prefs::kUniversalOptOutErrorToastShown, false);
  fake_extension_service()->SetHasGPCLoadError(false);

  // Second error occurs after pref reset: snackbar should be shown again.
  OCMExpect([mock_snackbar_commands_ showSnackbarWithMessage:[OCMArg any]
                                                  buttonText:[OCMArg any]
                                               messageAction:[OCMArg any]
                                            completionAction:[OCMArg any]]);
  fake_extension_service()->SetHasGPCLoadError(true);
  EXPECT_OCMOCK_VERIFY(mock_snackbar_commands_);
  EXPECT_TRUE(prefs->GetBoolean(prefs::kUniversalOptOutErrorToastShown));
}

// Tests that if the profile pref was already set, an error does not show
// the snackbar.
TEST_F(WebExtensionErrorSceneAgentTest,
       PrefAlreadySetPreventsSnackbarAtStartup) {
  PrefService* prefs = profile_->GetPrefs();
  ASSERT_TRUE(prefs);
  prefs->SetBoolean(prefs::kUniversalOptOutErrorToastShown, true);

  scene_state_.UIEnabled = YES;
  scene_state_.activationLevel = SceneActivationLevelForegroundInactive;

  fake_extension_service()->SetHasGPCLoadError(true);

  OCMReject([mock_snackbar_commands_ showSnackbarWithMessage:[OCMArg any]
                                                  buttonText:[OCMArg any]
                                               messageAction:[OCMArg any]
                                            completionAction:[OCMArg any]]);

  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  EXPECT_OCMOCK_VERIFY(mock_snackbar_commands_);
}

}  // namespace
