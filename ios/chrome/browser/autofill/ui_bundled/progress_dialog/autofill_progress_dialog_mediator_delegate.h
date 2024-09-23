// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_PROGRESS_DIALOG_AUTOFILL_PROGRESS_DIALOG_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_PROGRESS_DIALOG_AUTOFILL_PROGRESS_DIALOG_MEDIATOR_DELEGATE_H_

// Delegate class to handle user actions on the progress dialog.
@protocol AutofillProgressDialogMediatorDelegate <NSObject>

// Called when the progress dialog should be dismissed.
- (void)dismissDialog;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_PROGRESS_DIALOG_AUTOFILL_PROGRESS_DIALOG_MEDIATOR_DELEGATE_H_
