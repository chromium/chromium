// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/progress_dialog/autofill_progress_dialog_coordinator.h"

#import <Foundation/Foundation.h>
#import <memory>

#import "components/autofill/core/browser/ui/payments/autofill_progress_dialog_controller_impl.h"
#import "ios/chrome/browser/autofill/model/autofill_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/ui/autofill/chrome_autofill_client_ios.h"
#import "ios/chrome/browser/ui/autofill/ios_chrome_payments_autofill_client.h"

@implementation AutofillProgressDialogCoordinator {
  // The model layer controller. This model controller provide access to model
  // data and also handles interactions.
  std::unique_ptr<autofill::AutofillProgressDialogControllerImpl>
      _modelController;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    autofill::ChromeAutofillClientIOS* client =
        AutofillTabHelper::FromWebState(
            browser->GetWebStateList()->GetActiveWebState())
            ->autofill_client();
    CHECK(client);
    auto* paymentsClient = client->GetPaymentsAutofillClient();
    CHECK(paymentsClient);
    _modelController = paymentsClient->GetProgressDialogModel();
  }
  return self;
}

@end
