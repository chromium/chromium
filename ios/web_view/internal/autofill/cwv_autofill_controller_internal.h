// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_AUTOFILL_CONTROLLER_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_AUTOFILL_CONTROLLER_INTERNAL_H_

#include <memory>
#include <string>

#include "components/autofill/ios/browser/autofill_driver_ios_bridge.h"
#import "components/autofill/ios/form_util/form_activity_observer_bridge.h"
#import "components/password_manager/ios/shared_password_controller.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ios/web_view/internal/autofill/cwv_autofill_client_ios_bridge.h"
#import "ios/web_view/internal/passwords/web_view_password_manager_client.h"
#import "ios/web_view/public/cwv_autofill_controller.h"

NS_ASSUME_NONNULL_BEGIN

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

@interface CWVAutofillController () <AutofillDriverIOSBridge,
                                     CRWWebStateObserver,
                                     CWVAutofillClientIOSBridge,
                                     FormActivityObserver,
                                     PasswordManagerClientBridge,
                                     SharedPasswordControllerDelegate>

- (instancetype)
         initWithWebState:(web::WebState*)webState
           autofillClient:(std::unique_ptr<autofill::WebViewAutofillClientIOS>)
                              autofillClient
            autofillAgent:(AutofillAgent*)autofillAgent
          passwordManager:(std::unique_ptr<password_manager::PasswordManager>)
                              passwordManager
    passwordManagerClient:
        (std::unique_ptr<ios_web_view::WebViewPasswordManagerClient>)
            passwordManagerClient
       passwordController:(SharedPasswordController*)passwordController
        applicationLocale:(const std::string&)applicationLocale
    NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_AUTOFILL_CONTROLLER_INTERNAL_H_
