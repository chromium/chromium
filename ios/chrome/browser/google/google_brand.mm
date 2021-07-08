// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/google/google_brand.h"

#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/distribution/app_distribution_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {
namespace google_brand {

bool GetBrand(std::string* brand) {
  brand->assign(ios::GetChromeBrowserProvider()
                    .GetAppDistributionProvider()
                    ->GetDistributionBrandCode());
  return true;
}

bool IsOrganic(const std::string& brand) {
  // An empty brand string on iOS is used for organic installation. All other
  // iOS brand string are non-organic.
  return brand.empty();
}

}  // namespace google_brand
}  // namespace ios
