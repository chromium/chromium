// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/test/safari_data_import_earl_grey_ui.h"

#import "ios/chrome/browser/safari_data_import/public/utils.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

void DismissSafariDataImportEntryPoint(bool verify_visibility) {
  bool visible = [ChromeEarlGrey
      testUIElementAppearanceWithMatcher:
          grey_accessibilityID(
              GetSafariDataEntryPointAccessibilityIdentifier())];
  if (visible) {
    [[EarlGrey
        selectElementWithMatcher:
            grey_accessibilityID(
                kConfirmationAlertSecondaryActionAccessibilityIdentifier)]
        performAction:grey_tap()];
  }
  if (verify_visibility) {
    GREYAssertTrue(visible, @"Safari import landing page is not found.");
  }
}
