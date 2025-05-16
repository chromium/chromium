// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/coordinator/home_customization_background_preset_gallery_picker_mediator.h"

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/home_customization/model/background_collection_configuration.h"
#import "ios/chrome/browser/home_customization/model/background_customization_configuration.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_preset_gallery_picker_consumer.h"

@implementation HomeCustomizationBackgroundPresetGalleryPickerMediator

- (void)configureBackgroundConfigurations {
  NSMutableArray<BackgroundCollectionConfiguration*>* configurations =
      [NSMutableArray array];

  // Fake data
  BackgroundCollectionConfiguration* section1 =
      [[BackgroundCollectionConfiguration alloc] init];
  section1.collectionName = @"Section 1";
  [configurations addObject:section1];

  BackgroundCustomizationConfiguration* background1 =
      [[BackgroundCustomizationConfiguration alloc] init];
  background1.configurationID = @"config1";

  BackgroundCustomizationConfiguration* background2 =
      [[BackgroundCustomizationConfiguration alloc] init];
  background2.configurationID = @"config2";

  section1.configurations = @[ background1, background2 ];

  BackgroundCollectionConfiguration* section2 =
      [[BackgroundCollectionConfiguration alloc] init];
  section2.collectionName = @"Section 2";
  [configurations addObject:section2];

  BackgroundCustomizationConfiguration* background3 =
      [[BackgroundCustomizationConfiguration alloc] init];
  background3.configurationID = @"config3";

  BackgroundCustomizationConfiguration* background4 =
      [[BackgroundCustomizationConfiguration alloc] init];
  background4.configurationID = @"config4";

  BackgroundCustomizationConfiguration* background5 =
      [[BackgroundCustomizationConfiguration alloc] init];
  background5.configurationID = @"config5";

  BackgroundCustomizationConfiguration* background6 =
      [[BackgroundCustomizationConfiguration alloc] init];
  background6.configurationID = @"config6";

  BackgroundCustomizationConfiguration* background7 =
      [[BackgroundCustomizationConfiguration alloc] init];
  background7.configurationID = @"config7";

  BackgroundCustomizationConfiguration* background8 =
      [[BackgroundCustomizationConfiguration alloc] init];
  background8.configurationID = @"config8";

  section2.configurations = @[
    background3, background4, background5, background6, background7, background8
  ];

  NSString* selectedBackgroundId = @"config3";

  // TODO(crbug.com/408243803): fetch background customization
  // configurations and fill the `configurations` and
  // `selectedBackgroundId`.
  [_consumer setBackgroundCollectionConfigurations:configurations
                              selectedBackgroundId:selectedBackgroundId];
}

- (void)applyBackgroundForConfiguration:
    (BackgroundCustomizationConfiguration*)backgroundConfiguration {
  // TODO(crbug.com/408243803): apply NTP background configuration to NTP.
}

@end
