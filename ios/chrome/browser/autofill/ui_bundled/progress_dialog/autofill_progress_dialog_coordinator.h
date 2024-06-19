// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_PROGRESS_DIALOG_AUTOFILL_PROGRESS_DIALOG_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_PROGRESS_DIALOG_AUTOFILL_PROGRESS_DIALOG_COORDINATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/autofill/ui_bundled/progress_dialog/autofill_progress_dialog_mediator_delegate.h"

// The coordinator responsible for managing the autofill progress dialog.
// This dialog is shown to indicate some progress is ongoing in the background.
@interface AutofillProgressDialogCoordinator
    : ChromeCoordinator <AutofillProgressDialogMediatorDelegate>

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_PROGRESS_DIALOG_AUTOFILL_PROGRESS_DIALOG_COORDINATOR_H_
