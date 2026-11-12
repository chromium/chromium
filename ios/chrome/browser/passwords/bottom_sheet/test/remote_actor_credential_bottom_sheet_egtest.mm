// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "components/password_manager/core/browser/remote_actor/remote_actor_config.h"
#import "components/password_manager/core/browser/remote_actor/remote_actor_credential_sharing_result.h"
#import "components/password_manager/core/common/remote_actor/remote_actor_features.h"
#import "components/strings/grit/components_strings.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/passwords/bottom_sheet/test/remote_actor_credential_bottom_sheet_app_interface.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_manager_egtest_utils.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_settings_app_interface.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/common/ui/button_stack/button_stack_constants.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

using password_manager::RemoteActorCredentialSharingResult;

namespace {

// Domain targeted by the fake Remote Actor request.
NSString* const kRequestDomain = @"example.com";

// Site of the saved credentials, matching `kRequestDomain`.
NSString* const kCredentialSite = @"https://example.com/";

// Password of the saved credentials.
NSString* const kCredentialPassword = @"password";

// Username of the saved credential suggested in the bottom sheet.
NSString* const kSuggestion = @"user1@example.com";

// Histogram recording the final result of the sharing flow.
NSString* const kResultHistogram =
    @"PasswordManager.RemoteActorCredentialSharing.Result";

// Number of suggestions exceeding what fits on screen.
constexpr int kManySuggestionsCount = 8;

// Scroll amount, in points, used to reach suggestions outside of the screen.
constexpr CGFloat kScrollAmount = 150;

// Returns the username of the `index`-th generated suggestion.
NSString* GeneratedSuggestion(int index) {
  return [NSString stringWithFormat:@"user%d@example.com", index];
}

// Returns the matcher for the "Allow this time" button.
id<GREYMatcher> AllowButton() {
  return grey_allOf(
      grey_accessibilityID(kButtonStackPrimaryActionAccessibilityIdentifier),
      chrome_test_util::ButtonWithAccessibilityLabelId(
          IDS_REMOTE_ACTOR_SHARING_DIALOG_ALLOW_BUTTON),
      grey_sufficientlyVisible(), nil);
}

// Returns the matcher for the "Cancel" button.
id<GREYMatcher> CancelButton() {
  return grey_allOf(
      grey_accessibilityID(kButtonStackSecondaryActionAccessibilityIdentifier),
      chrome_test_util::ButtonWithAccessibilityLabelId(IDS_CANCEL),
      grey_sufficientlyVisible(), nil);
}

// Returns the matcher for the bottom sheet title.
id<GREYMatcher> Title() {
  NSString* title = l10n_util::GetNSStringF(
      IDS_REMOTE_ACTOR_SHARING_DIALOG_TITLE,
      l10n_util::GetStringUTF16(IDS_REMOTE_ACTOR_GEMINI_NAME),
      base::SysNSStringToUTF16(kRequestDomain));
  return grey_allOf(
      grey_accessibilityID(kConfirmationAlertTitleAccessibilityIdentifier),
      grey_text(title), nil);
}

// Returns the matcher for the cell of `suggestion`.
id<GREYMatcher> SuggestionCell(NSString* suggestion) {
  return grey_allOf(grey_accessibilityID(suggestion),
                    grey_kindOfClassName(@"UITableViewCell"),
                    grey_sufficientlyVisible(), nil);
}

// Returns the matcher for the learn-more link of the subtitle.
id<GREYMatcher> LearnMoreLink() {
  return grey_allOf(grey_ancestor(grey_accessibilityID(
                        kConfirmationAlertSubtitleAccessibilityIdentifier)),
                    // UIKit instantiates a `UIAccessibilityLinkSubelement` for
                    // the link element in the text view with attributed string.
                    grey_kindOfClassName(@"UIAccessibilityLinkSubelement"),
                    grey_accessibilityTrait(UIAccessibilityTraitLink), nil);
}

// Returns the matcher for the scroll view containing the bottom sheet content.
id<GREYMatcher> ContentScrollView() {
  return grey_allOf(grey_kindOfClass([UIScrollView class]),
                    grey_descendant(grey_accessibilityID(
                        kConfirmationAlertTitleAccessibilityIdentifier)),
                    nil);
}

// Returns a matcher for a table view cell displaying a checkmark.
id<GREYMatcher> HasCheckmark() {
  return [GREYElementMatcherBlock
      matcherWithMatchesBlock:^BOOL(id element) {
        return [element accessoryType] == UITableViewCellAccessoryCheckmark;
      }
      descriptionBlock:^(id<GREYDescription> description) {
        [description appendText:@"has a checkmark"];
      }];
}

// Asserts whether the cell of `suggestion` displays a checkmark.
void AssertCheckmark(NSString* suggestion, BOOL expected) {
  [[EarlGrey selectElementWithMatcher:SuggestionCell(suggestion)]
      assertWithMatcher:expected ? HasCheckmark() : grey_not(HasCheckmark())];
}

// Waits until the sharing flow recorded `result` as its unique result.
void WaitForResult(RemoteActorCredentialSharingResult result,
                   base::TimeDelta timeout) {
  BOOL recorded = base::test::ios::WaitUntilConditionOrTimeout(timeout, ^bool {
    return [MetricsAppInterface
               expectUniqueSampleWithCount:1
                                 forBucket:static_cast<int>(result)
                              forHistogram:kResultHistogram] == nil;
  });
  GREYAssertTrue(recorded, @"Expected result %d was not recorded.",
                 static_cast<int>(result));
}

// Taps "Cancel" and waits until the bottom sheet is dismissed.
void DismissBottomSheetWithCancel() {
  [[EarlGrey selectElementWithMatcher:CancelButton()] performAction:grey_tap()];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:Title()];
}

}  // namespace

