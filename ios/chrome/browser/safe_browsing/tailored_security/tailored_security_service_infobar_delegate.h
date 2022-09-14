// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_TAILORED_SECURITY_SERVICE_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_TAILORED_SECURITY_SERVICE_INFOBAR_DELEGATE_H_

#import "components/infobars/core/confirm_infobar_delegate.h"

#import <string>

namespace safe_browsing {

class TailoredSecurityServiceInfobarDelegate : public ConfirmInfoBarDelegate {
 public:
  TailoredSecurityServiceInfobarDelegate(bool consent_status);
  TailoredSecurityServiceInfobarDelegate(
      const TailoredSecurityServiceInfobarDelegate&) = delete;
  TailoredSecurityServiceInfobarDelegate& operator=(
      const TailoredSecurityServiceInfobarDelegate&) = delete;

  // Returns |delegate| as an TailoredSecurityServiceInfobarDelegate, or
  // nullptr if it is of another type.
  static TailoredSecurityServiceInfobarDelegate* FromInfobarDelegate(
      infobars::InfoBarDelegate* delegate);

  // Returns the message button text.
  std::u16string GetMessageActionText() const;

  // Returns the subtitle text to be displayed in the banner.
  std::u16string GetDescription() const;

  // Returns the consent status of the user.
  bool consent_status() const { return consent_status_; }

  // ConfirmInfoBarDelegate
  std::u16string GetMessageText() const override;
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  bool EqualsDelegate(infobars::InfoBarDelegate* delegate) const override;

 private:
  // Returns the consent status based on if the user has enabled Account
  // Enhanced Safe Browsing. Default value is false since Account Enhanced Safe
  // Browsing is disabled by default.
  bool consent_status_ = false;
};

}  // namespace safe_browsing

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_TAILORED_SECURITY_SERVICE_INFOBAR_DELEGATE_H_
