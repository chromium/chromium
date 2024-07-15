// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/coordinator/home_customization_mediator.h"

#import "ios/chrome/browser/home_customization/ui/home_customization_main_consumer.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"

@implementation HomeCustomizationMediator

#pragma mark - Public

- (void)configureMainPageData {
  std::vector<CustomizationToggleType> types = {
      CustomizationToggleType::kShortcuts,
      CustomizationToggleType::kMagicStack,
      CustomizationToggleType::kDiscover,
  };
  [self.mainPageConsumer populateTogglesWithTypes:types];
}

@end
