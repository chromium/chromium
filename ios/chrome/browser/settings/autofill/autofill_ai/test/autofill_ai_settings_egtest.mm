// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <optional>

#import "base/time/time.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "components/autofill/core/browser/field_types.h"
#import "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/public/autofill_ai_settings_constants.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/test/autofill_ai_settings_test_util.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_settings_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_constants.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

using chrome_test_util::SettingsMenuBackButton;

namespace {

NSString* const kOwnerName = @"autofilltestuser";
NSString* const kRedressNumber = @"1234567";
NSString* const kNewOwnerName = @"autofilltestuser2";

}  // namespace

@interface AutofillAISettingsEGTest : ChromeTestCase
@end

@implementation AutofillAISettingsEGTest

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(
      autofill::features::kAutofillAiWithDataSchema);
  config.features_enabled.push_back(
      autofill::features::kAutofillAiCreateEntityDataManager);
  config.features_enabled.push_back(
      autofill::features::kAutofillAiReauthRequired);
  return config;
}

- (void)setUp {
  [super setUp];
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
}

// Tests entity editing from "Addresses and more" in Settings.
- (void)testEntityEdit {
  NSString* uuidString =
      [AutofillAppInterface saveRedressNumberEntityWithName:kOwnerName
                                                     number:kRedressNumber];
  GREYAssertNotNil(uuidString, @"Failed to create an entity.");

  // Open "Addresses and more", the pre-HoT location.
  [AutofillAISettingsTestUtil openPreHoTLocation];

  // Check the newly saved entity is visible.
  [AutofillAISettingsTestUtil entityWithLabel:kOwnerName isVisible:YES];

  // Tap the entity to open the entity edit view.
  [AutofillAISettingsTestUtil tapEntityWithLabel:kOwnerName];

  // Check the entity edit view is visible.
  [AutofillAISettingsTestUtil entityEditViewIsVisible:YES];

  // Start editing the entity.
  [AutofillAISettingsTestUtil startFieldEditing];

  // Tap the UITextField to edit the owner name.
  id<GREYMatcher> textFieldMatcher = [AutofillAISettingsTestUtil
      textFieldForType:autofill::AttributeTypeName::kRedressNumberName];

  [[EarlGrey selectElementWithMatcher:textFieldMatcher]
      performAction:grey_tap()];

  // Replace the owner name with a new name.
  [[EarlGrey selectElementWithMatcher:textFieldMatcher]
      performAction:grey_replaceText(kNewOwnerName)];

  // Tap the edit done button to save the change.
  [AutofillAISettingsTestUtil tapEditDoneButton];

  // Tap the back button to return to the previous screen.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];

  // Check the new entity is visible and the old one is not.
  [AutofillAISettingsTestUtil entityWithLabel:kNewOwnerName isVisible:YES];
  [AutofillAISettingsTestUtil entityWithLabel:kOwnerName isVisible:NO];

  // Clean up the created entity.
  [AutofillAppInterface removeEntityWithUUID:uuidString];
}

@end
