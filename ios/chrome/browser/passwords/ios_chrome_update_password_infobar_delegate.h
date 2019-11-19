// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_UPDATE_PASSWORD_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_UPDATE_PASSWORD_INFOBAR_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_manager_infobar_delegate.h"

@protocol ApplicationCommands;
@class UIViewController;

namespace password_manager {
class PasswordFormManagerForUI;
}

namespace infobars {
class InfoBarManager;
}
@class NSArray;

class IOSChromeUpdatePasswordInfoBarDelegate
    : public IOSChromePasswordManagerInfoBarDelegate {
 public:
  // Creates the infobar for |form_to_save| and adds it to |infobar_manager|.
  // |is_sync_user| controls the footer text. |baseViewController| is the base
  // view controller from which to present UI, and is not retained.
  // |dispatcher| is not retained.
  static void Create(
      bool is_sync_user,
      infobars::InfoBarManager* infobar_manager,
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
      UIViewController* baseViewController,
      id<ApplicationCommands> dispatcher);

  ~IOSChromeUpdatePasswordInfoBarDelegate() override;

  // Returns whether the user has multiple saved credentials, of which the
  // infobar affects just one. If so, the infobar should clarify which
  // credential is being affected.
  bool ShowMultipleAccounts() const;

  // Returns an array of credentials. Applicable if the user has multiple
  // saved credentials for the site |form_to_save| is applied to.
  NSArray* GetAccounts() const;

  base::string16 selected_account() const { return selected_account_; }

  void set_selected_account(base::string16 account) {
    selected_account_ = account;
  }

 private:
  IOSChromeUpdatePasswordInfoBarDelegate(
      bool is_sync_user,
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save);

  // ConfirmInfoBarDelegate implementation.
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  base::string16 GetMessageText() const override;
  int GetButtons() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  void InfoBarDismissed() override;
  base::string16 GetLinkText() const override;

  // The credential that should be displayed in the infobar, and for which the
  // password will be updated.
  base::string16 selected_account_;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_UPDATE_PASSWORD_INFOBAR_DELEGATE_H_
