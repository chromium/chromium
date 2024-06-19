// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_CARD_UNMASK_PROMPT_VIEW_CONTROLLER_TESTING_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_CARD_UNMASK_PROMPT_VIEW_CONTROLLER_TESTING_H_

#import "ios/chrome/browser/autofill/ui_bundled/card_unmask_prompt_view_controller.h"

// Testing category to expose private methods used for tests.
@interface CardUnmaskPromptViewController (Testing)
// Exposed to simulate form submissions.
- (void)onVerifyTapped;
// Exposed for testing the addition of the expiration date link.
- (void)showUpdateExpirationDateLink;
// Exposed for testing the setup of the update expiration date form.
- (void)showUpdateExpirationDateForm;
// Exposed for testing the setup of the ViewController after an error.
- (void)onErrorAlertDismissedAndShouldCloseOnDismiss:(BOOL)closeOnDismiss;
@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_CARD_UNMASK_PROMPT_VIEW_CONTROLLER_TESTING_H_
