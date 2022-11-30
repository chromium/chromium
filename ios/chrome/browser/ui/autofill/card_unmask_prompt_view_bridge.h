// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_CARD_UNMASK_PROMPT_VIEW_BRIDGE_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_CARD_UNMASK_PROMPT_VIEW_BRIDGE_H_

#import "base/memory/weak_ptr.h"
#import "components/autofill/core/browser/ui/payments/card_unmask_prompt_view.h"

@class CardUnmaskPromptViewController;
@class UIViewController;
@class UINavigationController;

namespace autofill {

class CardUnmaskPromptController;

// iOS implementation of the unmask prompt UI.
class CardUnmaskPromptViewBridge : public CardUnmaskPromptView {
 public:
  CardUnmaskPromptViewBridge(CardUnmaskPromptController* controller,
                             UIViewController* base_view_controller);
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

  CardUnmaskPromptController* GetController();

  // Closes the view.
  virtual void PerformClose();

  // Called when `navigation_controller_` was dismissed.
  // This call destroys `this`.
  void NavigationControllerDismissed();

 protected:
  // The presented UINavigationController containing `prompt_view_controller_`.
  UINavigationController* navigation_controller_;

  // Created on `Show` and destroyed when 'this' is destroyed.
  CardUnmaskPromptViewController* prompt_view_controller_;

  // The controller `this` queries for logic and state.
  CardUnmaskPromptController* controller_;  // weak

 private:
  // Deletes self. Called after CardUnmaskPromptViewController finishes
  // dismissing its own UI elements.
  void DeleteSelf();

  // Weak reference to the view controller used to present UI.
  __weak UIViewController* base_view_controller_;

  base::WeakPtrFactory<CardUnmaskPromptViewBridge> weak_ptr_factory_;
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_CARD_UNMASK_PROMPT_VIEW_BRIDGE_H_
