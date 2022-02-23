// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/ios_chrome_password_manager_infobar_delegate.h"

#include <string>
#include <utility>

#include "base/strings/sys_string_conversions.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "ios/chrome/grit/ios_google_chrome_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IOSChromePasswordManagerInfoBarDelegate::
    ~IOSChromePasswordManagerInfoBarDelegate() = default;

IOSChromePasswordManagerInfoBarDelegate::
    IOSChromePasswordManagerInfoBarDelegate(
        NSString* user_email,
        bool is_sync_user,
        std::unique_ptr<password_manager::PasswordFormManagerForUI>
            form_to_save)
    : form_to_save_(std::move(form_to_save)),
      infobar_response_(password_manager::metrics_util::NO_DIRECT_INTERACTION),
      is_sync_user_(is_sync_user),
      user_email_(user_email) {}

NSString* IOSChromePasswordManagerInfoBarDelegate::GetDetailsMessageText()
    const {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::
              kIOSEnablePasswordManagerBrandingUpdate)) {
    return is_sync_user_ ? l10n_util::GetNSString(IDS_SAVE_PASSWORD_FOOTER)
                         : @"";
  }

  return is_sync_user_
             ? l10n_util::GetNSStringF(
                   IDS_SAVE_PASSWORD_FOOTER_DISPLAYING_USER_EMAIL,
                   base::SysNSStringToUTF16(user_email_))
             : l10n_util::GetNSString(IDS_IOS_SAVE_PASSWORD_FOOTER_NOT_SYNCING);
}

NSString* IOSChromePasswordManagerInfoBarDelegate::GetUserNameText() const {
  return base::SysUTF16ToNSString(
      form_to_save_->GetPendingCredentials().username_value);
}

NSString* IOSChromePasswordManagerInfoBarDelegate::GetPasswordText() const {
  return base::SysUTF16ToNSString(
      form_to_save_->GetPendingCredentials().password_value);
}

NSString* IOSChromePasswordManagerInfoBarDelegate::GetURLHostText() const {
  return base::SysUTF8ToNSString(form_to_save_->GetURL().host());
}

void IOSChromePasswordManagerInfoBarDelegate::set_handler(
    id<ApplicationCommands> handler) {
  handler_ = handler;
}

int IOSChromePasswordManagerInfoBarDelegate::GetIconId() const {
  return IDR_IOS_INFOBAR_SAVE_PASSWORD;
}
