// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bring_android_tabs/ui_bundled/bring_android_tabs_test_utils.h"

#import <memory>

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/bring_android_tabs/ui_bundled/bring_android_tabs_app_interface.h"
#import "ios/chrome/browser/bring_android_tabs/ui_bundled/bring_android_tabs_test_session.h"
#import "ios/chrome/browser/bring_android_tabs/ui_bundled/constants.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_app_interface.h"
#import "ios/chrome/browser/safari_data_import/test/safari_data_import_earl_grey_ui.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "url/gurl.h"

namespace {

// Timeout in seconds to wait for asynchronous sync operations.
constexpr base::TimeDelta kSyncOperationTimeout = base::Seconds(10);

// Taps a promo button.
void TapPromoStyleButton(id<GREYMatcher> buttonMatcher) {
  id<GREYMatcher> scrollViewMatcher =
      grey_accessibilityID(kPromoStyleScrollViewAccessibilityIdentifier);
  // Needs to scroll slowly to make sure to not miss a cell if it is not
  // currently on the screen. It should not be bigger than the visible part
  // of the collection view.
  id<GREYAction> searchAction = grey_scrollInDirection(kGREYDirectionDown, 200);
  GREYElementInteraction* element =
      [[EarlGrey selectElementWithMatcher:buttonMatcher]
             usingSearchAction:searchAction
          onElementWithMatcher:scrollViewMatcher];
  [element performAction:grey_tap()];
}

}  // namespace

AppLaunchConfiguration GetConfiguration(BOOL is_android_switcher) {
  AppLaunchConfiguration config;
  config.additional_args.push_back("-FirstRunForceEnabled");
  config.additional_args.push_back("true");
  if (is_android_switcher) {
    config.additional_args.push_back("-ForceExperienceForDeviceSwitcher");
    config.additional_args.push_back("AndroidPhone");
  }
  // TODO(crbug.com/379306137): If feature is not launched, fix
  // SignInViaFREWithHistorySyncEnabled() by moving the default browser
  // dismissal to after sign-in and sync.
  config.additional_args.push_back("--enable-features=UpdatedFirstRunSequence");
  // Relaunch app at each test to rewind the startup state.
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  return config;
}

void SignInViaFREWithHistorySyncEnabled(BOOL enable_history_sync) {
  FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fake_identity];
  // Default browser promo dismissal.
  TapPromoStyleButton(chrome_test_util::ButtonStackSecondaryButton());
  // Sign in.
  TapPromoStyleButton(chrome_test_util::ButtonStackPrimaryButton());
  // Enable history/tab sync if appropriate.
  TapPromoStyleButton(enable_history_sync
                          ? chrome_test_util::ButtonStackPrimaryButton()
                          : chrome_test_util::ButtonStackSecondaryButton());
  // If Safari import landing sheet is displayed, dismiss it.
  DismissSafariDataImportEntryPoint(/*verify_visibility=*/false);
  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncOperationTimeout];
}

void AddSessionToFakeSyncServerFromTestServer(
    BringAndroidTabsTestSession session,
    const GURL& test_server) {
  [BringAndroidTabsAppInterface
      addFakeSyncServerSession:session
                fromTestServer:base::SysUTF8ToNSString(test_server.spec())];
}

void VerifyConfirmationAlertPromptVisibility(BOOL visibility) {
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBringAndroidTabsPromptConfirmationAlertAXId)]
      assertWithMatcher:visibility ? grey_sufficientlyVisible()
                                   : grey_notVisible()];
}

void VerifyTabListPromptVisibility(BOOL visibility) {
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBringAndroidTabsPromptTabListAXId)]
      assertWithMatcher:visibility ? grey_sufficientlyVisible()
                                   : grey_notVisible()];
}

int GetTabCountOnPrompt() {
  return [BringAndroidTabsAppInterface tabsCountForPrompt];
}

void VerifyThatPromptDoesNotShowOnRestart(const GURL& test_server) {
  AppLaunchConfiguration config = GetConfiguration(/*is_android_switcher=*/YES);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
  AddSessionToFakeSyncServerFromTestServer(
      BringAndroidTabsTestSession::kRecentFromAndroidPhone, test_server);
  SignInViaFREWithHistorySyncEnabled(YES);
  [ChromeEarlGreyUI openTabGrid];
  VerifyConfirmationAlertPromptVisibility(NO);
}

void CleanUp() {
  [SigninEarlGrey signOut];
  [ChromeEarlGrey clearFakeSyncServerData];
  [ChromeEarlGrey
      clearUserPrefWithName:"ios.bring_android_tabs.prompt_displayed"];
}
