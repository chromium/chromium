// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/images/branded_image_provider.h"

#import <Foundation/Foundation.h>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BrandedImageProvider::BrandedImageProvider() {}

BrandedImageProvider::~BrandedImageProvider() {}

UIImage* BrandedImageProvider::GetAccountsListActivityControlsImage() {
  return nil;
}

UIImage* BrandedImageProvider::GetClearBrowsingDataAccountActivityImage() {
  return nil;
}

UIImage* BrandedImageProvider::GetClearBrowsingDataSiteDataImage() {
  return nil;
}

UIImage* BrandedImageProvider::GetSigninConfirmationSyncSettingsImage() {
  return nil;
}

UIImage* BrandedImageProvider::GetSigninConfirmationPersonalizeServicesImage() {
  return nil;
}

NSArray<UIImage*>* BrandedImageProvider::GetToolbarVoiceSearchButtonImages(
    bool incognito) {
  return nil;
}

UIImage* BrandedImageProvider::GetWhatsNewIconImage(WhatsNewIcon type) {
  return nil;
}

UIImage* BrandedImageProvider::GetDownloadGoogleDriveImage() {
  return nil;
}

UIImage* BrandedImageProvider::GetToolbarSearchIcon(SearchEngineIcon type,
                                                    bool dark_version) {
  return [UIImage imageNamed:@"toolbar_search"];
}

UIImage* BrandedImageProvider::GetOmniboxAnswerIcon() {
  return nil;
}
