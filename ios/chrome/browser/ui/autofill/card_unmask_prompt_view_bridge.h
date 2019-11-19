// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_CARD_UNMASK_PROMPT_VIEW_BRIDGE_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_CARD_UNMASK_PROMPT_VIEW_BRIDGE_H_

#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_view.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller.h"

extern NSString* const kCardUnmaskPromptCollectionViewAccessibilityID;

@class CardUnmaskPromptViewController;
@class UIViewController;

namespace autofill {

class CardUnmaskPromptController;

// iOS implementation of the unmask prompt UI.
class CardUnmaskPromptViewBridge : public CardUnmaskPromptView {
 public:
  // |base_view_controller| is a weak reference to the view controller used to
  // present UI.
  CardUnmaskPromptViewBridge(CardUnmaskPromptController* controller,
                             UIViewController* base_view_controller);
  ~CardUnmaskPromptViewBridge() override;

  // CardUnmaskPromptView:
  void Show() override;
  void ControllerGone() override;
  void DisableAndWaitForVerification() override;
  void GotVerificationResult(const base::string16& error_message,
                             bool allow_retry) override;

  CardUnmaskPromptController* GetController();

  // Closes the view.
  void PerformClose();

  // Deletes self. This should only be called by CardUnmaskPromptViewController
  // after it finishes dismissing its own UI elements.
  void DeleteSelf();

 protected:
  CardUnmaskPromptViewController* view_controller_;

 private:
  // The controller |this| queries for logic and state.
  CardUnmaskPromptController* controller_;  // weak

  // Weak reference to the view controller used to present UI.
  __weak UIViewController* base_view_controller_;

  base::WeakPtrFactory<CardUnmaskPromptViewBridge> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CardUnmaskPromptViewBridge);
};

}  // namespace autofill

@interface CardUnmaskPromptViewController : CollectionViewController

// Designated initializer. |bridge| must not be null.
- (instancetype)initWithBridge:(autofill::CardUnmaskPromptViewBridge*)bridge
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithLayout:(UICollectionViewLayout*)layout
                         style:(CollectionViewControllerStyle)style
    NS_UNAVAILABLE;

// Shows the form that allows the user to input their CVC.
- (void)showCVCInputForm;

// Shows the form that allows the user to input their CVC along with the
// supplied error message.
- (void)showCVCInputFormWithError:(NSString*)errorMessage;

// Shows a progress spinner with a "verifying" message.
- (void)showSpinner;

// Shows a checkmark image and a "success" message.
- (void)showSuccess;

// Shows an error image and the provided message.
- (void)showError:(NSString*)errorMessage;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_CARD_UNMASK_PROMPT_VIEW_BRIDGE_H_