// EarlGrey tests for `RemoteActorCredentialBottomSheetViewController`.
@interface RemoteActorCredentialBottomSheetTestCase : ChromeTestCase
@end

@implementation RemoteActorCredentialBottomSheetTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = NoForceRelaunchAndResetState;
  config.features_enabled.push_back(
      password_manager::features::kRemoteActorCredentialSharing);
  config.features_disabled.push_back(kEnableNewStartupFlow);
  return config;
}

- (void)setUp {
  [super setUp];
  chrome_test_util::GREYAssertErrorNil(
      [MetricsAppInterface setupHistogramTester]);
}

- (void)tearDownHelper {
  [RemoteActorCredentialBottomSheetAppInterface cleanUpRemoteActorKeychain];
  GREYAssertTrue([PasswordSettingsAppInterface clearPasswordStores],
                 @"Clearing password stores failed.");
  chrome_test_util::GREYAssertErrorNil(
      [MetricsAppInterface releaseHistogramTester]);
  [super tearDownHelper];
}

#pragma mark - Helpers

// Signs in, saves a credential to the account store for each of `suggestions`
// and opens a valid Remote Actor URL for `kRequestDomain`.
- (void)openRemoteActorURLWithSuggestions:(NSArray<NSString*>*)suggestions {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];

  for (NSString* suggestion in suggestions) {
    password_manager_test_utils::SavePasswordFormToAccountStore(
        kCredentialPassword, suggestion, kCredentialSite);
  }

  NSString* urlString = [RemoteActorCredentialBottomSheetAppInterface
      createValidRemoteActorURLForDomain:kRequestDomain
                                  gaiaID:fakeIdentity.gaiaId.ToNSString()];
  GREYAssertNotNil(urlString, @"Failed to create valid Remote Actor URL.");

  [ChromeEarlGrey sceneOpenURL:GURL(base::SysNSStringToUTF8(urlString))];
}

// Opens the bottom sheet with `suggestions` and waits until they are displayed.
- (void)openBottomSheetWithSuggestions:(NSArray<NSString*>*)suggestions {
  [self openRemoteActorURLWithSuggestions:suggestions];
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:Title()];
  for (NSString* suggestion in suggestions) {
    [ChromeEarlGrey
        waitForUIElementToAppearWithMatcher:SuggestionCell(suggestion)];
  }
}

#pragma mark - Tests

