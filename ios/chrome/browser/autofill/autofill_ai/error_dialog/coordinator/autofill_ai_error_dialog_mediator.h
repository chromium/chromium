// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_ERROR_DIALOG_COORDINATOR_AUTOFILL_AI_ERROR_DIALOG_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_ERROR_DIALOG_COORDINATOR_AUTOFILL_AI_ERROR_DIALOG_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/autofill/autofill_ai/error_dialog/model/autofill_ai_error_dialog_context.h"

@protocol AutofillAiErrorDialogMediatorDelegate;

// Class used to connect Autofill AI error dialog components with the IOS
// view implementation.
class AutofillAiErrorDialogMediator {
 public:
  AutofillAiErrorDialogMediator(
      autofill::AutofillAiErrorDialogContext error_context,
      id<AutofillAiErrorDialogMediatorDelegate> delegate);

  AutofillAiErrorDialogMediator(const AutofillAiErrorDialogMediator&) = delete;
  AutofillAiErrorDialogMediator& operator=(
      const AutofillAiErrorDialogMediator&) = delete;

  ~AutofillAiErrorDialogMediator();

  // Config the error dialog and notify mediator delegate.
  void Show();

 private:
  // The context containing data for the dialog.
  autofill::AutofillAiErrorDialogContext error_context_;

  __weak id<AutofillAiErrorDialogMediatorDelegate> delegate_;
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_ERROR_DIALOG_COORDINATOR_AUTOFILL_AI_ERROR_DIALOG_MEDIATOR_H_
