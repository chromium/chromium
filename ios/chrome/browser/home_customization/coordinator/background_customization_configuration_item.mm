// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/coordinator/background_customization_configuration_item.h"

#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/sync/protocol/theme_types.pb.h"
#import "ios/chrome/browser/home_customization/coordinator/home_customization_data_conversion.h"
#import "ios/chrome/browser/home_customization/model/home_background_data.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_framing_coordinates.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette_util.h"
#import "url/gurl.h"

@implementation BackgroundCustomizationConfigurationItem {
  HomeCustomizationBackgroundStyle _backgroundStyle;
  NSString* _configurationID;

  CollectionImage _collectionImage;

  sync_pb::NtpCustomBackground _customBackground;
  GURL _customBackgroundThumbnailURL;

  UIColor* _backgroundColor;
  ui::ColorProviderKey::SchemeVariant _colorVariant;

  NSString* _userUploadedImagePath;
  HomeCustomizationFramingCoordinates* _userUploadedFramingCoordinates;
}

@synthesize accessibilityName = _accessibilityName;
@synthesize accessibilityValue = _accessibilityValue;
@synthesize backgroundColor = _backgroundColor;
@synthesize isCustomColor = _isCustomColor;

- (instancetype)initWithUserUploadedImagePath:(NSString*)imagePath
                           framingCoordinates:
                               (const FramingCoordinates&)coordinates
                            accessibilityName:(NSString*)accessibilityName {
  self = [super init];
  if (self) {
    _backgroundStyle = HomeCustomizationBackgroundStyle::kUserUploaded;
    _userUploadedImagePath = [imagePath copy];
    _userUploadedFramingCoordinates =
        HomeCustomizationFramingCoordinatesFromFramingCoordinates(coordinates);
    _configurationID = [NSString
        stringWithFormat:@"%@_%ld_%@", kBackgroundCellIdentifier,
                         _backgroundStyle, [imagePath lastPathComponent]];
    _accessibilityName = accessibilityName;
  }
  return self;
}

- (instancetype)initWithCollectionImage:(const CollectionImage&)collectionImage
                      accessibilityName:(NSString*)accessibilityName
                     accessibilityValue:(NSString*)accessibilityValue {
  self = [super init];
  if (self) {
    _collectionImage = collectionImage;
    _backgroundStyle = HomeCustomizationBackgroundStyle::kPreset;
    _configurationID =
        [NSString stringWithFormat:@"%@_%ld_%@", kBackgroundCellIdentifier,
                                   _backgroundStyle,
                                   base::SysUTF8ToNSString(
                                       collectionImage.image_url.spec())];
    _accessibilityName = accessibilityName;
    _accessibilityValue = accessibilityValue;
  }
  return self;
}

- (instancetype)initWithNtpCustomBackground:
                    (const sync_pb::NtpCustomBackground&)customBackground
                          accessibilityName:(NSString*)accessibilityName {
  self = [super init];
  if (self) {
    _customBackground = customBackground;
    _backgroundStyle = HomeCustomizationBackgroundStyle::kPreset;
    _customBackgroundThumbnailURL = AddOptionsToImageURL(
        RemoveOptionsFromImageURL(_customBackground.url()).spec(),
        GetThumbnailImageOptions());
    _configurationID = [NSString
        stringWithFormat:@"%@_%ld_%@", kBackgroundCellIdentifier,
                         _backgroundStyle,
                         base::SysUTF8ToNSString(customBackground.url())];
    _accessibilityName = accessibilityName;
  }
  return self;
}

- (instancetype)initWithBackgroundColor:(UIColor*)backgroundColor
                           colorVariant:
                               (ui::ColorProviderKey::SchemeVariant)colorVariant
                      accessibilityName:(NSString*)accessibilityName {
  self = [super init];
  if (self) {
    _backgroundStyle = HomeCustomizationBackgroundStyle::kColor;
    _configurationID = [NSString
        stringWithFormat:@"%@_%ld_%@", kBackgroundCellIdentifier,
                         _backgroundStyle, backgroundColor.description];
    _backgroundColor = backgroundColor;
    _colorVariant = colorVariant;
    _accessibilityName = accessibilityName;
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

- (const sync_pb::NtpCustomBackground&)customBackground {
  return _customBackground;
}

#pragma mark - BackgroundCustomizationConfiguration

- (HomeCustomizationBackgroundStyle)backgroundStyle {
  return _backgroundStyle;
}

- (NSString*)configurationID {
  return _configurationID;
}

- (const GURL&)thumbnailURL {
  return (_collectionImage.thumbnail_image_url.is_empty())
             ? _customBackgroundThumbnailURL
             : _collectionImage.thumbnail_image_url;
}

- (UIColor*)backgroundColor {
  return _backgroundColor;
}

- (ui::ColorProviderKey::SchemeVariant)colorVariant {
  return _colorVariant;
}

- (NewTabPageColorPalette*)colorPalette {
  if (self.backgroundStyle != HomeCustomizationBackgroundStyle::kColor) {
    return nil;
  }
  return CreateColorPaletteFromSeedColor(self.backgroundColor,
                                         self.colorVariant);
}

- (NSString*)userUploadedImagePath {
  return _userUploadedImagePath;
}

- (HomeCustomizationFramingCoordinates*)userUploadedFramingCoordinates {
  return _userUploadedFramingCoordinates;
}

@end
