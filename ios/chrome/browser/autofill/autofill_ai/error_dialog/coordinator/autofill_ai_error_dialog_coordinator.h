// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_ERROR_DIALOG_COORDINATOR_AUTOFILL_AI_ERROR_DIALOG_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_ERROR_DIALOG_COORDINATOR_AUTOFILL_AI_ERROR_DIALOG_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol AutofillCommands;
class Browser;

namespace autofill {
struct AutofillAiErrorDialogContext;
}  // namespace autofill

// The coordinator responsible for managing the autofill ai error dialog. This
// dialog is shown when some error/alert state should be presented for autofill
// ai.
@interface AutofillAiErrorDialogCoordinator : ChromeCoordinator

// Handler for Autofill commands.
@property(nonatomic, weak) id<AutofillCommands> autofillCommandsHandler;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                              errorContext:
                                  (autofill::AutofillAiErrorDialogContext)
                                      errorContext NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_ERROR_DIALOG_COORDINATOR_AUTOFILL_AI_ERROR_DIALOG_COORDINATOR_H_
