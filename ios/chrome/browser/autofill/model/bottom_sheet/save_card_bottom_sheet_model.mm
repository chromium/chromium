// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/bottom_sheet/save_card_bottom_sheet_model.h"

#import <utility>

#import "base/check.h"
#import "components/autofill/core/browser/payments/payments_autofill_client.h"
#import "components/autofill/ios/browser/credit_card_save_metrics_ios.h"

namespace autofill {

SaveCardBottomSheetModel::Observer::~Observer() = default;

SaveCardBottomSheetModel::SaveCardBottomSheetModel(
    AutofillSaveCardUiInfo ui_info,
    std::unique_ptr<AutofillSaveCardDelegate> save_card_delegate)
    : ui_info_(std::move(ui_info)),
      save_card_delegate_(std::move(save_card_delegate)) {
  CHECK(save_card_delegate_);
}

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
  if (save_card_state_ != SaveCardState::kOffered) {
    return;
  }
  save_card_state_ = SaveCardState::kFailed;
  save_card_delegate()->OnUiCanceled();
}

void SaveCardBottomSheetModel::OnIgnored() {
  if (save_card_state_ != SaveCardState::kOffered) {
    return;
  }
  save_card_state_ = SaveCardState::kFailed;
  const auto options = save_card_delegate()->GetSaveCreditCardOptions();
  save_card_delegate()->OnUiIgnored();
  // While `kAccepted` and `kDenied` are logged by `SaveCardBottomSheetMediator`
  // during active presentation, `kIgnored` must be logged here in the model.
  // When a user taps a legal disclosure link, the bottom sheet and its mediator
  // are deallocated while the model remains pending in the tab helper. If the
  // user subsequently closes or navigates the background tab, or if another
  // prompt preempts this offer,
  // `AutofillPendingPromptManager::ExpirePendingPromptAndDismissDialog()`
  // expires the model via `OnIgnored()`. Because the UI mediator is no longer
  // alive at that point, the model layer is responsible for logging `kIgnored`.
  autofill::autofill_metrics::LogSaveCreditCardPromptResultIOS(
      autofill::autofill_metrics::SaveCreditCardPromptResultIOS::kIgnored,
      is_for_upload(), options,
      autofill::autofill_metrics::SaveCreditCardPromptOverlayType::
          kBottomSheet);
}

void SaveCardBottomSheetModel::CreditCardUploadCompleted(
    bool card_saved,
    payments::PaymentsAutofillClient::OnConfirmationClosedCallback
        on_confirmation_closed_callback) {
  save_card_state_ =
      card_saved ? SaveCardState::kSaved : SaveCardState::kFailed;
  on_confirmation_closed_callback_ = std::move(on_confirmation_closed_callback);
  observer_list_.Notify(&Observer::OnCreditCardUploadCompleted, card_saved);
}

void SaveCardBottomSheetModel::OnUpdatedAndAcceptedForSaveAndFill(
    payments::PaymentsAutofillClient::UserProvidedCardSaveAndFillDetails
        details) {
  if (save_card_delegate()) {
    save_card_delegate()->OnUiUpdatedAndAcceptedForSaveAndFill(
        std::move(details));
  }
}

}  // namespace autofill
