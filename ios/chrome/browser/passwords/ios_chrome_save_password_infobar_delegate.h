// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_SAVE_PASSWORD_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_SAVE_PASSWORD_INFOBAR_DELEGATE_H_

#include <memory>

#import "ios/chrome/browser/passwords/ios_chrome_password_infobar_metrics_recorder.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_manager_infobar_delegate.h"

@protocol ApplicationCommands;

namespace password_manager {
class PasswordFormManagerForUI;
}

// After a successful *new* login attempt, Chrome passes the current
// password_manager::PasswordFormManager and move it to a
// IOSChromeSavePasswordInfoBarDelegate while the user makes up their mind
// with the "save password" infobar.
// If |password_update| is true the delegate will use "Update" related strings,
// and should Update the credentials instead of Saving new ones.
class IOSChromeSavePasswordInfoBarDelegate
    : public IOSChromePasswordManagerInfoBarDelegate {
 public:
  IOSChromeSavePasswordInfoBarDelegate(
      NSString* user_email,
      bool is_sync_user,
      bool password_update,
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save);

  IOSChromeSavePasswordInfoBarDelegate(
      const IOSChromeSavePasswordInfoBarDelegate&) = delete;
  IOSChromeSavePasswordInfoBarDelegate& operator=(
      const IOSChromeSavePasswordInfoBarDelegate&) = delete;

  ~IOSChromeSavePasswordInfoBarDelegate() override;

  // Returns |delegate| as an IOSChromeSavePasswordInfoBarDelegate, or nullptr
  // if it is of another type.
  static IOSChromeSavePasswordInfoBarDelegate* FromInfobarDelegate(
      infobars::InfoBarDelegate* delegate);

  // InfoBarDelegate implementation
  bool ShouldExpire(const NavigationDetails& details) const override;

  // ConfirmInfoBarDelegate implementation.
  std::u16string GetMessageText() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;
  void InfoBarDismissed() override;

  // Updates the credentials being saved with |username| and |password|.
  // TODO(crbug.com/1040653): This function is only virtual so it can be mocked
  // for testing purposes.  It should become non-virtual once this test is
  // refactored for testability.
  virtual void UpdateCredentials(NSString* username, NSString* password);

  // Informs the delegate that the Infobar has been presented. If |automatic|
  // YES the Infobar was presented automatically (e.g. The banner was
  // presented), if NO the user triggered it  (e.g. Tapped on the badge).
  // TODO(crbug.com/1040653): This function is only virtual so it can be mocked
  // for testing purposes.  It should become non-virtual once this test is
  // refactored for testability.
  virtual void InfobarPresenting(bool automatic);

  // Informs the delegate that the Infobar has been dismissed.
  // TODO(crbug.com/1040653): This function is only virtual so it can be mocked
  // for testing purposes.  It should become non-virtual once this test is
  // refactored for testability.
  virtual void InfobarDismissed();

  // true if password is being updated at the moment the InfobarModal is
  // created.
  bool IsPasswordUpdate() const;

  // true if the current set of credentials has already been saved at the moment
  // the InfobarModal is created.
  bool IsCurrentPasswordSaved() const;

  // The title for the InfobarModal being presented.
  NSString* GetInfobarModalTitleText() const;

 private:
  // ConfirmInfoBarDelegate implementation.
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;

  // true if password is being updated at the moment the InfobarModal is
  // created.
  bool password_update_ = false;

  // true if the current set of credentials has already been saved at the moment
  // the InfobarModal is created.
  bool current_password_saved_ = false;

  // The PasswordInfobarType for this delegate. Set at initialization and won't
  // change throughout the life of the delegate.
  const PasswordInfobarType infobar_type_;

  // YES if an Infobar is being presented by this delegate.
  bool infobar_presenting_ = false;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_SAVE_PASSWORD_INFOBAR_DELEGATE_H_
