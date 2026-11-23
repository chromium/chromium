// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_SIGNIN_NON_MODAL_PROMO_MODEL_NON_MODAL_SIGNIN_PROMO_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_SIGNIN_NON_MODAL_PROMO_MODEL_NON_MODAL_SIGNIN_PROMO_INFOBAR_DELEGATE_H_

#import <string>

#import "base/functional/callback.h"
#import "components/infobars/core/confirm_infobar_delegate.h"

// Delegate for the infobar backing the non-modal sign-in promo banner. It only
// provides the banner content and forwards the user's main action and the
// infobar's destruction to its owner, which owns the promo logic.
class NonModalSignInPromoInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // `message_text` is the banner subtitle. `accept_callback` is run when the
  // user taps the banner's action button. `destroyed_callback` is run when the
  // infobar is destroyed, whatever the reason (user action, timeout, explicit
  // removal or WebState destruction).
  NonModalSignInPromoInfoBarDelegate(std::u16string message_text,
                                     base::OnceClosure accept_callback,
                                     base::OnceClosure destroyed_callback);
  ~NonModalSignInPromoInfoBarDelegate() override;

  NonModalSignInPromoInfoBarDelegate(
      const NonModalSignInPromoInfoBarDelegate&) = delete;
  NonModalSignInPromoInfoBarDelegate& operator=(
      const NonModalSignInPromoInfoBarDelegate&) = delete;

  // ConfirmInfoBarDelegate:
  InfoBarIdentifier GetIdentifier() const override;
  std::u16string GetTitleText() const override;
  std::u16string GetMessageText() const override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  ui::ImageModel GetIcon() const override;
  bool UseIconBackgroundTint() const override;
  bool Accept() override;
  bool ShouldExpire(const NavigationDetails& details) const override;

 private:
  const std::u16string message_text_;
  base::OnceClosure accept_callback_;
  base::OnceClosure destroyed_callback_;
};

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_SIGNIN_NON_MODAL_PROMO_MODEL_NON_MODAL_SIGNIN_PROMO_INFOBAR_DELEGATE_H_
