// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/model/background_customization_configuration_item.h"

#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "url/gurl.h"

@implementation BackgroundCustomizationConfigurationItem {
  CollectionImage _collectionImage;
  HomeCustomizationBackgroundStyle _backgroundStyle;
  NSString* _configurationID;
  UIColor* _backgroundColor;
  ui::ColorProviderKey::SchemeVariant _colorVariant;
}

- (instancetype)initWithCollectionImage:
    (const CollectionImage&)collectionImage {
  self = [super init];
  if (self) {
    _collectionImage = collectionImage;
    _backgroundStyle = HomeCustomizationBackgroundStyle::kPreset;
    _configurationID = [NSString
        stringWithFormat:@"%@_%ld_%@", kBackgroundCellIdentifier,
                         _backgroundStyle,
                         base::SysUTF8ToNSString(
                             base::NumberToString(collectionImage.asset_id))];
  }
  return self;
}

- (instancetype)initWithBackgroundColor:(UIColor*)backgroundColor
                           colorVariant:(ui::ColorProviderKey::SchemeVariant)
                                            colorVariant {
  self = [super init];
  if (self) {
    _backgroundStyle = HomeCustomizationBackgroundStyle::kColor;
    _configurationID = [NSString
        stringWithFormat:@"%@_%ld_%@", kBackgroundCellIdentifier,
                         _backgroundStyle, backgroundColor.description];
    _backgroundColor = backgroundColor;
    _colorVariant = colorVariant;
  }
  return self;
}

- (instancetype)initWithNoBackground {
  self = [super init];
  if (self) {
    _configurationID =
        [NSString stringWithFormat:@"%@_%ld", kBackgroundCellIdentifier,
                                   HomeCustomizationBackgroundStyle::kDefault];
  }
  return self;
}

- (const CollectionImage&)collectionImage {
  return _collectionImage;
}

#pragma mark - BackgroundCustomizationConfiguration

- (HomeCustomizationBackgroundStyle)backgroundStyle {
  return _backgroundStyle;
}

- (NSString*)configurationID {
  return _configurationID;
}

- (const GURL&)thumbnailURL {
  return _collectionImage.thumbnail_image_url;
}

- (UIColor*)backgroundColor {
  return _backgroundColor;
}

- (ui::ColorProviderKey::SchemeVariant)colorVariant {
  return _colorVariant;
}

@end
