// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/providers/images/chromium_branded_image_provider.h"

#include "ios/chrome/grit/ios_theme_resources.h"
#import "ios/public/provider/chrome/browser/branded_images/branded_images_api.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

ChromiumBrandedImageProvider::ChromiumBrandedImageProvider() {}

ChromiumBrandedImageProvider::~ChromiumBrandedImageProvider() {}

UIImage*
ChromiumBrandedImageProvider::GetClearBrowsingDataAccountActivityImage() {
  return ios::provider::GetBrandedImage(
      ios::provider::BrandedImage::kClearBrowsingDataAccountActivity);
}

UIImage* ChromiumBrandedImageProvider::GetClearBrowsingDataSiteDataImage() {
  return ios::provider::GetBrandedImage(
      ios::provider::BrandedImage::kClearBrowsingDataSiteData);
}

UIImage* ChromiumBrandedImageProvider::GetWhatsNewIconImage(WhatsNewIcon type) {
  switch (type) {
    case WHATS_NEW_LOGO:
      return ios::provider::GetBrandedImage(
          ios::provider::BrandedImage::kWhatsNewLogo);
    case WHATS_NEW_LOGO_ROUNDED_RECTANGLE:
      return ios::provider::GetBrandedImage(
          ios::provider::BrandedImage::kWhatsNewLogoRoundedRectangle);
    case WHATS_NEW_INFO: {
      ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
      return rb.GetNativeImageNamed(IDR_IOS_PROMO_INFO).ToUIImage();
    }
  }
}

UIImage* ChromiumBrandedImageProvider::GetDownloadGoogleDriveImage() {
  return ios::provider::GetBrandedImage(
      ios::provider::BrandedImage::kDownloadGoogleDrive);
}

UIImage* ChromiumBrandedImageProvider::GetStaySafePromoImage() {
  return ios::provider::GetBrandedImage(
      ios::provider::BrandedImage::kStaySafePromo);
}

UIImage* ChromiumBrandedImageProvider::GetMadeForIOSPromoImage() {
  return ios::provider::GetBrandedImage(
      ios::provider::BrandedImage::kMadeForIOSPromo);
}

UIImage* ChromiumBrandedImageProvider::GetMadeForIPadOSPromoImage() {
  return ios::provider::GetBrandedImage(
      ios::provider::BrandedImage::kMadeForIPadOSPromo);
}

UIImage* ChromiumBrandedImageProvider::GetNonModalPromoImage() {
  return ios::provider::GetBrandedImage(
      ios::provider::BrandedImage::kNonModalDefaultBrowserPromo);
}
