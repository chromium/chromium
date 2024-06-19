// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_OTP_INPUT_DIALOG_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_OTP_INPUT_DIALOG_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "base/memory/weak_ptr.h"
#import "components/autofill/core/browser/ui/payments/card_unmask_otp_input_dialog_view.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/otp_input_dialog_mutator_bridge_target.h"

@protocol OtpInputDialogConsumer;
@protocol OtpInputDialogMutator;
@protocol OtpInputDialogMediatorDelegate;

@class OtpInputDialogMutatorBridge;

namespace autofill {
class CardUnmaskOtpInputDialogControllerImpl;
}  // namespace autofill

// Bridge class used to connect Autofill OTP input dialog components with the
// IOS view implementation.
class OtpInputDialogMediator : public autofill::CardUnmaskOtpInputDialogView,
                               public OtpInputDialogMutatorBridgeTarget {
 public:
  OtpInputDialogMediator(
      base::WeakPtr<autofill::CardUnmaskOtpInputDialogControllerImpl>
          model_controller,
      id<OtpInputDialogMediatorDelegate> delegate);
  OtpInputDialogMediator(const OtpInputDialogMediator&) = delete;
  OtpInputDialogMediator& operator=(const OtpInputDialogMediator&) = delete;
  ~OtpInputDialogMediator() override;

  // CardUnmaskOtpInputDialogView:
  void ShowPendingState() override;
  void ShowInvalidState(const std::u16string& invalid_label_text) override;
  void Dismiss(bool show_confirmation_before_closing,
               bool user_closed_dialog) override;
  base::WeakPtr<CardUnmaskOtpInputDialogView> GetWeakPtr() override;

  // OtpInputDialogMutatorTarget:
  void DidTapConfirmButton(const std::u16string& input_value) override;
  void DidTapCancelButton() override;
  void OnOtpInputChanges(const std::u16string& input_value) override;
  void DidTapNewCodeLink() override;

  void SetConsumer(id<OtpInputDialogConsumer> consumer);

  // Returns an implementation of the mutator that forwards to this mediator.
  // We need this bridge since this mediator is C++ whereas the ViewController
  // expects the Objective-C protocol.
  id<OtpInputDialogMutator> AsMutator();

 private:
  // The model to provide data to be shown in the IOS view implementation.
  base::WeakPtr<autofill::CardUnmaskOtpInputDialogControllerImpl>
      model_controller_;

  __weak id<OtpInputDialogConsumer> consumer_;

  __weak id<OtpInputDialogMediatorDelegate> delegate_;

  OtpInputDialogMutatorBridge* mutator_bridge_;

  base::WeakPtrFactory<OtpInputDialogMediator> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_OTP_INPUT_DIALOG_MEDIATOR_H_
