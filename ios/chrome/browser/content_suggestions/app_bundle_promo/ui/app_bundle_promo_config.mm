// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/app_bundle_promo/ui/app_bundle_promo_config.h"

#import "ios/chrome/browser/content_suggestions/app_bundle_promo/public/app_bundle_promo_constants.h"
#import "ios/chrome/browser/content_suggestions/app_bundle_promo/ui/app_bundle_promo_audience.h"
#import "ios/chrome/browser/content_suggestions/public/content_suggestions_constants.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/icon_view_configuration.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

using l10n_util::GetNSString;

namespace {

// Size of the App Bundle Icon.
constexpr CGFloat kAppBundleIconSize = 40;

}  // namespace

@implementation AppBundlePromoConfig

#pragma mark - Public

- (instancetype)init {
  return [self initWithImageNamed:kAppBundleIconDefaultImageName];
}

- (instancetype)initWithImageNamed:(NSString*)imageName {
  if ((self = [super init])) {
    _imageName = [imageName copy];
  }
  return self;
}

#pragma mark - NSCopying

- (instancetype)copyWithZone:(NSZone*)zone {
  AppBundlePromoConfig* config =
      [[super copyWithZone:zone] initWithImageNamed:_imageName];
  config.audience = self.audience;
  return config;
}

#pragma mark - MagicStackModule

- (ContentSuggestionsModuleType)type {
  return ContentSuggestionsModuleType::kAppBundlePromo;
}

#pragma mark - IconDetailViewConfig

- (NSString*)titleText {
  return GetNSString(IDS_IOS_MAGIC_STACK_APP_BUNDLE_PROMO_CARD_TITLE);
}

- (NSString*)descriptionText {
  return (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET)
             ? GetNSString(
                   IDS_IOS_MAGIC_STACK_APP_BUNDLE_PROMO_CARD_IPAD_DESCRIPTION)
             : GetNSString(
                   IDS_IOS_MAGIC_STACK_APP_BUNDLE_PROMO_CARD_IPHONE_DESCRIPTION);
}

- (NSString*)accessibilityIdentifier {
  return kAppBundlePromoViewID;
}

- (IconDetailViewLayoutType)layoutType {
  return IconDetailViewLayoutType::kHero;
}

- (NSString*)iconName {
  return _imageName;
}

- (IconViewSourceType)iconSource {
  return IconViewSourceType::kImage;
}

- (CGFloat)iconWidth {
  return kAppBundleIconSize;
}

#pragma mark - IconDetailViewTapDelegate

- (void)didTapIconDetailView:(IconDetailView*)view {
  [self.audience didSelectAppBundlePromo];
}

@end
