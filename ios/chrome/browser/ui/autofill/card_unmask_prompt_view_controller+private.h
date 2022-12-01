// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_CARD_UNMASK_PROMPT_VIEW_CONTROLLER_PRIVATE_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_CARD_UNMASK_PROMPT_VIEW_CONTROLLER_PRIVATE_H_

#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

@interface CardUnmaskPromptViewController (Private)
// Exposed to simulate form submissions.
- (void)onVerifyTapped;
// Exposed for testing the addition of the expiration date link.
- (void)showUpdateExpirationDateLink;
// Exposed for testing the setup of the update expiration date form.
- (void)showUpdateExpirationDateForm;
// Exposed for testing the setup of the ViewController after an error.
- (void)onErrorAlertDismissedAndShouldCloseOnDismiss:(BOOL)closeOnDismiss;
@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_CARD_UNMASK_PROMPT_VIEW_CONTROLLER_PRIVATE_H_
