// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/providers/images/chromium_branded_image_provider.h"

#import <UIKit/UIKit.h>

#include "ios/chrome/grit/ios_theme_resources.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

ChromiumBrandedImageProvider::ChromiumBrandedImageProvider() {}

ChromiumBrandedImageProvider::~ChromiumBrandedImageProvider() {}

UIImage* ChromiumBrandedImageProvider::GetAccountsListActivityControlsImage() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetNativeImageNamed(IDR_IOS_SETTINGS_INFO_24).ToUIImage();
}

UIImage*
ChromiumBrandedImageProvider::GetClearBrowsingDataAccountActivityImage() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetNativeImageNamed(IDR_IOS_SETTINGS_INFO_24).ToUIImage();
}

UIImage* ChromiumBrandedImageProvider::GetClearBrowsingDataSiteDataImage() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetNativeImageNamed(IDR_IOS_SETTINGS_INFO_24).ToUIImage();
}

UIImage*
ChromiumBrandedImageProvider::GetSigninConfirmationSyncSettingsImage() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetNativeImageNamed(IDR_IOS_SETTINGS_INFO_24).ToUIImage();
}

UIImage*
ChromiumBrandedImageProvider::GetSigninConfirmationPersonalizeServicesImage() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetNativeImageNamed(IDR_IOS_SETTINGS_INFO_24).ToUIImage();
}

UIImage* ChromiumBrandedImageProvider::GetWhatsNewIconImage(WhatsNewIcon type) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetNativeImageNamed(IDR_IOS_PROMO_INFO).ToUIImage();
}

UIImage* ChromiumBrandedImageProvider::GetDownloadGoogleDriveImage() {
  return [UIImage imageNamed:@"download_drivium"];
}
