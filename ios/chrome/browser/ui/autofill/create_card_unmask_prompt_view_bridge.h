// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_CREATE_CARD_UNMASK_PROMPT_VIEW_BRIDGE_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_CREATE_CARD_UNMASK_PROMPT_VIEW_BRIDGE_H_

#import "components/autofill/core/browser/ui/payments/card_unmask_prompt_view.h"

@class UIViewController;

namespace autofill {

class CardUnmaskPromptController;

// Creates the view bridge for the iOS implementation of the Card Unmask
// Prompt.
CardUnmaskPromptView* CreateCardUnmaskPromptViewBridge(
    CardUnmaskPromptController* unmask_controller,
    UIViewController* base_view_controller);
}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_CREATE_CARD_UNMASK_PROMPT_VIEW_BRIDGE_H_
