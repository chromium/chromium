// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_CARD_UNMASK_PROMPT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_CARD_UNMASK_PROMPT_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"

namespace autofill {
class CardUnmaskPromptViewBridge;
}

// IOS UI for the Autofill Card Unmask Prompt.
//
// This view controller is presented when the user needs to verify a saved
// credit card (e.g: when trying to autofill the card in a payment form). It
// asks the user to input the Card Verification Code (CVC). If the the card had
// expired and has a new CVC and expiration date, it asks the user to input the
// new CVC and expiration date. Once the card is verified the prompt is
// dismissed and the operation requiring the card verification is continued
// (e.g: the card is autofilled in a payment form).
@interface CardUnmaskPromptViewController
    : LegacyChromeTableViewController <UIAdaptivePresentationControllerDelegate>
// Designated initializer. `bridge` must not be null.
- (instancetype)initWithBridge:(autofill::CardUnmaskPromptViewBridge*)bridge
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle*)style NS_UNAVAILABLE;

// Replaces the right button in the navigation bar with an activity indicator
// and disables user interactions with the tableView.
- (void)showLoadingState;

// Displays an error alert with the given message.
// `closeOnDismiss` indicates if the ViewController should be dismissed when the
// alert is dismissed.
- (void)showErrorAlertWithMessage:(NSString*)message
                   closeOnDismiss:(BOOL)closeOnDismiss;

// Called when the bridge is about to be deallocated.
- (void)disconnectFromBridge;

@end
#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_CARD_UNMASK_PROMPT_VIEW_CONTROLLER_H_
