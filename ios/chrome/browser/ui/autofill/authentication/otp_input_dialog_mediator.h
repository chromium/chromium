// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_AUTHENTICATION_OTP_INPUT_DIALOG_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_AUTHENTICATION_OTP_INPUT_DIALOG_MEDIATOR_H_

#import "components/autofill/core/browser/ui/payments/card_unmask_otp_input_dialog_view.h"

#import <Foundation/Foundation.h>

#import "base/memory/weak_ptr.h"

@protocol OtpInputDialogConsumer;

namespace autofill {
class CardUnmaskOtpInputDialogControllerImpl;
}  // namespace autofill

// Bridge class used to connect Autofill OTP input dialog components with the
// IOS view implementation.
class OtpInputDialogMediator : public autofill::CardUnmaskOtpInputDialogView {
 public:
  explicit OtpInputDialogMediator(
      base::WeakPtr<autofill::CardUnmaskOtpInputDialogControllerImpl>
          model_controller);
  OtpInputDialogMediator(const OtpInputDialogMediator&) = delete;
  OtpInputDialogMediator& operator=(const OtpInputDialogMediator&) = delete;
  ~OtpInputDialogMediator() override;

  // CardUnmaskOtpInputDialogView:
  void ShowPendingState() override;
  void ShowInvalidState(const std::u16string& invalid_label_text) override;
  void Dismiss(bool show_confirmation_before_closing,
               bool user_closed_dialog) override;
  base::WeakPtr<CardUnmaskOtpInputDialogView> GetWeakPtr() override;

  void SetConsumer(id<OtpInputDialogConsumer> consumer);

 private:
  // The model to provide data to be shown in the IOS view implementation.
  base::WeakPtr<autofill::CardUnmaskOtpInputDialogControllerImpl>
      model_controller_;

  __weak id<OtpInputDialogConsumer> consumer_;

  base::WeakPtrFactory<OtpInputDialogMediator> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_AUTHENTICATION_OTP_INPUT_DIALOG_MEDIATOR_H_
