// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/permissions_overlay_infobar_delegate.h"

#import "components/infobars/core/infobar_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

PermissionsOverlayInfobarDelegate::PermissionsOverlayInfobarDelegate(
    NSArray<NSNumber*>* recently_accessible_permissions,
    web::WebState* web_state)
    : recently_accessible_permissions_(recently_accessible_permissions),
      web_state_(web_state) {}

PermissionsOverlayInfobarDelegate::~PermissionsOverlayInfobarDelegate() =
    default;

NSArray<NSNumber*>*
PermissionsOverlayInfobarDelegate::GetMostRecentlyAccessiblePermissions() {
  return recently_accessible_permissions_;
}

// As we don't need message in the infobar, we return empty message to satisfy
// implementation requirement for ConfirmInfoBarDelegate.
std::u16string PermissionsOverlayInfobarDelegate::GetMessageText() const {
  return std::u16string();
}

web::WebState* PermissionsOverlayInfobarDelegate::GetWebState() const {
  return web_state_;
}

infobars::InfoBarDelegate::InfoBarIdentifier
PermissionsOverlayInfobarDelegate::GetIdentifier() const {
  return IOS_PERMISSIONS_INFOBAR_DELEGATE;
}
