// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_ERROR_DIALOG_AUTOFILL_ERROR_DIALOG_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_ERROR_DIALOG_AUTOFILL_ERROR_DIALOG_MEDIATOR_H_

#import "components/autofill/core/browser/ui/payments/autofill_error_dialog_view.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "base/memory/weak_ptr.h"

@protocol AutofillErrorDialogMediatorDelegate;

namespace autofill {
class AutofillErrorDialogController;
}  // namespace autofill

// Bridge class used to connect Autofill error dialog components with the IOS
// view implementation.
class AutofillErrorDialogMediator : public autofill::AutofillErrorDialogView {
 public:
  AutofillErrorDialogMediator(
      base::WeakPtr<autofill::AutofillErrorDialogController> model_controller,
      id<AutofillErrorDialogMediatorDelegate> delegate);

  AutofillErrorDialogMediator(const AutofillErrorDialogMediator&) = delete;
  AutofillErrorDialogMediator& operator=(const AutofillErrorDialogMediator&) =
      delete;

  ~AutofillErrorDialogMediator() override;

  // AutofillErrorDialogView:
  void Dismiss() override;
  base::WeakPtr<autofill::AutofillErrorDialogView> GetWeakPtr() override;

  // Config the error dialog and notify mediator delegate.
  base::WeakPtr<autofill::AutofillErrorDialogView> Show();

 private:
  // The model to provide data to be shown in the IOS view implementation.
  base::WeakPtr<autofill::AutofillErrorDialogController> model_controller_;

  __weak id<AutofillErrorDialogMediatorDelegate> delegate_;

  base::WeakPtrFactory<AutofillErrorDialogMediator> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_ERROR_DIALOG_AUTOFILL_ERROR_DIALOG_MEDIATOR_H_
