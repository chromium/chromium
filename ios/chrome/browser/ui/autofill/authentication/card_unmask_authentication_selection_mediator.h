// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_AUTHENTICATION_CARD_UNMASK_AUTHENTICATION_SELECTION_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_AUTHENTICATION_CARD_UNMASK_AUTHENTICATION_SELECTION_MEDIATOR_H_

#import "base/memory/weak_ptr.h"
#import "components/autofill/core/browser/ui/payments/card_unmask_authentication_selection_dialog.h"
#import "ios/chrome/browser/ui/autofill/authentication/card_unmask_authentication_selection_consumer.h"

namespace autofill {
class CardUnmaskAuthenticationSelectionDialogControllerImpl;
}  // namespace autofill

// CardUnmaskAuthenticationSelectionMediator mediates between
// CardUnmaskAuthenticationSelectionDialogControllerImpl and view controllers
// via the CardUnmaskAuthenticationSelectionConsumer interface.
class CardUnmaskAuthenticationSelectionMediator
    : public autofill::CardUnmaskAuthenticationSelectionDialog {
 public:
  CardUnmaskAuthenticationSelectionMediator(
      base::WeakPtr<
          autofill::CardUnmaskAuthenticationSelectionDialogControllerImpl>
          model_controller,
      id<CardUnmaskAuthenticationSelectionConsumer> consumer);

  // Non-copyable.
  CardUnmaskAuthenticationSelectionMediator(
      CardUnmaskAuthenticationSelectionMediator&) = delete;
  const CardUnmaskAuthenticationSelectionMediator& operator=(
      CardUnmaskAuthenticationSelectionMediator&) = delete;

  virtual ~CardUnmaskAuthenticationSelectionMediator();

  // Handles selecting a challenge option.
  void DidSelectChallengeOption(CardUnmaskChallengeOptionIOS* option);

  // autofill::CardUnmaskAuthenticationSelectionDialog
  void Dismiss(bool user_closed_dialog, bool server_success) override;
  void UpdateContent() override;

 private:
  base::WeakPtr<autofill::CardUnmaskAuthenticationSelectionDialogControllerImpl>
      model_controller_;
  id<CardUnmaskAuthenticationSelectionConsumer> consumer_;

  // Converts the autofill challenge options to ios challenge options destined
  // for the CardUnmaskAuthenticationSelectionConsumer.
  NSArray<CardUnmaskChallengeOptionIOS*>* ConvertChallengeOptions();
};

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_AUTHENTICATION_CARD_UNMASK_AUTHENTICATION_SELECTION_MEDIATOR_H_
