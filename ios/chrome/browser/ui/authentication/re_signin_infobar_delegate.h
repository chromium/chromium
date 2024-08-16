// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_RE_SIGNIN_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_RE_SIGNIN_INFOBAR_DELEGATE_H_

#import <memory>
#import <string>

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "components/infobars/core/confirm_infobar_delegate.h"
#import "components/infobars/core/infobar.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ui/gfx/image/image.h"

@class AppState;
class AuthenticationService;
@protocol SigninPresenter;

// A confirmation infobar prompting user to bring up the sign-in screen.
class ReSignInInfoBarDelegate : public ConfirmInfoBarDelegate,
                                public signin::IdentityManager::Observer {
 public:
  // Returns nullptr if the infobar must not be shown.
  static std::unique_ptr<ReSignInInfoBarDelegate> Create(
      AuthenticationService* authentication_service,
      signin::IdentityManager* identity_manager,
      AppState* app_state,
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

  // IdentityManager::Observer.
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;

 private:
  ReSignInInfoBarDelegate(AuthenticationService* authentication_service,
                          signin::IdentityManager* identity_manager,
                          id<SigninPresenter> signin_presenter);

  const raw_ptr<AuthenticationService> authentication_service_;
  const id<SigninPresenter> signin_presenter_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observer_{this};
};

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_RE_SIGNIN_INFOBAR_DELEGATE_H_
