// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/password_manager_app_interface.h"

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/testing/nserror_util.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation PasswordManagerAppInterface

+ (NSError*)storeCredentialWithUsername:(NSString*)username
                               password:(NSString*)password {
  // Obtain a PasswordStore.
  scoped_refptr<password_manager::PasswordStore> passwordStore =
      IOSChromePasswordStoreFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState(),
          ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  if (passwordStore == nullptr) {
    return testing::NSErrorWithLocalizedDescription(
        @"PasswordStore is unexpectedly null for BrowserState");
  }

  // Store a PasswordForm representing a PasswordCredential.
  password_manager::PasswordForm passwordCredentialForm;
  passwordCredentialForm.username_value = base::SysNSStringToUTF16(username);
  passwordCredentialForm.password_value = base::SysNSStringToUTF16(password);
  passwordCredentialForm.url =
      chrome_test_util::GetCurrentWebState()->GetLastCommittedURL().GetOrigin();
  passwordCredentialForm.signon_realm = passwordCredentialForm.url.spec();
  passwordCredentialForm.scheme = password_manager::PasswordForm::Scheme::kHtml;
  passwordStore->AddLogin(passwordCredentialForm);

  return nil;
}

+ (void)clearCredentials {
  scoped_refptr<password_manager::PasswordStore> passwordStore =
      IOSChromePasswordStoreFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState(),
          ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  // Remove credentials stored during executing the test.
  passwordStore->RemoveLoginsCreatedBetween(base::Time(), base::Time::Now(),
                                            base::OnceClosure());
}

+ (void)getCredentialsInTabAtIndex:(int)index {
  // Get WebState for the original tab.
  web::WebState* webState =
      chrome_test_util::GetWebStateAtIndexInCurrentMode(index);

  // Execute JavaScript from inactive tab.
  webState->ExecuteJavaScript(
      base::UTF8ToUTF16("typeof navigator.credentials.get({password: true})"));
}

@end
