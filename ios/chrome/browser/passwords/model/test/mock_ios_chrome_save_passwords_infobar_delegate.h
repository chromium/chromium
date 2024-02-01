// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_TEST_MOCK_IOS_CHROME_SAVE_PASSWORDS_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_TEST_MOCK_IOS_CHROME_SAVE_PASSWORDS_INFOBAR_DELEGATE_H_

#import <Foundation/Foundation.h>

#include <memory>
#include <optional>

#include "components/password_manager/core/browser/password_form.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_save_password_infobar_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

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

  MOCK_METHOD0(InfoBarDismissed, void());
  MOCK_METHOD2(UpdateCredentials, void(NSString* username, NSString* password));
  MOCK_METHOD0(Accept, bool());
  MOCK_METHOD0(Cancel, bool());
  MOCK_METHOD1(InfobarPresenting, void(bool automatic));
  MOCK_METHOD0(InfobarGone, void());

 private:
  MockIOSChromeSavePasswordInfoBarDelegate(
      std::unique_ptr<password_manager::PasswordForm> form,
      std::unique_ptr<GURL> url,
      std::optional<std::string> account_to_store_password);

  std::unique_ptr<password_manager::PasswordForm> form_;
  std::unique_ptr<GURL> url_;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_TEST_MOCK_IOS_CHROME_SAVE_PASSWORDS_INFOBAR_DELEGATE_H_
