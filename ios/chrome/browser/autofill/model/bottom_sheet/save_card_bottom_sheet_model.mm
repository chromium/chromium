// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/bottom_sheet/save_card_bottom_sheet_model.h"

#import <utility>

#import "base/check.h"
#import "components/autofill/core/browser/payments/payments_autofill_client.h"

namespace autofill {

SaveCardBottomSheetModel::Observer::~Observer() = default;

SaveCardBottomSheetModel::SaveCardBottomSheetModel(
    AutofillSaveCardUiInfo ui_info,
    std::unique_ptr<AutofillSaveCardDelegate> save_card_delegate)
    : ui_info_(std::move(ui_info)),
      save_card_delegate_(std::move(save_card_delegate)) {}

SaveCardBottomSheetModel::~SaveCardBottomSheetModel() {
  // Model is destroyed on bottomsheet's dismissal. Presence of
  // `on_confirmation_closed_callback_` indicates that the bottomsheet has been
  // dismissed after getting save card confirmation. Thus the callback should be
  // executed upon dismissing the bottomsheet.
  if (on_confirmation_closed_callback_) {
    std::move(on_confirmation_closed_callback_).Run();
  }
}

void SaveCardBottomSheetModel::OnAccepted() {
  // For local save, there is no save in progress state since card is directly
  // saved to the device.
  save_card_state_ = save_card_delegate()->is_for_upload()
                         ? SaveCardState::kSaveInProgress
                         : SaveCardState::kSaved;
  save_card_delegate()->OnUiAccepted();
}

void SaveCardBottomSheetModel::OnCanceled() {
  // This method is called when bottomsheet is dismissed using the `No thanks`
  // button which is enabled only when save card is offered. Once bottomsheet
  // has been accepted, the button is disabled while showing loading or
  // confirmation.
  CHECK(save_card_state_ == SaveCardState::kOffered);
  save_card_delegate()->OnUiCanceled();
}

void SaveCardBottomSheetModel::CreditCardUploadCompleted(
    bool card_saved,
    autofill::payments::PaymentsAutofillClient::OnConfirmationClosedCallback
        on_confirmation_closed_callback) {
  save_card_state_ =
      card_saved ? SaveCardState::kSaved : SaveCardState::kFailed;
  on_confirmation_closed_callback_ = std::move(on_confirmation_closed_callback);
  observer_list_.Notify(&Observer::OnCreditCardUploadCompleted, card_saved);
}

}  // namespace autofill
