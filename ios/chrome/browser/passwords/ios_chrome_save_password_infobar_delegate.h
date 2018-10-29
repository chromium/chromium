// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_SAVE_PASSWORD_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_SAVE_PASSWORD_INFOBAR_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_manager_infobar_delegate.h"

@protocol ApplicationCommands;

namespace password_manager {
class PasswordFormManagerForUI;
}

namespace infobars {
class InfoBarManager;
}

// After a successful *new* login attempt, Chrome passes the current
// password_manager::PasswordFormManager and move it to a
// IOSChromeSavePasswordInfoBarDelegate while the user makes up their mind
// with the "save password" infobar.
class IOSChromeSavePasswordInfoBarDelegate
    : public IOSChromePasswordManagerInfoBarDelegate {
 public:
  // Creates the infobar for |form_to_save| and adds it to |infobar_manager|.
  // |is_sync_user| controls the footer string. |dispatcher| is not retained.
  static void Create(
      bool is_sync_user,
      infobars::InfoBarManager* infobar_manager,
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
      id<ApplicationCommands> dispatcher);

  ~IOSChromeSavePasswordInfoBarDelegate() override;

  bool ShouldExpire(const NavigationDetails& details) const override;

 private:
  IOSChromeSavePasswordInfoBarDelegate(
      bool is_sync_user,
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save);

  // ConfirmInfoBarDelegate implementation.
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  base::string16 GetMessageText() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;

  DISALLOW_COPY_AND_ASSIGN(IOSChromeSavePasswordInfoBarDelegate);
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_SAVE_PASSWORD_INFOBAR_DELEGATE_H_
