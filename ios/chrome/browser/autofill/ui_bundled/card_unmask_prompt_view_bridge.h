// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_CARD_UNMASK_PROMPT_VIEW_BRIDGE_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_CARD_UNMASK_PROMPT_VIEW_BRIDGE_H_

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/autofill/core/browser/ui/payments/card_unmask_prompt_view.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"

@class CardUnmaskPromptViewController;
@class UIViewController;
@class UINavigationController;
@class CreditCardData;
@class UIImage;

namespace autofill {

class CardUnmaskPromptController;
class PersonalDataManager;

// iOS implementation of the unmask prompt UI.
class CardUnmaskPromptViewBridge : public CardUnmaskPromptView {
 public:
  CardUnmaskPromptViewBridge(
      CardUnmaskPromptController* controller,
      UINavigationController* navigation_controller,
      PersonalDataManager* personal_data_manager,
      id<BrowserCoordinatorCommands> browser_coordinator_commands_handler);
  CardUnmaskPromptViewBridge(const CardUnmaskPromptViewBridge&) = delete;
  CardUnmaskPromptViewBridge& operator=(const CardUnmaskPromptViewBridge&) =
      delete;

  ~CardUnmaskPromptViewBridge() override;

  // CardUnmaskPromptView:
  void Show() override;
  void Dismiss() override;
  void ControllerGone() override;
  void DisableAndWaitForVerification() override;
  void GotVerificationResult(const std::u16string& error_message,
                             bool allow_retry) override;

  CardUnmaskPromptController* GetController();

  // Closes the view.
  virtual void PerformClose();

  CreditCardData* credit_card_data() { return credit_card_data_; }

 protected:
  // The controller `this` queries for logic and state.
  raw_ptr<CardUnmaskPromptController> controller_;  // weak

  // The presented UINavigationController containing `prompt_view_controller_`.
  UINavigationController* navigation_controller_;

  // Created on `Show` and destroyed when 'this' is destroyed.
  CardUnmaskPromptViewController* prompt_view_controller_;

 private:
  UIImage* GetCardIcon();

  raw_ptr<PersonalDataManager> personal_data_manager_;

  CreditCardData* credit_card_data_;

  __weak id<BrowserCoordinatorCommands> browser_coordinator_commands_handler_;

  base::WeakPtrFactory<CardUnmaskPromptViewBridge> weak_ptr_factory_;
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_CARD_UNMASK_PROMPT_VIEW_BRIDGE_H_
