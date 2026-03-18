// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/error_dialog/coordinator/autofill_ai_error_dialog_mediator.h"

#import "base/check.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/autofill_ai/error_dialog/coordinator/autofill_ai_error_dialog_mediator_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

AutofillAiErrorDialogMediator::AutofillAiErrorDialogMediator(
    autofill::AutofillAiErrorDialogContext error_context,
    id<AutofillAiErrorDialogMediatorDelegate> delegate)
    : error_context_(std::move(error_context)), delegate_(delegate) {
  CHECK(delegate_);
}

AutofillAiErrorDialogMediator::~AutofillAiErrorDialogMediator() = default;

void AutofillAiErrorDialogMediator::Show() {
  NSString* title;
  NSString* message;
  NSString* buttonLabel = l10n_util::GetNSString(
      IDS_AUTOFILL_AI_LOCAL_SAVE_NOTIFICATION_DISMISS_BUTTON_LABEL);

  switch (error_context_.type) {
    case autofill::AutofillAiErrorDialogType::kTypeLocalSave:
      title =
          l10n_util::GetNSString(IDS_AUTOFILL_AI_LOCAL_SAVE_NOTIFICATION_TITLE);
      message = l10n_util::GetNSString(
          IDS_AUTOFILL_AI_LOCAL_SAVE_NOTIFICATION_DESCRIPTION);
      break;
    case autofill::AutofillAiErrorDialogType::kTypeSaveToWalletFailure:
      title = l10n_util::GetNSString(
          IDS_AUTOFILL_AI_WALLET_UPDATE_OR_MIGRATE_FAILURE_NOTIFICATION);
      message =
          l10n_util::GetNSString(IDS_IOS_AUTOFILL_AI_WALLET_CONNECTION_FAILURE);
      break;
    case autofill::AutofillAiErrorDialogType::kTypeFetchFromWalletFailure:
      title = l10n_util::GetNSString(
          IDS_IOS_AUTOFILL_AI_WALLET_FETCH_FAILURE_TITLE);
      message =
          l10n_util::GetNSString(IDS_IOS_AUTOFILL_AI_WALLET_CONNECTION_FAILURE);
      break;
  }

  [delegate_ showErrorDialog:title message:message buttonLabel:buttonLabel];
}
