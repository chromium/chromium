// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_RE_SIGNIN_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_RE_SIGNIN_INFOBAR_DELEGATE_H_

#include <memory>
#include <string>

#import "base/memory/raw_ptr.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "ui/gfx/image/image.h"

class AuthenticationService;
@protocol SigninPresenter;

// A confirmation infobar prompting user to bring up the sign-in screen.
class ReSignInInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Returns nullptr if the infobar must not be shown.
  static std::unique_ptr<ReSignInInfoBarDelegate> Create(
      AuthenticationService* authentication_service,
      id<SigninPresenter> signin_presenter);

  ReSignInInfoBarDelegate(const ReSignInInfoBarDelegate&) = delete;
  ReSignInInfoBarDelegate& operator=(const ReSignInInfoBarDelegate&) = delete;

  ~ReSignInInfoBarDelegate() override;

  // InfobarDelegate implementation.
  InfoBarIdentifier GetIdentifier() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;

  // ConfirmInfoBarDelegate implementation.
  std::u16string GetTitleText() const override;
  std::u16string GetMessageText() const override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  ui::ImageModel GetIcon() const override;
  bool Accept() override;
  void InfoBarDismissed() override;

 private:
  ReSignInInfoBarDelegate(AuthenticationService* authentication_service,
                          id<SigninPresenter> signin_presenter);

  const raw_ptr<AuthenticationService> authentication_service_;
  const id<SigninPresenter> signin_presenter_;
};

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_RE_SIGNIN_INFOBAR_DELEGATE_H_
