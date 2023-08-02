// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/password_manager_app_interface.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_store_consumer.h"
#import "components/password_manager/core/browser/password_store_interface.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/testing/nserror_util.h"
#import "ios/web/public/web_state.h"
#import "net/base/mac/url_conversions.h"
#import "url/gurl.h"

using base::test::ios::kWaitForActionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;
using password_manager::PasswordForm;
using password_manager::PasswordStoreInterface;
using password_manager::PasswordStoreConsumer;

class PasswordStoreConsumerHelper : public PasswordStoreConsumer {
 public:
  PasswordStoreConsumerHelper() {}

  PasswordStoreConsumerHelper(const PasswordStoreConsumerHelper&) = delete;
  PasswordStoreConsumerHelper& operator=(const PasswordStoreConsumerHelper&) =
      delete;

  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results) override {
    result_.swap(results);
  }

  std::vector<std::unique_ptr<PasswordForm>> WaitForResult() {
    bool unused = WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
      return result_.size() > 0;
    });
    (void)unused;
    return std::move(result_);
  }

  base::WeakPtr<PasswordStoreConsumer> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  std::vector<std::unique_ptr<PasswordForm>> result_;
  base::WeakPtrFactory<PasswordStoreConsumerHelper> weak_ptr_factory_{this};
};

@implementation PasswordManagerAppInterface

+ (NSError*)storeCredentialWithUsername:(NSString*)username
                               password:(NSString*)password
                                    URL:(NSURL*)URL {
  // Obtain a PasswordStore.
  scoped_refptr<password_manager::PasswordStoreInterface> passwordStore =
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
      net::GURLWithNSURL(URL).DeprecatedGetOriginAsURL();
  passwordCredentialForm.signon_realm = passwordCredentialForm.url.spec();
  passwordCredentialForm.scheme = password_manager::PasswordForm::Scheme::kHtml;
  passwordStore->AddLogin(passwordCredentialForm);

  return nil;
}

+ (NSError*)storeCredentialWithUsername:(NSString*)username
                               password:(NSString*)password {
  NSURL* URL = net::NSURLWithGURL(
      chrome_test_util::GetCurrentWebState()->GetLastCommittedURL());
  return [PasswordManagerAppInterface storeCredentialWithUsername:username
                                                         password:password
                                                              URL:URL];
}

+ (void)clearCredentials {
  scoped_refptr<password_manager::PasswordStoreInterface> passwordStore =
      IOSChromePasswordStoreFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState(),
          ServiceAccessType::IMPLICIT_ACCESS)
          .get();
  // Remove credentials stored during executing the test.
  passwordStore->RemoveLoginsCreatedBetween(base::Time(), base::Time::Now(),
                                            base::DoNothing());
}

+ (int)storedCredentialsCount {
  // Obtain a PasswordStore.
  scoped_refptr<PasswordStoreInterface> passwordStore =
      IOSChromePasswordStoreFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState(),
          ServiceAccessType::IMPLICIT_ACCESS)
          .get();

  PasswordStoreConsumerHelper consumer;
  passwordStore->GetAllLogins(consumer.GetWeakPtr());

  std::vector<std::unique_ptr<PasswordForm>> credentials =
      consumer.WaitForResult();

  return credentials.size();
}

+ (void)setAccountStorageNoticeShown:(BOOL)shown {
  chrome_test_util::GetOriginalBrowserState()->GetPrefs()->SetBoolean(
      password_manager::prefs::kAccountStorageNoticeShown, shown);
}

@end
