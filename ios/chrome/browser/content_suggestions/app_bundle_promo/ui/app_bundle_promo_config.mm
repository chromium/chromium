// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/app_bundle_promo/ui/app_bundle_promo_config.h"

#import "ios/chrome/browser/content_suggestions/app_bundle_promo/public/app_bundle_promo_constants.h"
#import "ios/chrome/browser/content_suggestions/public/content_suggestions_constants.h"

@implementation AppBundlePromoConfig

- (instancetype)init {
  return [self initWithImageNamed:kAppBundleIconDefaultImageName];
}

- (instancetype)initWithImageNamed:(NSString*)imageName {
  if ((self = [super init])) {
    _imageName = [imageName copy];
  }
  return self;
}

#pragma mark - MagicStackModule

- (ContentSuggestionsModuleType)type {
  return ContentSuggestionsModuleType::kAppBundlePromo;
}

@end
