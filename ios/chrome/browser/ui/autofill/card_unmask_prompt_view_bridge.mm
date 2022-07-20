// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/card_unmask_prompt_view_bridge.h"

#import "base/notreached.h"
#import "components/autofill/core/browser/ui/payments/card_unmask_prompt_controller.h"
#import "ios/chrome/browser/ui/autofill/card_unmask_prompt_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace autofill {

#pragma mark CardUnmaskPromptViewBridge

CardUnmaskPromptViewBridge::CardUnmaskPromptViewBridge(
    CardUnmaskPromptController* controller,
    UIViewController* base_view_controller)
    : controller_(controller),
      base_view_controller_(base_view_controller),
      weak_ptr_factory_(this) {
  DCHECK(controller_);
}

CardUnmaskPromptViewBridge::~CardUnmaskPromptViewBridge() {
  if (controller_) {
    controller_->OnUnmaskDialogClosed();
  }
}

void CardUnmaskPromptViewBridge::Show() {
  view_controller_ =
      [[CardUnmaskPromptViewController alloc] initWithBridge:this];

  UINavigationController* navigation_controller =
      [[UINavigationController alloc]
          initWithRootViewController:view_controller_];
  [navigation_controller
      setModalPresentationStyle:UIModalPresentationFormSheet];
  [navigation_controller
      setModalTransitionStyle:UIModalTransitionStyleCoverVertical];
  // If this prompt is swiped away, it cannot be opened again. Work
  // around this bug by preventing swipe-to-dismiss.
  // TODO(crbug.com/1346060)
  [navigation_controller setModalInPresentation:YES];

  [base_view_controller_ presentViewController:navigation_controller
                                      animated:YES
                                    completion:nil];
}

void CardUnmaskPromptViewBridge::ControllerGone() {
  controller_ = nullptr;
  PerformClose();
}

void CardUnmaskPromptViewBridge::DisableAndWaitForVerification() {
  NOTIMPLEMENTED();
}

void CardUnmaskPromptViewBridge::GotVerificationResult(
    const std::u16string& error_message,
    bool allow_retry) {
  NOTIMPLEMENTED();
}

CardUnmaskPromptController* CardUnmaskPromptViewBridge::GetController() {
  return controller_;
}

void CardUnmaskPromptViewBridge::PerformClose() {
  base::WeakPtr<CardUnmaskPromptViewBridge> weak_this =
      weak_ptr_factory_.GetWeakPtr();
  [view_controller_.navigationController
      dismissViewControllerAnimated:YES
                         completion:^{
                           if (weak_this) {
                             weak_this->DeleteSelf();
                           }
                         }];
}

void CardUnmaskPromptViewBridge::DeleteSelf() {
  delete this;
}

}  // namespace autofill
