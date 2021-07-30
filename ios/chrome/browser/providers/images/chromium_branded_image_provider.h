// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROVIDERS_IMAGES_CHROMIUM_BRANDED_IMAGE_PROVIDER_H_
#define IOS_CHROME_BROWSER_PROVIDERS_IMAGES_CHROMIUM_BRANDED_IMAGE_PROVIDER_H_

#include "ios/public/provider/chrome/browser/images/branded_image_provider.h"

class ChromiumBrandedImageProvider : public BrandedImageProvider {
 public:
  ChromiumBrandedImageProvider();
  ~ChromiumBrandedImageProvider() override;

  // BrandedImageProvider.
  UIImage* GetClearBrowsingDataAccountActivityImage() override;
  UIImage* GetClearBrowsingDataSiteDataImage() override;
  UIImage* GetWhatsNewIconImage(WhatsNewIcon type) override;
  UIImage* GetDownloadGoogleDriveImage() override;
  UIImage* GetStaySafePromoImage() override;
  UIImage* GetMadeForIOSPromoImage() override;
  UIImage* GetMadeForIPadOSPromoImage() override;
  UIImage* GetNonModalPromoImage() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromiumBrandedImageProvider);
};

#endif  // IOS_CHROME_BROWSER_PROVIDERS_IMAGES_CHROMIUM_BRANDED_IMAGE_PROVIDER_H_
