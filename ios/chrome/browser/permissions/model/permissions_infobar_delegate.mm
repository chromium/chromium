// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/permissions/model/permissions_infobar_delegate.h"

#import "components/infobars/core/infobar_delegate.h"

PermissionsInfobarDelegate::PermissionsInfobarDelegate(
    NSArray<NSNumber*>* recently_accessible_permissions,
    web::WebState* web_state)
    : recently_accessible_permissions_(recently_accessible_permissions),
      web_state_(web_state) {}

PermissionsInfobarDelegate::~PermissionsInfobarDelegate() = default;

NSArray<NSNumber*>*
PermissionsInfobarDelegate::GetMostRecentlyAccessiblePermissions() {
  return recently_accessible_permissions_;
}

// As we don't need message in the infobar, we return empty message to satisfy
// implementation requirement for ConfirmInfoBarDelegate.
std::u16string PermissionsInfobarDelegate::GetMessageText() const {
  return std::u16string();
}

web::WebState* PermissionsInfobarDelegate::GetWebState() const {
  return web_state_;
}

infobars::InfoBarDelegate::InfoBarIdentifier
PermissionsInfobarDelegate::GetIdentifier() const {
  return IOS_PERMISSIONS_INFOBAR_DELEGATE;
}

bool PermissionsInfobarDelegate::EqualsDelegate(
    infobars::InfoBarDelegate* delegate) const {
  return delegate->GetIdentifier() == GetIdentifier();
}
