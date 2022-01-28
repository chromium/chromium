// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/permissions_infobar_delegate.h"

#include "components/infobars/core/infobar_delegate.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

PermissionsInfoBarDelegate::PermissionsInfoBarDelegate(
    NSArray<NSNumber*>* recently_accessible_permissions,
    web::WebState* web_state)
    : recently_accessible_permissions_(recently_accessible_permissions),
      web_state_(web_state) {}

PermissionsInfoBarDelegate::~PermissionsInfoBarDelegate() = default;

NSArray<NSNumber*>*
PermissionsInfoBarDelegate::GetMostRecentlyAccessiblePermissions() {
  return recently_accessible_permissions_;
}

// As we don't need message in the infobar, we return empty message to satisfy
// implementation requirement for ConfirmInfoBarDelegate.
std::u16string PermissionsInfoBarDelegate::GetMessageText() const {
  return std::u16string();
}

web::WebState* PermissionsInfoBarDelegate::GetWebState() const {
  return web_state_;
}

infobars::InfoBarDelegate::InfoBarIdentifier
PermissionsInfoBarDelegate::GetIdentifier() const {
  return IOS_PERMISSIONS_INFOBAR_DELEGATE;
}
