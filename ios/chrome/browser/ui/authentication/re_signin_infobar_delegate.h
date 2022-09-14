// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_RE_SIGNIN_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_RE_SIGNIN_INFOBAR_DELEGATE_H_

#include <memory>
#include <string>

#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "ui/gfx/image/image.h"

class ChromeBrowserState;
@protocol SigninPresenter;

namespace infobars {
class InfoBarManager;
}  // namespace infobars

namespace web {
class WebState;
}  // namespace web

// A confirmation infobar prompting user to bring up the sign-in screen.
class ReSignInInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  ReSignInInfoBarDelegate(ChromeBrowserState* browser_state,
                          id<SigninPresenter> presenter);

  ReSignInInfoBarDelegate(const ReSignInInfoBarDelegate&) = delete;
  ReSignInInfoBarDelegate& operator=(const ReSignInInfoBarDelegate&) = delete;

  ~ReSignInInfoBarDelegate() override;

  // Creates a re-sign-in error infobar and adds it to the `web_state`. Returns
  // whether the infobar was actually added.
  static bool Create(ChromeBrowserState* browser_state,
                     web::WebState* web_state,
                     id<SigninPresenter> presenter);

  // Creates a re-sign-in error infobar, but does not add it to tab content.
  static std::unique_ptr<infobars::InfoBar> CreateInfoBar(
      infobars::InfoBarManager* infobar_manager,
      ChromeBrowserState* browser_state,
      id<SigninPresenter> presenter);

  // Creates a re-sign-in error infobar delegate, visible for testing.
  static std::unique_ptr<ReSignInInfoBarDelegate> CreateInfoBarDelegate(
      ChromeBrowserState* browser_state,
      id<SigninPresenter> presenter);

  // InfobarDelegate implementation.
  InfoBarIdentifier GetIdentifier() const override;

  // ConfirmInfoBarDelegate implementation.
  std::u16string GetMessageText() const override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  ui::ImageModel GetIcon() const override;
  bool Accept() override;
  void InfoBarDismissed() override;

 private:
  ChromeBrowserState* browser_state_;
  gfx::Image icon_;
  id<SigninPresenter> presenter_;
};

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_RE_SIGNIN_INFOBAR_DELEGATE_H_
