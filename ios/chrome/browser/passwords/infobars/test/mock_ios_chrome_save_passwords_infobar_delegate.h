// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_INFOBARS_TEST_MOCK_IOS_CHROME_SAVE_PASSWORDS_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_PASSWORDS_INFOBARS_TEST_MOCK_IOS_CHROME_SAVE_PASSWORDS_INFOBAR_DELEGATE_H_

#import <Foundation/Foundation.h>

#import <memory>
#import <optional>

#import "components/password_manager/core/browser/password_form.h"
#import "ios/chrome/browser/passwords/infobars/model/ios_chrome_save_password_infobar_delegate.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "url/gurl.h"

namespace password_manager {
class MockPasswordFormManagerForUI;
class PasswordFormMetricsRecorder;
}  // namespace password_manager

namespace syncer {
class SyncService;
}  // namespace syncer

// Mock queue observer.
class MockIOSChromeSavePasswordInfoBarDelegate
    : public IOSChromeSavePasswordInfoBarDelegate {
 public:
  ~MockIOSChromeSavePasswordInfoBarDelegate() override;

  // Factory method that creates a mock save password delegate for pending
  // with credentials `username` and `password` for the page at `url`.
  static std::unique_ptr<MockIOSChromeSavePasswordInfoBarDelegate> Create(
      NSString* username,
      NSString* password,
      const GURL& url = GURL(),
      const syncer::SyncService* sync_service = nullptr,
      password_manager::PasswordFormMetricsRecorder* metrics_recorder =
          nullptr);

  MOCK_METHOD(void, InfoBarDismissed, (), (override));
  MOCK_METHOD(bool, Accept, (), (override));
  MOCK_METHOD(bool, Cancel, (), (override));

  password_manager::MockPasswordFormManagerForUI* mock_form_manager() const {
    return mock_form_manager_;
  }

 private:
  MockIOSChromeSavePasswordInfoBarDelegate(
      std::unique_ptr<password_manager::PasswordForm> form,
      std::unique_ptr<GURL> url,
      const syncer::SyncService* sync_service,
      std::unique_ptr<password_manager::MockPasswordFormManagerForUI>
          form_manager,
      password_manager::MockPasswordFormManagerForUI* mock_form_manager_ptr);

  std::unique_ptr<password_manager::PasswordForm> form_;
  std::unique_ptr<GURL> url_;
  raw_ptr<password_manager::MockPasswordFormManagerForUI> mock_form_manager_ =
      nullptr;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_INFOBARS_TEST_MOCK_IOS_CHROME_SAVE_PASSWORDS_INFOBAR_DELEGATE_H_
