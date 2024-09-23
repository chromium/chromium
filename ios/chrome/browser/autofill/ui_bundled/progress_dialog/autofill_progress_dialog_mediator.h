// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_PROGRESS_DIALOG_AUTOFILL_PROGRESS_DIALOG_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_PROGRESS_DIALOG_AUTOFILL_PROGRESS_DIALOG_MEDIATOR_H_

#import "components/autofill/core/browser/ui/payments/autofill_progress_dialog_view.h"

#import <Foundation/Foundation.h>

#import "base/memory/weak_ptr.h"

@protocol AlertConsumer;
@protocol AutofillProgressDialogMediatorDelegate;

namespace autofill {
class AutofillProgressDialogControllerImpl;
}  // namespace autofill

// Bridge class used to connect Autofill progress dialog components with the
// IOS view implementation.
class AutofillProgressDialogMediator
    : public autofill::AutofillProgressDialogView {
 public:
  AutofillProgressDialogMediator(
      base::WeakPtr<autofill::AutofillProgressDialogControllerImpl>
          model_controller,
      id<AutofillProgressDialogMediatorDelegate> delegate);
  AutofillProgressDialogMediator(const AutofillProgressDialogMediator&) =
      delete;
  AutofillProgressDialogMediator& operator=(
      const AutofillProgressDialogMediator&) = delete;
  ~AutofillProgressDialogMediator() override;

  // AutofillProgressDialogView:
  void Dismiss(bool show_confirmation_before_closing,
               bool is_canceled_by_user) override;
  void InvalidateControllerForCallbacks() override;
  base::WeakPtr<AutofillProgressDialogView> GetWeakPtr() override;

  void SetConsumer(id<AlertConsumer> consumer);

 private:
  void OnCancelButtonTapped();

  // The model to provide data to be shown in the IOS view implementation.
  base::WeakPtr<autofill::AutofillProgressDialogControllerImpl>
      model_controller_;

  __weak id<AlertConsumer> consumer_;

  __weak id<AutofillProgressDialogMediatorDelegate> delegate_;

  // Whether the dialog dismissal is invoked by user action.
  bool is_canceled_by_user_ = false;

  base::WeakPtrFactory<AutofillProgressDialogMediator> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_PROGRESS_DIALOG_AUTOFILL_PROGRESS_DIALOG_MEDIATOR_H_
