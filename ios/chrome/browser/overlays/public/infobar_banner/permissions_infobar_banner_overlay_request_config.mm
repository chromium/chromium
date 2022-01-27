// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/infobar_banner/permissions_infobar_banner_overlay_request_config.h"

#include "base/strings/sys_string_conversions.h"
#include "components/infobars/core/infobar.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_type.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/ui/reading_list/ios_add_to_reading_list_infobar_delegate.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using infobars::InfoBar;

// TODO(crbug.com/1289645): Use appropriate images.
namespace {

// Name of the camera icon image for the permissions banner.
NSString* const kCameraIconImageName = @"infobar_reading_list";
// Name of the microphone icon image for the permissions banner.
NSString* const kMicrophoneIconImageName = @"infobar_reading_list";

}  // namespace

OVERLAY_USER_DATA_SETUP_IMPL(PermissionsBannerRequestConfig);

PermissionsBannerRequestConfig::PermissionsBannerRequestConfig(InfoBar* infobar)
    : infobar_(infobar) {
  DCHECK(infobar_);
  // TODO(crbug.com/1289645): Retrieve permissions from a delegate.

  // TODO(crbug.com/1289645): Use appropriate images.
  icon_image_name_ = kCameraIconImageName;

  title_text_ = @"Placeholder";

  button_text_ = l10n_util::GetNSString(IDS_IOS_EDIT_ACTION_TITLE);
}

PermissionsBannerRequestConfig::~PermissionsBannerRequestConfig() = default;

void PermissionsBannerRequestConfig::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  InfobarOverlayRequestConfig::CreateForUserData(
      user_data, static_cast<InfoBarIOS*>(infobar_),
      InfobarOverlayType::kBanner, false);
}
