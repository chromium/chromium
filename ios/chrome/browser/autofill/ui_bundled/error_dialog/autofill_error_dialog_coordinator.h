// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_ERROR_DIALOG_AUTOFILL_ERROR_DIALOG_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_ERROR_DIALOG_AUTOFILL_ERROR_DIALOG_COORDINATOR_H_

#import <memory>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol AutofillCommands;

namespace autofill {
struct AutofillErrorDialogContext;
}  // namespace autofill

// The coordinator responsible for managing the autofill error dialog. This
// dialog is shown when some error/alert state should be presented.
@interface AutofillErrorDialogCoordinator : ChromeCoordinator

// Handler for Autofill commands.
@property(nonatomic, weak) id<AutofillCommands> autofillCommandsHandler;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                              errorContext:
                                  (autofill::AutofillErrorDialogContext)
                                      errorContext NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_ERROR_DIALOG_AUTOFILL_ERROR_DIALOG_COORDINATOR_H_
