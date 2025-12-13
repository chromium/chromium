// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/app_bundle_promo/ui/app_bundle_promo_config.h"

#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_constants.h"

@implementation AppBundlePromoConfig

#pragma mark - MagicStackModule

- (ContentSuggestionsModuleType)type {
  return ContentSuggestionsModuleType::kAppBundlePromo;
}

@end
