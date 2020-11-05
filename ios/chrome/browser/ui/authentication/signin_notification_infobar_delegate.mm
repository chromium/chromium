// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/authentication/signin_notification_infobar_delegate.h"

#import <UIKit/UIKit.h>

#include <utility>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/infobars/core/infobar_manager.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
bool SigninNotificationInfoBarDelegate::Create(
    infobars::InfoBarManager* infobar_manager,
    ChromeBrowserState* browser_state) {
  DCHECK(infobar_manager);
  std::unique_ptr<ConfirmInfoBarDelegate> delegate(
      std::make_unique<SigninNotificationInfoBarDelegate>(browser_state));
  return !!infobar_manager->AddInfoBar(
      infobar_manager->CreateConfirmInfoBar(std::move(delegate)));
}

SigninNotificationInfoBarDelegate::SigninNotificationInfoBarDelegate(
    ChromeBrowserState* browser_state) {
  DCHECK(!browser_state->IsOffTheRecord());

  AuthenticationService* auth_service =
      AuthenticationServiceFactory::GetForBrowserState(browser_state);
  DCHECK(auth_service);
  ChromeIdentity* identity = auth_service->GetAuthenticatedIdentity();

  message_ = base::SysNSStringToUTF16(l10n_util::GetNSStringF(
      IDS_IOS_SIGNIN_ACCOUNT_NOTIFICATION_TITLE_WITH_USERNAME,
      base::SysNSStringToUTF16(identity.userFullName)));
  button_text_ =
      base::SysNSStringToUTF16(l10n_util::GetNSString(IDS_IOS_SETTINGS_TITLE));
}

SigninNotificationInfoBarDelegate::~SigninNotificationInfoBarDelegate() {}

infobars::InfoBarDelegate::InfoBarIdentifier
SigninNotificationInfoBarDelegate::GetIdentifier() const {
  return RE_SIGN_IN_INFOBAR_DELEGATE_IOS;
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

bool SigninNotificationInfoBarDelegate::Accept() {
  // TODO(crbug.com/1145592): Add event to open Settings menu.
  return false;
}
