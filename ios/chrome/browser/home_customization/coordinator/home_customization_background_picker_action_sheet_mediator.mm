// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/coordinator/home_customization_background_picker_action_sheet_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/sync/protocol/theme_types.pb.h"
#import "ios/chrome/browser/home_customization/coordinator/background_customization_configuration_item.h"
#import "ios/chrome/browser/home_customization/coordinator/home_customization_data_conversion.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service_observer_bridge.h"
#import "ios/chrome/browser/home_customization/model/home_background_data.h"
#import "ios/chrome/browser/home_customization/ui/background_customization_configuration.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_photo_framing_coordinates.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_picker_action_sheet_consumer.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_picker_action_sheet_presentation_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/theme_utils.h"
#import "skia/ext/skia_utils_ios.h"

@interface HomeCustomizationBackgroundPickerActionSheetMediator () <
    HomeBackgroundCustomizationServiceObserving>
@end

@implementation HomeCustomizationBackgroundPickerActionSheetMediator {
  raw_ptr<HomeBackgroundCustomizationService>
      _homeBackgroundCustomizationService;

  // Observer for the customization service.
  std::unique_ptr<HomeBackgroundCustomizationServiceObserverBridge>
      _backgroundCustomizationServiceObserverBridge;
}

- (instancetype)initWithHomeBackgroundCustomizationService:
    (HomeBackgroundCustomizationService*)homeBackgroundCustomizationService {
  self = [super init];
  if (self) {
    _homeBackgroundCustomizationService = homeBackgroundCustomizationService;
    _backgroundCustomizationServiceObserverBridge =
        std::make_unique<HomeBackgroundCustomizationServiceObserverBridge>(
            _homeBackgroundCustomizationService, self);
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
      [self applyDefaultBackground];
      break;
    default:
      NOTREACHED();
  }
}

#pragma mark - HomeBackgroundCustomizationServiceObserving

- (void)onBackgroundChanged {
  if (self.consumer.navigationItem.leftBarButtonItem) {
    return;
  }

  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(discardBackground)];

  UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(confirmBackground)];

  self.consumer.navigationItem.leftBarButtonItem = cancelButton;
  self.consumer.navigationItem.rightBarButtonItem = doneButton;
}

#pragma mark - Private

// Applies the user-uploaded photo background to the NTP.
- (void)applyUserUploadedBackground:
    (BackgroundCustomizationConfigurationItem*)configurationItem {
  FramingCoordinates coordinates =
      FramingCoordinatesFromHomeCustomizationFramingCoordinates(
          configurationItem.userUploadedFramingCoordinates);
  _homeBackgroundCustomizationService->SetCurrentUserUploadedBackground(
      base::SysNSStringToUTF8(configurationItem.userUploadedImagePath),
      coordinates);
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

  if (!configurationItem.backgroundColor) {
    [self applyDefaultBackground];
    return;
  }

  _homeBackgroundCustomizationService->SetBackgroundColor(
      skia::UIColorToSkColor(configurationItem.backgroundColor),
      SchemeVariantToProtoEnum(configurationItem.colorVariant));
}

- (void)applyDefaultBackground {
  _homeBackgroundCustomizationService->ClearCurrentBackground();
}

// Discards customization changes and dismiss the menu.
- (void)discardBackground {
  _homeBackgroundCustomizationService->RestoreCurrentTheme();
  [self.delegate backgroundPickerActionSheetDidRequestDismissal];
}

// Saves customization changes and dismiss the menu.
- (void)confirmBackground {
  _homeBackgroundCustomizationService->StoreCurrentTheme();
  [self.delegate backgroundPickerActionSheetDidRequestDismissal];
}

@end
