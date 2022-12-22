// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/infobar_banner/permissions_infobar_banner_overlay_request_config.h"

#import "base/strings/sys_string_conversions.h"
#import "components/infobars/core/infobar.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_type.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/permissions/permissions_infobar_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/permissions/permissions.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using infobars::InfoBar;

OVERLAY_USER_DATA_SETUP_IMPL(PermissionsBannerRequestConfig);

PermissionsBannerRequestConfig::PermissionsBannerRequestConfig(InfoBar* infobar)
    : infobar_(infobar) {
  DCHECK(infobar_);
  PermissionsInfobarDelegate* delegate =
      static_cast<PermissionsInfobarDelegate*>(infobar_->delegate());
  NSArray<NSNumber*>* accessible_permissions =
      delegate->GetMostRecentlyAccessiblePermissions();
  if ([accessible_permissions containsObject:@(web::PermissionCamera)]) {
    // Camera access is enabled.
    is_camera_accessible_ = true;
    title_text_ =
        [accessible_permissions containsObject:@(web::PermissionMicrophone)]
            ? l10n_util::GetNSString(
                  IDS_IOS_PERMISSIONS_INFOBAR_BANNER_CAMERA_AND_MICROPHONE_ACCESSIBLE)
            : l10n_util::GetNSString(
                  IDS_IOS_PERMISSIONS_INFOBAR_BANNER_CAMERA_ACCESSIBLE);
  } else {
    // Only microphone access is enabled.
    title_text_ = l10n_util::GetNSString(
        IDS_IOS_PERMISSIONS_INFOBAR_BANNER_MICROPHONE_ACCESSIBLE);
  }
  button_text_ = l10n_util::GetNSString(IDS_IOS_EDIT_ACTION_TITLE);
}

PermissionsBannerRequestConfig::~PermissionsBannerRequestConfig() = default;

void PermissionsBannerRequestConfig::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  InfobarOverlayRequestConfig::CreateForUserData(
      user_data, static_cast<InfoBarIOS*>(infobar_),
      InfobarOverlayType::kBanner, false);
}
