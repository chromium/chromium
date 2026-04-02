// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_ERROR_DIALOG_MODEL_AUTOFILL_AI_ERROR_DIALOG_CONTEXT_H_
#define IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_ERROR_DIALOG_MODEL_AUTOFILL_AI_ERROR_DIALOG_CONTEXT_H_

#import <optional>
#import <string>

#import "base/functional/callback.h"

namespace autofill {

// The type of autofill ai error dialog that will be displayed.
enum class AutofillAiErrorDialogType {
  kTypeLocalSave = 0,
  kTypeSaveToWalletFailure = 1,
  kTypeFetchFromWalletFailure = 2,
};

// The context for the autofill ai error dialog.
struct AutofillAiErrorDialogContext {
  AutofillAiErrorDialogContext();

  AutofillAiErrorDialogContext(const AutofillAiErrorDialogContext& other) =
      delete;
  AutofillAiErrorDialogContext& operator=(
      const AutofillAiErrorDialogContext& other) = delete;
  AutofillAiErrorDialogContext(AutofillAiErrorDialogContext&& other);
  AutofillAiErrorDialogContext& operator=(AutofillAiErrorDialogContext&& other);

  ~AutofillAiErrorDialogContext();

  // The type of autofill ai error dialog that will be displayed.
  AutofillAiErrorDialogType type = AutofillAiErrorDialogType::kTypeLocalSave;

  // If true, the dialog bypasses the presentation queue and presents
  // immediately over any currently presented view controller.
  bool show_immediately = false;

  // Callback executed when the user dismisses the dialog.
  base::OnceClosure on_dismissed_callback;
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_ERROR_DIALOG_MODEL_AUTOFILL_AI_ERROR_DIALOG_CONTEXT_H_
