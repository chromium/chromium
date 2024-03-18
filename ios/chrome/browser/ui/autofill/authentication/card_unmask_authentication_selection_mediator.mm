// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/authentication/card_unmask_authentication_selection_mediator.h"

#import "base/memory/weak_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/ui/payments/card_unmask_authentication_selection_dialog_controller_impl.h"

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
  model_controller_->ShowDialog(
      base::BindOnce(&ReturnMediatorIgnoreControllerArg, this));
  [consumer_ setHeaderTitle:base::SysUTF16ToNSString(
                                model_controller_->GetWindowTitle())];
  [consumer_ setHeaderText:base::SysUTF16ToNSString(
                               model_controller_->GetContentHeaderText())];
  [consumer_ setCardUnmaskOptions:ConvertChallengeOptions()];
  [consumer_ setFooterText:base::SysUTF16ToNSString(
                               model_controller_->GetContentFooterText())];
}

CardUnmaskAuthenticationSelectionMediator::
    ~CardUnmaskAuthenticationSelectionMediator() = default;

void CardUnmaskAuthenticationSelectionMediator::DidSelectChallengeOption(
    CardUnmaskChallengeOptionIOS* option) {
  model_controller_->SetSelectedChallengeOptionId(option.id);
  // The done button label changes in response.
  [consumer_
      setChallengeAcceptanceLabel:base::SysUTF16ToNSString(
                                      model_controller_->GetOkButtonLabel())];
}

// Implemention of autofill::CardUnmaskAuthenticationSelectionDialog follows:

void CardUnmaskAuthenticationSelectionMediator::Dismiss(bool user_closed_dialog,
                                                        bool server_success) {
  // TODO(crbug.com/40282545): Implement dismissal of the authentication
  // selection view by delegating to the coordinator.
  model_controller_->OnDialogClosed(user_closed_dialog, server_success);
}

void CardUnmaskAuthenticationSelectionMediator::UpdateContent() {
  [consumer_ enterPendingState];
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
