// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_CARD_UNMASK_AUTHENTICATION_SELECTION_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_CARD_UNMASK_AUTHENTICATION_SELECTION_MEDIATOR_H_

#import "base/memory/weak_ptr.h"
#import "components/autofill/core/browser/ui/payments/card_unmask_authentication_selection_dialog.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/card_unmask_authentication_selection_consumer.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/card_unmask_authentication_selection_mediator_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/card_unmask_authentication_selection_mutator_bridge_target.h"

@protocol CardUnmaskAuthenticationSelectionMutator;
@class CardUnmaskAuthenticationSelectionMutatorBridge;

namespace autofill {
class CardUnmaskAuthenticationSelectionDialogControllerImpl;
}  // namespace autofill

// CardUnmaskAuthenticationSelectionMediator mediates between
// CardUnmaskAuthenticationSelectionDialogControllerImpl and view controllers
// via the CardUnmaskAuthenticationSelectionConsumer interface.
class CardUnmaskAuthenticationSelectionMediator
    : public autofill::CardUnmaskAuthenticationSelectionDialog,
      CardUnmaskAuthenticationSelectionMutatorBridgeTarget {
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

  // CardUnmaskAuthenticationSelectionMutator methods
  void DidSelectChallengeOption(CardUnmaskChallengeOptionIOS* option) override;
  void DidAcceptSelection() override;
  void DidCancelSelection() override;

  // autofill::CardUnmaskAuthenticationSelectionDialog
  void Dismiss(bool user_closed_dialog, bool server_success) override;
  void UpdateContent() override;

  // Set the delegate of this mediator.
  void set_delegate(
      id<CardUnmaskAuthenticationSelectionMediatorDelegate> delegate) {
    delegate_ = delegate;
  }

  // Returns an implementation of the mutator that forwards to this mediator.
  // We need this bridge since this mediator is C++ whereas the ViewController
  // expects the Objective-C protocol.
  id<CardUnmaskAuthenticationSelectionMutator> AsMutator();

 private:
  base::WeakPtr<autofill::CardUnmaskAuthenticationSelectionDialogControllerImpl>
      model_controller_;
  __weak id<CardUnmaskAuthenticationSelectionConsumer> consumer_;
  __weak id<CardUnmaskAuthenticationSelectionMediatorDelegate> delegate_;
  CardUnmaskAuthenticationSelectionMutatorBridge* mutator_bridge_;

  // Set to true the prompt has been dismissed.
  bool was_dismissed_ = false;

  base::WeakPtrFactory<CardUnmaskAuthenticationSelectionMutatorBridgeTarget>
      weak_ptr_factory_{this};

  // Converts the autofill challenge options to ios challenge options destined
  // for the CardUnmaskAuthenticationSelectionConsumer.
  NSArray<CardUnmaskChallengeOptionIOS*>* ConvertChallengeOptions();
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_CARD_UNMASK_AUTHENTICATION_SELECTION_MEDIATOR_H_
