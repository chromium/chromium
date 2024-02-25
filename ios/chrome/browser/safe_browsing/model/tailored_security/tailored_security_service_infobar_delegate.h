// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_TAILORED_SECURITY_TAILORED_SECURITY_SERVICE_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_TAILORED_SECURITY_TAILORED_SECURITY_SERVICE_INFOBAR_DELEGATE_H_

#import "components/infobars/core/confirm_infobar_delegate.h"

#import <Foundation/Foundation.h>

#import <string>

#import "base/memory/weak_ptr.h"

namespace web {
class WebState;
}  // namespace web

namespace safe_browsing {

enum class TailoredSecurityServiceMessageState {
  // Triggers message prompt when account level enhanced safe browsing is
  // enabled and user is synced.
  kConsentedAndFlowEnabled = 1,
  // Triggers message prompt when account level enhanced safe browsing is
  // disabled and user is synced.
  kConsentedAndFlowDisabled = 2,
  // Triggers message prompt when account level enhanced safe browsing is
  // disabled and user is not synced.
  kUnconsentedAndFlowEnabled = 3,
};

class TailoredSecurityServiceInfobarDelegate : public ConfirmInfoBarDelegate {
 public:
  explicit TailoredSecurityServiceInfobarDelegate(
      TailoredSecurityServiceMessageState message_state,
      web::WebState* web_state);
  TailoredSecurityServiceInfobarDelegate(
      const TailoredSecurityServiceInfobarDelegate&) = delete;
  TailoredSecurityServiceInfobarDelegate& operator=(
      const TailoredSecurityServiceInfobarDelegate&) = delete;
  ~TailoredSecurityServiceInfobarDelegate() override;

  // Returns |delegate| as an TailoredSecurityServiceInfobarDelegate, or
  // nullptr if it is of another type.
  static TailoredSecurityServiceInfobarDelegate* FromInfobarDelegate(
      infobars::InfoBarDelegate* delegate);

  // Returns the message button text.
  std::u16string GetMessageActionText() const;

  // Returns the subtitle text to be displayed in the banner.
  std::u16string GetDescription() const;

  // Returns the message state.
  TailoredSecurityServiceMessageState message_state() const {
    return message_state_;
  }

  // ConfirmInfoBarDelegate
  std::u16string GetMessageText() const override;
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  bool EqualsDelegate(infobars::InfoBarDelegate* delegate) const override;
  bool Accept() override;

 private:
  // Stores the state of the consent flow and is used to
  // return appropriate messages for the prompt.
  TailoredSecurityServiceMessageState message_state_;

  // Stores associated WebState.
  base::WeakPtr<web::WebState> web_state_;
};

}  // namespace safe_browsing

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_TAILORED_SECURITY_TAILORED_SECURITY_SERVICE_INFOBAR_DELEGATE_H_
