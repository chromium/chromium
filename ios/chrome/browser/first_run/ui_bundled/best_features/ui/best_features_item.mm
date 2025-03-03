// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/best_features/ui/best_features_item.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@implementation BestFeaturesItem

+ (BestFeaturesItem*)itemForType:(BestFeaturesItemType)itemType {
  BestFeaturesItem* item = [[BestFeaturesItem alloc] init];
  // TODO(crbug.com/396480750): Set the properties for the item.
  return item;
}

@end
