// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_BOTTOM_SHEET_SAVE_CARD_BOTTOM_SHEET_MODEL_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_BOTTOM_SHEET_SAVE_CARD_BOTTOM_SHEET_MODEL_H_

#import <base/observer_list.h>
#import <base/observer_list_types.h>

#import <memory>
#import <string>

#import "base/memory/weak_ptr.h"
#import "components/autofill/core/browser/payments/autofill_save_card_delegate.h"
#import "components/autofill/core/browser/payments/autofill_save_card_ui_info.h"
#import "components/autofill/core/browser/payments/legal_message_line.h"
#import "components/autofill/core/browser/payments/payments_autofill_client.h"

namespace autofill {

// Model layer component for save card bottomsheet. This model is composed of
// AutofillSaveCardUiInfo (holds resources for save card bottomsheet) and
// AutofillSaveCardDelegate (provides callbacks to handle user interactions with
// the bottomsheet).
class SaveCardBottomSheetModel {
 public:
  // Save card flow states.
  enum class SaveCardState {
    // Save card is offered to the user.
    kOffered,
    // User accepted the bottomsheet and save card is in progress.
    kSaveInProgress,
    // Save card has succeeded.
    kSaved,
    // Save card has failed.
    kFailed,
    kMaxValue = kFailed,
  };

  // Interface for observer of this model.
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override;
    // Called when card upload request is completed.
    virtual void OnCreditCardUploadCompleted(bool card_saved) = 0;
  };

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

  // Updates observer with save card result and sets the callback to be run
  // after bottomsheet is closed upon showing the success confirmation.
  virtual void CreditCardUploadCompleted(
      bool card_saved,
      autofill::payments::PaymentsAutofillClient::OnConfirmationClosedCallback
          on_confirmation_closed_callback);

  void AddObserver(Observer* observer) { observer_list_.AddObserver(observer); }

  void RemoveObserver(Observer* observer) {
    observer_list_.RemoveObserver(observer);
  }

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

  // Card name and its last four digits displayed in the bottomsheet for the
  // card to be saved.
  inline const std::u16string& card_name_last_four_digits() const {
    return ui_info_.card_label;
  }

  // Expiry date displayed in the bottomsheet for the card to be saved.
  inline const std::u16string& card_expiry_date() const {
    return ui_info_.card_sub_label;
  }

  // Card's accessibility description with the card's nick name (if present),
  // card issuer name, last four digits and expiry date.
  inline const std::u16string& card_accessibility_description() const {
    return ui_info_.card_description;
  }

  // Accessibility description announced when loading is shown to indicate card
  // upload is in progress.
  inline const std::u16string& loading_accessibility_description() const {
    return ui_info_.loading_description;
  }

  // Card's issuer icon displayed in the bottomsheet for the card to be saved.
  inline int issuer_icon_id() const { return ui_info_.issuer_icon_id; }

  // The legal message with links displayed in the bottomsheet.
  inline const LegalMessageLines& legal_messages() const {
    return ui_info_.legal_message_lines;
  }

  inline SaveCardState save_card_state() const { return save_card_state_; }

  base::WeakPtr<SaveCardBottomSheetModel> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  AutofillSaveCardDelegate* save_card_delegate() {
    return save_card_delegate_.get();
  }

  void SetSaveCardStateForTesting(SaveCardState state) {
    save_card_state_ = state;
  }

 private:
  // Holds resources for the save card UI.
  const AutofillSaveCardUiInfo ui_info_;

  // Provides callbacks to handle user interactions with the UI.
  std::unique_ptr<AutofillSaveCardDelegate> save_card_delegate_;

  // Callback to be called after credit card upload is complete and bottomsheet
  // has been closed.
  payments::PaymentsAutofillClient::OnConfirmationClosedCallback
      on_confirmation_closed_callback_;

  SaveCardState save_card_state_{SaveCardState::kOffered};

  base::ObserverList<Observer> observer_list_;

  base::WeakPtrFactory<SaveCardBottomSheetModel> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_BOTTOM_SHEET_SAVE_CARD_BOTTOM_SHEET_MODEL_H_
