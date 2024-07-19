// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/utils/home_customization_helper.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"

@implementation HomeCustomizationHelper

+ (NSString*)titleForToggleType:(CustomizationToggleType)type {
  switch (type) {
    case CustomizationToggleType::kMostVisited:
      return @"Test title 1 (Most visited)";
    case CustomizationToggleType::kMagicStack:
      return @"Test title 2 (Magic Stack)";
    case CustomizationToggleType::kDiscover:
      return @"Test title 3 (Discover)";
  }
}

+ (NSString*)subtitleForToggleType:(CustomizationToggleType)type {
  switch (type) {
    case CustomizationToggleType::kMostVisited:
      return @"Test subtitle 1";
    case CustomizationToggleType::kMagicStack:
      return @"Test subtitle 2";
    case CustomizationToggleType::kDiscover:
      return @"Test subtitle 3";
  }
}

+ (UIImage*)iconForToggleType:(CustomizationToggleType)type {
  switch (type) {
    case CustomizationToggleType::kMostVisited:
      return DefaultSymbolWithPointSize(kHistorySymbol, kToggleIconPointSize);
    case CustomizationToggleType::kMagicStack:
      return DefaultSymbolWithPointSize(kMagicStackSymbol,
                                        kToggleIconPointSize);
    case CustomizationToggleType::kDiscover:
      return DefaultSymbolWithPointSize(kDiscoverFeedSymbol,
                                        kToggleIconPointSize);
  }
}

+ (NSString*)accessibilityIdentifierForToggleType:
    (CustomizationToggleType)type {
  switch (type) {
    case CustomizationToggleType::kMostVisited:
      return kCustomizationToggleMostVisitedIdentifier;
    case CustomizationToggleType::kMagicStack:
      return kCustomizationToggleMagicStackIdentifier;
    case CustomizationToggleType::kDiscover:
      return kCustomizationToggleDiscoverIdentifier;
  }
}

@end
