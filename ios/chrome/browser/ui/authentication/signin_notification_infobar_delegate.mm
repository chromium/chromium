// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/authentication/signin_notification_infobar_delegate.h"

#import <UIKit/UIKit.h>

#include <utility>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/sys_string_conversions.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/signin/ios/browser/features.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/infobars/infobar_utils.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Avatar profile picture size.
const CGFloat kAvatarImageDimension = 40.0f;

}  // namespace

// static
bool SigninNotificationInfoBarDelegate::Create(
    infobars::InfoBarManager* infobar_manager,
    ChromeBrowserState* browser_state,
    id<ApplicationSettingsCommands> dispatcher,
    UIViewController* view_controller) {
  DCHECK(infobar_manager);
  std::unique_ptr<ConfirmInfoBarDelegate> delegate(
      std::make_unique<SigninNotificationInfoBarDelegate>(
          browser_state, dispatcher, view_controller));
  std::unique_ptr<infobars::InfoBar> infobar =
      CreateHighPriorityConfirmInfoBar(std::move(delegate));
  return !!infobar_manager->AddInfoBar(std::move(infobar));
}

SigninNotificationInfoBarDelegate::SigninNotificationInfoBarDelegate(
    ChromeBrowserState* browser_state,
    id<ApplicationSettingsCommands> dispatcher,
    UIViewController* view_controller)
    : dispatcher_(dispatcher), base_view_controller_(view_controller) {
  DCHECK(!browser_state->IsOffTheRecord());

  AuthenticationService* auth_service =
      AuthenticationServiceFactory::GetForBrowserState(browser_state);
  DCHECK(auth_service);
  ChromeIdentity* identity = auth_service->GetAuthenticatedIdentity();

  UIImage* image = ios::GetChromeBrowserProvider()
                       ->GetChromeIdentityService()
                       ->GetCachedAvatarForIdentity(identity);
  if (!image) {
    image = [UIImage imageNamed:@"ios_default_avatar"];
  }
  icon_ = gfx::Image(CircularImageFromImage(image, kAvatarImageDimension));

  if (base::FeatureList::IsEnabled(
          signin::kSigninNotificationInfobarUsernameInTitle)) {
    title_ = base::SysNSStringToUTF16(l10n_util::GetNSStringF(
        IDS_IOS_SIGNIN_ACCOUNT_NOTIFICATION_TITLE_WITH_USERNAME,
        base::SysNSStringToUTF16(identity.userGivenName)));
  } else {
    title_ = base::SysNSStringToUTF16(
        l10n_util::GetNSString(IDS_IOS_SIGNIN_ACCOUNT_NOTIFICATION_TITLE));
  }

  message_ = base::SysNSStringToUTF16(identity.userEmail);
  button_text_ =
      base::SysNSStringToUTF16(l10n_util::GetNSString(IDS_IOS_SETTINGS_TITLE));
}

SigninNotificationInfoBarDelegate::~SigninNotificationInfoBarDelegate() {}

infobars::InfoBarDelegate::InfoBarIdentifier
SigninNotificationInfoBarDelegate::GetIdentifier() const {
  return RE_SIGN_IN_INFOBAR_DELEGATE_IOS;
}

base::string16 SigninNotificationInfoBarDelegate::GetTitleText() const {
  return title_;
}

base::string16 SigninNotificationInfoBarDelegate::GetMessageText() const {
  return message_;
}

int SigninNotificationInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

base::string16 SigninNotificationInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK(button == BUTTON_OK);
  return button_text_;
}

gfx::Image SigninNotificationInfoBarDelegate::GetIcon() const {
  return icon_;
}

bool SigninNotificationInfoBarDelegate::UseIconBackgroundTint() const {
  return false;
}

bool SigninNotificationInfoBarDelegate::Accept() {
  [dispatcher_ showAccountsSettingsFromViewController:base_view_controller_];
  base::RecordAction(base::UserMetricsAction(
      "Settings.GoogleServices.FromSigninNotificationInfobar"));
  return true;
}
