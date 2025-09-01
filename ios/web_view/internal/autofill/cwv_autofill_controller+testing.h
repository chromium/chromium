// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_AUTOFILL_CONTROLLER_TESTING_H_
#define IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_AUTOFILL_CONTROLLER_TESTING_H_

#import "ios/web_view/public/cwv_autofill_controller.h"

namespace autofill {
class WebViewAutofillClientIOS;
}  // namespace autofill

namespace ios_web_view {
class WebViewPasswordManagerClient;
}  // namespace ios_web_view

namespace password_manager {
class PasswordManager;
}  // namespace password_manager

namespace web {
class WebState;
}  // namespace web

@class AutofillAgent;
@class SharedPasswordController;

// Provides test-only initializers to allow injection of an alternate
// AutofillClient.
@interface CWVAutofillController (Testing)

- (instancetype)
         initWithWebState:(web::WebState*)webState
    autofillClientForTest:
        (std::unique_ptr<autofill::WebViewAutofillClientIOS>)autofillClient
            autofillAgent:(AutofillAgent*)autofillAgent
          passwordManager:(std::unique_ptr<password_manager::PasswordManager>)
                              passwordManager
    passwordManagerClient:
        (std::unique_ptr<ios_web_view::WebViewPasswordManagerClient>)
            passwordManagerClient
       passwordController:(SharedPasswordController*)passwordController;

@end

#endif  // IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_AUTOFILL_CONTROLLER_TESTING_H_
