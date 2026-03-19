// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_ERROR_DIALOG_MODEL_AUTOFILL_AI_ERROR_DIALOG_CONTEXT_H_
#define IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_ERROR_DIALOG_MODEL_AUTOFILL_AI_ERROR_DIALOG_CONTEXT_H_

#import <optional>
#import <string>

namespace autofill {

// The type of autofill ai error dialog that will be displayed.
enum class AutofillAiErrorDialogType {
  kTypeLocalSave = 0,
  kTypeSaveToWalletFailure = 1,
  kTypeFetchFromWalletFailure = 2,
};

// The context for the autofill ai error dialog.
struct AutofillAiErrorDialogContext {
  AutofillAiErrorDialogContext() = default;
  AutofillAiErrorDialogContext(const AutofillAiErrorDialogContext& other) =
      default;
  AutofillAiErrorDialogContext(AutofillAiErrorDialogContext&& other) = default;
  AutofillAiErrorDialogContext& operator=(const AutofillAiErrorDialogContext&) =
      delete;
  AutofillAiErrorDialogContext& operator=(AutofillAiErrorDialogContext&&) =
      delete;

  // The type of autofill ai error dialog that will be displayed.
  AutofillAiErrorDialogType type = AutofillAiErrorDialogType::kTypeLocalSave;
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_ERROR_DIALOG_MODEL_AUTOFILL_AI_ERROR_DIALOG_CONTEXT_H_
