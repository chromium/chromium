// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_TEST_TEST_INFOBAR_PASSWORD_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_TEST_TEST_INFOBAR_PASSWORD_DELEGATE_H_

#import "ios/chrome/browser/passwords/model/ios_chrome_save_password_infobar_delegate.h"

// An infobar that displays `infobar_message` and one button.
class TestInfobarPasswordDelegate
    : public IOSChromeSavePasswordInfoBarDelegate {
 public:
  explicit TestInfobarPasswordDelegate(NSString* infobar_message);

  bool Create(infobars::InfoBarManager* infobar_manager);

  // InfoBarDelegate implementation.
  InfoBarIdentifier GetIdentifier() const override;
  // ConfirmInfoBarDelegate implementation.
  std::u16string GetMessageText() const override;
  int GetButtons() const override;

 private:
  NSString* infobar_message_;
};

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_TEST_TEST_INFOBAR_PASSWORD_DELEGATE_H_
