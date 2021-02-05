// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_NOTIFICATION_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_NOTIFICATION_INFOBAR_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/sync/driver/sync_service_observer.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ui/gfx/image/image.h"

@protocol ApplicationSettingsCommands;
class ChromeBrowserState;
@class UIViewController;

namespace gfx {
class Image;
}

namespace infobars {
class InfoBarManager;
}

// Shows a sign-in notification in an infobar.
class SigninNotificationInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  SigninNotificationInfoBarDelegate(ChromeBrowserState* browser_state,
                                    id<ApplicationSettingsCommands> dispatcher,
                                    UIViewController* view_controller);
  ~SigninNotificationInfoBarDelegate() override;

  // Creates a sign-in notification infobar and adds it to |infobar_manager|.
  static bool Create(infobars::InfoBarManager* infobar_manager,
                     ChromeBrowserState* browser_state,
                     id<ApplicationSettingsCommands> dispatcher,
                     UIViewController* view_controller);

  // InfoBarDelegate implementation.
  InfoBarIdentifier GetIdentifier() const override;

  // ConfirmInfoBarDelegate implementation.
  base::string16 GetTitleText() const override;
  base::string16 GetMessageText() const override;
  int GetButtons() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  gfx::Image GetIcon() const override;
  bool UseIconBackgroundTint() const override;
  bool Accept() override;
  bool ShouldExpire(const NavigationDetails& details) const override;

 private:
  gfx::Image icon_;
  base::string16 title_;
  base::string16 message_;
  base::string16 button_text_;

  // Dispatcher.
  __weak id<ApplicationSettingsCommands> dispatcher_ = nil;
  __weak UIViewController* base_view_controller_ = nil;

  DISALLOW_COPY_AND_ASSIGN(SigninNotificationInfoBarDelegate);
};

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_NOTIFICATION_INFOBAR_DELEGATE_H_