// Verifies that navigating to a valid Remote Actor URL displays
// `RemoteActorCredentialBottomSheetViewController` when a password is saved
// for the requested domain.
- (void)testBottomSheetShowsForValidRemoteActorURL {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];

  password_manager_test_utils::SavePasswordFormToAccountStore(
      @"password", @"user@example.com", @"https://example.com/");

  NSString* URLString = [RemoteActorCredentialBottomSheetAppInterface
      createValidRemoteActorURLForDomain:@"example.com"
                                  gaiaID:fakeIdentity.gaiaId.ToNSString()];
  GREYAssertNotNil(URLString, @"Failed to create valid Remote Actor URL.");

  [ChromeEarlGrey sceneOpenURL:GURL(base::SysNSStringToUTF8(URLString))];

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:AllowButton()];
  [[EarlGrey selectElementWithMatcher:CancelButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  DismissBottomSheetWithCancel();
}

// Tests that swiping the bottom sheet down dismisses it and cancels the flow.
- (void)testDismissBottomSheet {
  [self openBottomSheetWithSuggestions:@[ kSuggestion ]];

  [[EarlGrey selectElementWithMatcher:Title()]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:Title()];
  WaitForResult(RemoteActorCredentialSharingResult::kUserCancelledDialog,
                base::test::ios::kWaitForUIElementTimeout);
}

// Tests that tapping the learn-more link dismisses the bottom sheet and opens
// the learn-more page. The page reuses the current tab, as it is the NTP.
- (void)testTapLearnMoreLink {
  [self openBottomSheetWithSuggestions:@[ kSuggestion ]];

  [[EarlGrey selectElementWithMatcher:LearnMoreLink()]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:Title()];
  const GURL learnMoreURL = password_manager::GetRemoteActorConfig(
                                password_manager::RemoteActorType::kGemini)
                                .learn_more_url;
  [ChromeEarlGrey waitForWebStateVisibleURL:learnMoreURL];
}

// Tests that suggestions that don't fit on screen can be scrolled to and
// selected.
- (void)testManySuggestions {
  NSMutableArray<NSString*>* suggestions = [NSMutableArray array];
  for (int i = 0; i < kManySuggestionsCount; ++i) {
    [suggestions addObject:GeneratedSuggestion(i)];
  }
  [self openRemoteActorURLWithSuggestions:suggestions];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:SuggestionCell(
                                              suggestions.firstObject)];

  for (NSString* suggestion in suggestions) {
    [[[EarlGrey selectElementWithMatcher:SuggestionCell(suggestion)]
           usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                    kScrollAmount)
        onElementWithMatcher:ContentScrollView()]
        assertWithMatcher:grey_notNil()];
  }

  NSString* lastSuggestion = suggestions.lastObject;
  [[EarlGrey selectElementWithMatcher:SuggestionCell(lastSuggestion)]
      performAction:grey_tap()];
  AssertCheckmark(lastSuggestion, YES);

  DismissBottomSheetWithCancel();
}

// Verifies that navigating to a valid Remote Actor URL for a domain without
// saved passwords displays the no saved passwords state with a single "Back to
// Gemini" button, which dismisses the bottom sheet.
- (void)testBottomSheetShowsNoSavedPasswords {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];

  NSString* URLString = [RemoteActorCredentialBottomSheetAppInterface
      createValidRemoteActorURLForDomain:@"example.com"
                                  gaiaID:fakeIdentity.gaiaId.ToNSString()];
  GREYAssertNotNil(URLString, @"Failed to create valid Remote Actor URL.");

  [ChromeEarlGrey sceneOpenURL:GURL(base::SysNSStringToUTF8(URLString))];

  std::u16string actorName =
      l10n_util::GetStringUTF16(IDS_REMOTE_ACTOR_GEMINI_NAME);
  id<GREYMatcher> backToGeminiButtonMatcher = grey_allOf(
      grey_accessibilityID(kButtonStackPrimaryActionAccessibilityIdentifier),
      chrome_test_util::ButtonWithAccessibilityLabel(l10n_util::GetNSStringF(
          IDS_IOS_REMOTE_ACTOR_NO_PASSWORD_DIALOG_BACK_BUTTON, actorName)),
      grey_sufficientlyVisible(), nil);
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:backToGeminiButtonMatcher];

  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityID(
                                kConfirmationAlertTitleAccessibilityIdentifier),
                            grey_accessibilityLabel(l10n_util::GetNSStringF(
                                IDS_IOS_REMOTE_ACTOR_NO_PASSWORD_DIALOG_TITLE,
                                u"example.com")),
                            nil)] assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              grey_accessibilityID(
                  kConfirmationAlertSubtitleAccessibilityIdentifier),
              grey_text(l10n_util::GetNSStringF(
                  IDS_IOS_REMOTE_ACTOR_NO_PASSWORD_DIALOG_SUBTITLE, actorName)),
              nil)] assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_accessibilityID(
                         kButtonStackSecondaryActionAccessibilityIdentifier),
                     grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:backToGeminiButtonMatcher]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:backToGeminiButtonMatcher];
}

@end
