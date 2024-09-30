// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_NOTIFICATION_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_NOTIFICATION_INFOBAR_DELEGATE_H_

#import <memory>
#import <string>

#import "components/infobars/core/confirm_infobar_delegate.h"
#import "components/sync/service/sync_service_observer.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#import "ui/gfx/image/image.h"

@protocol SettingsCommands;
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
  SigninNotificationInfoBarDelegate(ProfileIOS* profile,
                                    id<SettingsCommands> dispatcher,
                                    UIViewController* view_controller);

  SigninNotificationInfoBarDelegate(const SigninNotificationInfoBarDelegate&) =
      delete;
  SigninNotificationInfoBarDelegate& operator=(
      const SigninNotificationInfoBarDelegate&) = delete;

  ~SigninNotificationInfoBarDelegate() override;

  // Creates a sign-in notification infobar and adds it to `infobar_manager`.
  static bool Create(infobars::InfoBarManager* infobar_manager,
                     ProfileIOS* profile,
                     id<SettingsCommands> dispatcher,
                     UIViewController* view_controller);

  // InfoBarDelegate implementation.
  InfoBarIdentifier GetIdentifier() const override;

  // ConfirmInfoBarDelegate implementation.
  std::u16string GetTitleText() const override;
  std::u16string GetMessageText() const override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  ui::ImageModel GetIcon() const override;
  bool UseIconBackgroundTint() const override;
  bool Accept() override;
  bool ShouldExpire(const NavigationDetails& details) const override;

 private:
  gfx::Image icon_;
  std::u16string title_;
  std::u16string message_;
  std::u16string button_text_;

  // Dispatcher.
  __weak id<SettingsCommands> dispatcher_ = nil;
  __weak UIViewController* base_view_controller_ = nil;
};

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_NOTIFICATION_INFOBAR_DELEGATE_H_
