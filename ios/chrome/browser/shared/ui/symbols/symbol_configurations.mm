// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/symbols/symbol_configurations.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"

const CGFloat kSymbolActionPointSize = 18;

const CGFloat kColorfulBackgroundSymbolCornerRadius = 7;

const CGFloat kSymbolDownloadInfobarPointSize = 18;
const CGFloat kSymbolDownloadSmallInfobarPointSize = 13;

const CGFloat kInfobarSymbolPointSize = 18;

const CGFloat kSymbolAccessoryPointSize = 18;

const CGFloat kSettingsRootSymbolImagePointSize = 18;

const CGFloat kCloudSlashSymbolPointSize = 20;

NSArray<UIColor*>* SmallIncognitoPalette() {
  return @[
    [UIColor colorNamed:kGrey400Color], [UIColor colorNamed:kGrey100Color]
  ];
}

NSArray<UIColor*>* LargeIncognitoPalette() {
  return @[
    [UIColor colorNamed:kGrey100Color], [UIColor colorNamed:kGrey700Color]
  ];
}

UIColor* CloudSlashTintColor() {
  return [UIColor colorNamed:kTextTertiaryColor];
}
