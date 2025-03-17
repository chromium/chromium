// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/bottom_sheet/save_card_bottom_sheet_model.h"

#import <utility>

namespace autofill {

SaveCardBottomSheetModel::SaveCardBottomSheetModel(
    AutofillSaveCardUiInfo ui_info,
    std::unique_ptr<AutofillSaveCardDelegate> save_card_delegate)
    : ui_info_(std::move(ui_info)),
      save_card_delegate_(std::move(save_card_delegate)) {}

SaveCardBottomSheetModel::~SaveCardBottomSheetModel() = default;

void SaveCardBottomSheetModel::OnAccepted(std::u16string cardholder_name,
                                          std::u16string expiration_date_month,
                                          std::u16string expiration_date_year) {
  save_card_delegate()->OnUiUpdatedAndAccepted(
      payments::PaymentsAutofillClient::UserProvidedCardDetails{
          .cardholder_name = std::move(cardholder_name),
          .expiration_date_month = std::move(expiration_date_month),
          .expiration_date_year = std::move(expiration_date_year)});
}

void SaveCardBottomSheetModel::OnDismissed() {
  save_card_delegate()->OnUiCanceled();
}

}  // namespace autofill
