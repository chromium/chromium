// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/coordinator/home_customization_background_picker_action_sheet_mediator.h"

#import "ios/chrome/browser/home_customization/model/background_customization_configuration.h"
#import "ios/chrome/browser/home_customization/model/background_customization_configuration_item.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service.h"
#import "ios/chrome/browser/ntp/ui_bundled/theme_utils.h"
#import "skia/ext/skia_utils_ios.h"

@implementation HomeCustomizationBackgroundPickerActionSheetMediator {
  raw_ptr<HomeBackgroundCustomizationService>
      _homeBackgroundCustomizationService;
}

- (instancetype)initWithHomeBackgroundCustomizationService:
    (HomeBackgroundCustomizationService*)homeBackgroundCustomizationService {
  self = [super init];
  if (self) {
    _homeBackgroundCustomizationService = homeBackgroundCustomizationService;
  }
  return self;
}

- (void)applyBackgroundForConfiguration:
    (id<BackgroundCustomizationConfiguration>)backgroundConfiguration {
  if (![backgroundConfiguration
          isKindOfClass:[BackgroundCustomizationConfigurationItem class]]) {
    return;
  }

  BackgroundCustomizationConfigurationItem* configurationItem =
      static_cast<BackgroundCustomizationConfigurationItem*>(
          backgroundConfiguration);
  switch (configurationItem.backgroundStyle) {
    case HomeCustomizationBackgroundStyle::kUserUploaded:
      [self applyUserUploadedBackground:configurationItem];
      break;
    case HomeCustomizationBackgroundStyle::kPreset:
      [self
          applyPresetGalleryBackgroundForCollectionImage:configurationItem
                                                             .collectionImage];
      break;
    case HomeCustomizationBackgroundStyle::kColor:
      [self applyBackgroundColor:backgroundConfiguration];
      break;
    case HomeCustomizationBackgroundStyle::kDefault:
      break;
    default:
      NOTREACHED();
  }
}

- (void)addBackgroundToRecentlyUsed:
    (id<BackgroundCustomizationConfiguration>)backgroundConfiguration {
  // TODO(crbug.com/408243803): Add the selected background configuration to the
  // recently used list.
}

#pragma mark - Private

// Applies the user-uploaded photo background to the NTP.
- (void)applyUserUploadedBackground:
    (BackgroundCustomizationConfigurationItem*)configurationItem {
  // TODO(crbug.com/427973907): Add applyUserUploadedBackground Implementation.
}

// Applies the preset gallery background for the given collection image.
- (void)applyPresetGalleryBackgroundForCollectionImage:
    (const CollectionImage&)collectionImage {
  std::string attribution_line_1;
  std::string attribution_line_2;
  // Set attribution lines if available.
  if (!collectionImage.attribution.empty()) {
    attribution_line_1 = collectionImage.attribution[0];
    if (collectionImage.attribution.size() > 1) {
      attribution_line_2 = collectionImage.attribution[1];
    }
  }

  _homeBackgroundCustomizationService->SetCurrentBackground(
      collectionImage.image_url, collectionImage.thumbnail_image_url,
      attribution_line_1, attribution_line_2,
      collectionImage.attribution_action_url, collectionImage.collection_id);
}

// Applies a background color to the NTP.
- (void)applyBackgroundColor:
    (id<BackgroundCustomizationConfiguration>)backgroundConfiguration {
  if (![backgroundConfiguration
          isKindOfClass:[BackgroundCustomizationConfigurationItem class]]) {
    // Only `BackgroundCustomizationConfigurationItem` exposes required
    // fields. Other implementations may not support expected properties.
    return;
  }

  BackgroundCustomizationConfigurationItem* configurationItem =
      static_cast<BackgroundCustomizationConfigurationItem*>(
          backgroundConfiguration);

  _homeBackgroundCustomizationService->SetBackgroundColor(
      skia::UIColorToSkColor(configurationItem.backgroundColor),
      SchemeVariantToProtoEnum(configurationItem.colorVariant));
}

@end
