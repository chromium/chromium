// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/notreached.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_theme_resources.h"
#import "ios/public/provider/chrome/browser/branded_images/branded_images_api.h"
#import "ui/base/resource/resource_bundle.h"

namespace ios {
namespace provider {

UIImage* GetBrandedImage(BrandedImage branded_image) {
  switch (branded_image) {
    case BrandedImage::kDownloadGoogleDrive:
      return [UIImage imageNamed:@"download_drivium"];

    case BrandedImage::kOmniboxAnswer:
      return nil;

    case BrandedImage::kStaySafePromo:
      return [UIImage imageNamed:@"chromium_stay_safe"];

    case BrandedImage::kMadeForIOSPromo:
      return [UIImage imageNamed:@"chromium_ios_made"];

    case BrandedImage::kMadeForIPadOSPromo:
      return [UIImage imageNamed:@"chromium_ipados_made"];

    case BrandedImage::kNonModalDefaultBrowserPromo:
      return [UIImage imageNamed:@"chromium_non_default_promo"];

    case BrandedImage::kPasswordSuggestionKey:
      return [UIImage imageNamed:@"password_suggestion_key"];
  }

  NOTREACHED_IN_MIGRATION();
  return nil;
}

}  // namespace provider
}  // namespace ios
