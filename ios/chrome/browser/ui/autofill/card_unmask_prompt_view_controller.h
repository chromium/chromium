// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_CARD_UNMASK_PROMPT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_CARD_UNMASK_PROMPT_VIEW_CONTROLLER_H_
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

// IOS Ui for the Autofill Card Unmask Prompt.
//
// This screen is displayed when the user needs to verify a saved credit card
// (e.g: when trying to auto fill the card in a payment form). It asks the user
// to input the Card Verification Code (CVC). If the the card had expired and
// has a new CVC and expiration date, it asks the user to input the new CVC and
// expiration date. Once the card is verified the prompt is dismissed and the
// operation requiring the card verification is continued (e.g: the card is auto
// filled in a payment form).
@interface CardUnmaskPromptViewController : ChromeTableViewController

@end
#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_CARD_UNMASK_PROMPT_VIEW_CONTROLLER_H_
