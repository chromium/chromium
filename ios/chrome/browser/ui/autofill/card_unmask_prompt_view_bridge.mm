// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/card_unmask_prompt_view_bridge.h"

#import "base/apple/foundation_util.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/credit_card.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/core/browser/ui/payments/card_unmask_prompt_controller.h"
#import "ios/chrome/browser/autofill/model/credit_card/credit_card_data.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/autofill/card_unmask_prompt_view_controller.h"

namespace autofill {

#pragma mark CardUnmaskPromptViewBridge

CardUnmaskPromptViewBridge::CardUnmaskPromptViewBridge(
    CardUnmaskPromptController* controller,
    UIViewController* base_view_controller,
    PersonalDataManager* personal_data_manager)
    : controller_(controller),
      base_view_controller_(base_view_controller),
      personal_data_manager_(personal_data_manager),
      weak_ptr_factory_(this) {
  CHECK(controller_);
  CHECK(personal_data_manager_);
  credit_card_data_ =
      [[CreditCardData alloc] initWithCreditCard:controller_->GetCreditCard()
                                            icon:GetCardIcon()];
}

CardUnmaskPromptViewBridge::~CardUnmaskPromptViewBridge() {
  [prompt_view_controller_ disconnectFromBridge];
  if (controller_) {
    controller_->OnUnmaskDialogClosed();
  }
}

void CardUnmaskPromptViewBridge::Show() {
  prompt_view_controller_ =
      [[CardUnmaskPromptViewController alloc] initWithBridge:this];

  navigation_controller_ = [[UINavigationController alloc]
      initWithRootViewController:prompt_view_controller_];
  [navigation_controller_
      setModalPresentationStyle:UIModalPresentationFormSheet];
  [navigation_controller_
      setModalTransitionStyle:UIModalTransitionStyleCoverVertical];
  navigation_controller_.presentationController.delegate =
      prompt_view_controller_;

  [base_view_controller_ presentViewController:navigation_controller_
                                      animated:YES
                                    completion:nil];
}

void CardUnmaskPromptViewBridge::ControllerGone() {
  controller_ = nullptr;
  PerformClose();
}

void CardUnmaskPromptViewBridge::DisableAndWaitForVerification() {
  [prompt_view_controller_ showLoadingState];
}

void CardUnmaskPromptViewBridge::GotVerificationResult(
    const std::u16string& error_message,
    bool allow_retry) {
  // No error. Dismiss the prompt.
  if (error_message.empty()) {
    PerformClose();
  } else {
    [prompt_view_controller_
        showErrorAlertWithMessage:base::SysUTF16ToNSString(error_message)
                   closeOnDismiss:!allow_retry];
  }
}

CardUnmaskPromptController* CardUnmaskPromptViewBridge::GetController() {
  // This public accessor is used by the view controller, which shouldn't access
  // the controller after it was released. Adding a CHECK to explicitly catch
  // any UAF errors.
  CHECK(controller_);
  return controller_;
}

void CardUnmaskPromptViewBridge::PerformClose() {
  // Disconnect the vc from the bride, dismiss it and delete the bridge.
  [prompt_view_controller_ disconnectFromBridge];

  base::WeakPtr<CardUnmaskPromptViewBridge> weak_this =
      weak_ptr_factory_.GetWeakPtr();
  [navigation_controller_
      dismissViewControllerAnimated:YES
                         completion:^{
                           if (weak_this) {
                             weak_this->NavigationControllerDismissed();
                           }
                         }];
}

void CardUnmaskPromptViewBridge::NavigationControllerDismissed() {
  DeleteSelf();
}

void CardUnmaskPromptViewBridge::DeleteSelf() {
  delete this;
}

UIImage* CardUnmaskPromptViewBridge::GetCardIcon() {
  // Firstly check if card art image is available.
  const CreditCard& credit_card = GetController()->GetCreditCard();
  gfx::Image* image = personal_data_manager_->GetCreditCardArtImageForUrl(
      credit_card.card_art_url());
  if (image) {
    return image->ToUIImage();
  }

  // Use card network icon.
  Suggestion::Icon icon = credit_card.CardIconForAutofillSuggestion();
  return icon == Suggestion::Icon::kNoIcon
             ? nil
             : NativeImage(CreditCard::IconResourceId(
                   credit_card.CardIconForAutofillSuggestion()));
}

}  // namespace autofill
