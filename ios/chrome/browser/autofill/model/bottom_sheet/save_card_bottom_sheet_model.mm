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

void SaveCardBottomSheetModel::OnAccepted() {
  save_card_delegate()->OnUiAccepted();
}

void SaveCardBottomSheetModel::OnCanceled() {
  save_card_delegate()->OnUiCanceled();
}

}  // namespace autofill
