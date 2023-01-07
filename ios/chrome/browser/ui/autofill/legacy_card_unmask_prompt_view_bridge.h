// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_LEGACY_CARD_UNMASK_PROMPT_VIEW_BRIDGE_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_LEGACY_CARD_UNMASK_PROMPT_VIEW_BRIDGE_H_

#import <string>

#import <UIKit/UIKit.h>

#import "base/memory/weak_ptr.h"
#import "components/autofill/core/browser/ui/payments/card_unmask_prompt_view.h"

extern NSString* const kCardUnmaskPromptCollectionViewAccessibilityID;

@class LegacyCardUnmaskPromptViewController;
@class UIViewController;

namespace autofill {

class CardUnmaskPromptController;

// iOS implementation of the unmask prompt UI.
// A new implementation is on the works and will replace this one.
// Target Milestone: 107
class LegacyCardUnmaskPromptViewBridge : public CardUnmaskPromptView {
 public:
  // `base_view_controller` is a weak reference to the view controller used to
  // present UI.
  LegacyCardUnmaskPromptViewBridge(CardUnmaskPromptController* controller,
                                   UIViewController* base_view_controller);

  LegacyCardUnmaskPromptViewBridge(const LegacyCardUnmaskPromptViewBridge&) =
      delete;
  LegacyCardUnmaskPromptViewBridge& operator=(
      const LegacyCardUnmaskPromptViewBridge&) = delete;

  ~LegacyCardUnmaskPromptViewBridge() override;

  // CardUnmaskPromptView:
  void Show() override;
  void ControllerGone() override;
  void DisableAndWaitForVerification() override;
  void GotVerificationResult(const std::u16string& error_message,
                             bool allow_retry) override;

  CardUnmaskPromptController* GetController();

  // Closes the view.
  void PerformClose();

  // Called when `view_controller` was dismissed.
  // This call destroys `this`.
  void NavigationControllerDismissed();

 protected:
  // The presented UINavigationController.
  UINavigationController* view_controller_;

  // The view controller with the unmask prompt UI.
  LegacyCardUnmaskPromptViewController* card_view_controller_;

 private:
  // Deletes self. Called after LegacyCardUnmaskPromptViewController finishes
  // dismissing its own UI elements.
  void DeleteSelf();

  // The controller `this` queries for logic and state.
  CardUnmaskPromptController* controller_;  // weak

  // Weak reference to the view controller used to present UI.
  __weak UIViewController* base_view_controller_;

  base::WeakPtrFactory<LegacyCardUnmaskPromptViewBridge> weak_ptr_factory_;
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_LEGACY_CARD_UNMASK_PROMPT_VIEW_BRIDGE_H_
