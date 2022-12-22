// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/infobar_modal/permissions/permissions_modal_overlay_request_config.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/permissions/permissions_infobar_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

OVERLAY_USER_DATA_SETUP_IMPL(PermissionsInfobarModalOverlayRequestConfig);

PermissionsInfobarModalOverlayRequestConfig::
    PermissionsInfobarModalOverlayRequestConfig(InfoBarIOS* infobar)
    : infobar_(infobar) {
  DCHECK(infobar_);
  PermissionsInfobarDelegate* delegate =
      static_cast<PermissionsInfobarDelegate*>(infobar_->delegate());
  web_state_ = delegate->GetWebState();

  web::NavigationItem* visible_item =
      web_state_->GetNavigationManager()->GetVisibleItem();
  const GURL& URL = visible_item->GetURL();

  permissions_description_ =
      l10n_util::GetNSStringF(IDS_IOS_PERMISSIONS_INFOBAR_MODAL_DESCRIPTION,
                              base::UTF8ToUTF16(URL.host()));
}

PermissionsInfobarModalOverlayRequestConfig::
    ~PermissionsInfobarModalOverlayRequestConfig() = default;

void PermissionsInfobarModalOverlayRequestConfig::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  InfobarOverlayRequestConfig::CreateForUserData(
      user_data, infobar_, InfobarOverlayType::kModal, false);
}
