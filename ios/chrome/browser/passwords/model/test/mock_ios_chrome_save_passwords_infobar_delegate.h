// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_TEST_MOCK_IOS_CHROME_SAVE_PASSWORDS_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_TEST_MOCK_IOS_CHROME_SAVE_PASSWORDS_INFOBAR_DELEGATE_H_

#import <Foundation/Foundation.h>

#import <memory>
#import <optional>

#import "components/password_manager/core/browser/password_form.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_save_password_infobar_delegate.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "url/gurl.h"

// Mock queue observer.
class MockIOSChromeSavePasswordInfoBarDelegate
    : public IOSChromeSavePasswordInfoBarDelegate {
 public:
  MockIOSChromeSavePasswordInfoBarDelegate(
      bool password_update,
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save);
  ~MockIOSChromeSavePasswordInfoBarDelegate() override;

  // Factory method that creates a mock save password delegate for pending
  // with credentials `username` and `password` for the page at `url`.
  static std::unique_ptr<MockIOSChromeSavePasswordInfoBarDelegate> Create(
      NSString* username,
      NSString* password,
      const GURL& url = GURL(),
      std::optional<std::string> account_to_store_password = std::nullopt);

  MOCK_METHOD(void, InfoBarDismissed, (), (override));
  MOCK_METHOD(void,
              UpdateCredentials,
              (NSString * username, NSString* password),
              (override));
  MOCK_METHOD(bool, Accept, (), (override));
  MOCK_METHOD(bool, Cancel, (), (override));
  MOCK_METHOD(void, InfobarPresenting, (bool automatic), (override));
  MOCK_METHOD(void, InfobarGone, (), (override));

 private:
  MockIOSChromeSavePasswordInfoBarDelegate(
      std::unique_ptr<password_manager::PasswordForm> form,
      std::unique_ptr<GURL> url,
      std::optional<std::string> account_to_store_password);

  std::unique_ptr<password_manager::PasswordForm> form_;
  std::unique_ptr<GURL> url_;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_TEST_MOCK_IOS_CHROME_SAVE_PASSWORDS_INFOBAR_DELEGATE_H_
