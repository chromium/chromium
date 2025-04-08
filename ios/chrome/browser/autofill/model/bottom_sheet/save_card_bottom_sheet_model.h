// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_BOTTOM_SHEET_SAVE_CARD_BOTTOM_SHEET_MODEL_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_BOTTOM_SHEET_SAVE_CARD_BOTTOM_SHEET_MODEL_H_

#import <memory>
#import <string>

#import "base/memory/weak_ptr.h"
#import "components/autofill/core/browser/payments/autofill_save_card_delegate.h"
#import "components/autofill/core/browser/payments/autofill_save_card_ui_info.h"

namespace autofill {

// Model layer component for save card bottomsheet. This model is composed of
// AutofillSaveCardUiInfo (holds resources for save card bottomsheet) and
// AutofillSaveCardDelegate (provides callbacks to handle user interactions with
// the bottomsheet).
class SaveCardBottomSheetModel {
 public:
  SaveCardBottomSheetModel(
      AutofillSaveCardUiInfo ui_info,
      std::unique_ptr<AutofillSaveCardDelegate> save_card_delegate);

  SaveCardBottomSheetModel(SaveCardBottomSheetModel&) = delete;
  SaveCardBottomSheetModel& operator=(SaveCardBottomSheetModel&) = delete;
  virtual ~SaveCardBottomSheetModel();

  // Calls `AutofillSaveCardDelegate` to handle the accept event.
  virtual void OnAccepted();

  // Calls `AutofillSaveCardDelegate` to handle the dismiss event.
  virtual void OnCanceled();

  // Logo to be displayed above the title in the bottomsheet.
  inline int logo_icon_id() const { return ui_info_.logo_icon_id; }

  // Accessibility description for the logo.
  inline const std::u16string& logo_icon_description() const {
    return ui_info_.logo_icon_description;
  }

  // Title displayed in the bottomsheet.
  inline const std::u16string& title() const { return ui_info_.title_text; }

  // Explanatory text displayed below the title in the bottomsheet.
  inline const std::u16string& subtitle() const {
    return ui_info_.description_text;
  }

  // Text for the button to accept saving the card in the bottomsheet.
  inline const std::u16string& accept_button_text() const {
    return ui_info_.confirm_text;
  }

  // Text for the button to dismiss the bottomsheet.
  inline const std::u16string& cancel_button_text() const {
    return ui_info_.cancel_text;
  }

  base::WeakPtr<SaveCardBottomSheetModel> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  AutofillSaveCardDelegate* save_card_delegate() {
    return save_card_delegate_.get();
  }

  // Holds resources for the save card UI.
  const AutofillSaveCardUiInfo ui_info_;

  // Provides callbacks to handle user interactions with the UI.
  std::unique_ptr<AutofillSaveCardDelegate> save_card_delegate_;

  base::WeakPtrFactory<SaveCardBottomSheetModel> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_BOTTOM_SHEET_SAVE_CARD_BOTTOM_SHEET_MODEL_H_
