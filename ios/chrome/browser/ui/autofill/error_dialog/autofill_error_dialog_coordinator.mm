// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/error_dialog/autofill_error_dialog_coordinator.h"

#import <memory>

#import "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#import "components/autofill/core/browser/ui/payments/autofill_error_dialog_controller_impl.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"

@implementation AutofillErrorDialogCoordinator {
  // The model layer controller. This model controller provide access to model
  // data and also handles interactions.
  std::unique_ptr<autofill::AutofillErrorDialogControllerImpl> _modelController;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                              errorContext:
                                  (autofill::AutofillErrorDialogContext)
                                      errorContext {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _modelController =
        std::make_unique<autofill::AutofillErrorDialogControllerImpl>(
            std::move(errorContext));
  }
  return self;
}

@end
