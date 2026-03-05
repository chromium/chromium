// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/core/common/autofill_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_matchers_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Save button on the infobar.
id<GREYMatcher> infoBarSaveButton() {
  return chrome_test_util::ButtonWithAccessibilityLabel(
      l10n_util::GetNSString(IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL));
}

// UITableView to show the new entity and the old entity if there is one.
id<GREYMatcher> entitiesView() {
  return grey_accessibilityID(kAutofillAISaveEntityTableViewId);
}

// Save button on the save entity view controller.
id<GREYMatcher> saveEntityButton() {
  return chrome_test_util::ButtonWithAccessibilityLabel(
      l10n_util::GetNSString(IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL));
}

}  // namespace

@interface AutofillAIEGTest : ChromeTestCase
@end

@implementation AutofillAIEGTest

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(
      autofill::features::kAutofillAiWithDataSchema);
  return config;
}

- (void)setUp {
  [super setUp];
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
}

// Tests the infobar banner shows and can be tapped. Once tapped, the detailed
// save entity UI is shown.
- (void)testSaveEntityUI {
  // Simulate a trigger for the infobar.
  [AutofillAppInterface showAutofillAiSaveEntityBubble];

  // Tap the banner.
  [[EarlGrey selectElementWithMatcher:infoBarSaveButton()]
      performAction:grey_tap()];

  // Verify the detailed UI.
  [[EarlGrey selectElementWithMatcher:entitiesView()]
      assertWithMatcher:grey_notNil()];

  // Tap the final Save button.
  [[EarlGrey selectElementWithMatcher:saveEntityButton()]
      performAction:grey_tap()];

  // Verify the detailed UI is gone.
  [[EarlGrey selectElementWithMatcher:entitiesView()]
      assertWithMatcher:grey_nil()];
}

@end
