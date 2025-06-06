// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/model/background_customization_configuration.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "url/gurl.h"

@implementation BackgroundCustomizationConfiguration {
  NSString* _configurationID;
  GURL _thumbnailURL;
  GURL _highResURL;
}

- (instancetype)initWithCollectionImage:
    (const CollectionImage&)collectionImage {
  self = [super init];
  if (self) {
    _configurationID = [NSString
        stringWithFormat:@"%ld_%@",
                         HomeCustomizationBackgroundPickerType::
                             HomeCustomizationPickerTypePresetGallery,
                         base::SysUTF8ToNSString(
                             base::NumberToString(collectionImage.asset_id))];
    _thumbnailURL = collectionImage.thumbnail_image_url;
    _highResURL = collectionImage.image_url;
  }
  return self;
}

- (instancetype)initWithBackgroundColor:(UIColor*)backgroundColor {
  self = [super init];
  if (self) {
    _configurationID =
        [NSString stringWithFormat:@"%ld_%@",
                                   HomeCustomizationBackgroundPickerType::
                                       HomeCustomizationPickerTypeColor,
                                   backgroundColor.description];
    _backgroundColor = backgroundColor;
  }
  return self;
}

@end
