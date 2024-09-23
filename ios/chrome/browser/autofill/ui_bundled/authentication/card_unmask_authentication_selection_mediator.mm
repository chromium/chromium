// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/authentication/card_unmask_authentication_selection_mediator.h"

#import "base/memory/weak_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/ui/payments/card_unmask_authentication_selection_dialog_controller_impl.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/card_unmask_authentication_selection_mediator_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/card_unmask_authentication_selection_mutator_bridge.h"

namespace {
autofill::CardUnmaskAuthenticationSelectionDialog*
ReturnMediatorIgnoreControllerArg(
    CardUnmaskAuthenticationSelectionMediator* mediator,
    autofill::CardUnmaskAuthenticationSelectionDialogController*
        _unused_controller) {
  return mediator;
}
}  // namespace

CardUnmaskAuthenticationSelectionMediator::
    CardUnmaskAuthenticationSelectionMediator(
        base::WeakPtr<
            autofill::CardUnmaskAuthenticationSelectionDialogControllerImpl>
            model_controller,
        id<CardUnmaskAuthenticationSelectionConsumer> consumer)
    : model_controller_(model_controller), consumer_(consumer) {
  mutator_bridge_ = [[CardUnmaskAuthenticationSelectionMutatorBridge alloc]
      initWithTarget:weak_ptr_factory_.GetWeakPtr()];
  model_controller_->ShowDialog(
      base::BindOnce(&ReturnMediatorIgnoreControllerArg, this));
  [consumer_ setHeaderTitle:base::SysUTF16ToNSString(
                                model_controller_->GetWindowTitle())];
  [consumer_ setHeaderText:base::SysUTF16ToNSString(
                               model_controller_->GetContentHeaderText())];
  [consumer_ setCardUnmaskOptions:ConvertChallengeOptions()];
  [consumer_ setFooterText:base::SysUTF16ToNSString(
                               model_controller_->GetContentFooterText())];
  [consumer_
      setChallengeAcceptanceLabel:base::SysUTF16ToNSString(
                                      model_controller_->GetOkButtonLabel())];
}

CardUnmaskAuthenticationSelectionMediator::
    ~CardUnmaskAuthenticationSelectionMediator() {
  if (!was_dismissed_ && model_controller_) {
    // Our coordinator is stopping (e.g. closing Chromium, or view has been
    // swiped away). We must call OnDialogClosed on the model_controller_ before
    // this mediator is destroyed.
    model_controller_->OnDialogClosed(
        /*user_closed_dialog=*/true, /*server_success=*/false);
  }
}

// Implementation of CardUnmaskAuthenticationSelectionMutatorBridgeTarget
// follows:

void CardUnmaskAuthenticationSelectionMediator::DidSelectChallengeOption(
    CardUnmaskChallengeOptionIOS* option) {
  model_controller_->SetSelectedChallengeOptionId(option.id);
  // The done button label changes in response.
  [consumer_
      setChallengeAcceptanceLabel:base::SysUTF16ToNSString(
                                      model_controller_->GetOkButtonLabel())];
}

void CardUnmaskAuthenticationSelectionMediator::DidAcceptSelection() {
  model_controller_->OnOkButtonClicked();
}

void CardUnmaskAuthenticationSelectionMediator::DidCancelSelection() {
  Dismiss(/*user_closed_dialog=*/true, /*server_success=*/false);
}

// Implemention of autofill::CardUnmaskAuthenticationSelectionDialog follows:

void CardUnmaskAuthenticationSelectionMediator::Dismiss(bool user_closed_dialog,
                                                        bool server_success) {
  DCHECK(!was_dismissed_);
  was_dismissed_ = true;
  model_controller_->OnDialogClosed(user_closed_dialog, server_success);
  // If the user dismissed the authentication selection, then the dismissal
  // needs to be delegated up through our delegate (a coordinator).
  // If Dismiss() was called after a response from the server, then we expect
  // another prompt to be initiated without dismissal of the overall flow.
  if (user_closed_dialog) {
    [delegate_ dismissAuthenticationSelection];
  }
}

void CardUnmaskAuthenticationSelectionMediator::UpdateContent() {
  [consumer_ enterPendingState];
}

id<CardUnmaskAuthenticationSelectionMutator>
CardUnmaskAuthenticationSelectionMediator::AsMutator() {
  return mutator_bridge_;
}

// TODO(crbug.com/40282545): Once the ViewController is implemented, handle
// cancelling and pushing ok by implementing the view controller's mutator.

NSArray<CardUnmaskChallengeOptionIOS*>*
CardUnmaskAuthenticationSelectionMediator::ConvertChallengeOptions() {
  const std::vector<autofill::CardUnmaskChallengeOption>& autofill_options =
      model_controller_->GetChallengeOptions();
  NSMutableArray* ios_options =
      [NSMutableArray arrayWithCapacity:autofill_options.size()];
  for (auto option : autofill_options) {
    [ios_options
        addObject:[CardUnmaskChallengeOptionIOS
                      convertFrom:option
                        modeLabel:model_controller_->GetAuthenticationModeLabel(
                                      option)]];
  }
  return ios_options;
}
