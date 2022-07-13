// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_CARD_UNMASK_PROMPT_VIEW_BRIDGE_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_CARD_UNMASK_PROMPT_VIEW_BRIDGE_H_

#import "components/autofill/core/browser/ui/payments/card_unmask_prompt_view.h"

namespace autofill {
// iOS implementation of the unmask prompt UI.
class CardUnmaskPromptViewBridge : public CardUnmaskPromptView {
 public:
  CardUnmaskPromptViewBridge(const CardUnmaskPromptViewBridge&) = delete;
  CardUnmaskPromptViewBridge& operator=(const CardUnmaskPromptViewBridge&) =
      delete;

  ~CardUnmaskPromptViewBridge() override;

  // CardUnmaskPromptView:
  void Show() override;
  void ControllerGone() override;
  void DisableAndWaitForVerification() override;
  void GotVerificationResult(const std::u16string& error_message,
                             bool allow_retry) override;
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_CARD_UNMASK_PROMPT_VIEW_BRIDGE_H_
