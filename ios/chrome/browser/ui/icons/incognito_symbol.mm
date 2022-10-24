// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/icons/incognito_symbol.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Custom symbol names.
NSString* const kIncognitoSymbol = @"incognito";
NSString* const kIncognitoCircleFilliOS14Symbol =
    @"incognito_circle_fill_ios14";

// Custom symbol names which can be configured a "palette".
NSString* const kIncognitoCircleFillSymbol = @"incognito_circle_fill";

NSArray<UIColor*>* SmallIncognitoColorsPalette() {
  return @[
    [UIColor colorNamed:kGrey400Color], [UIColor colorNamed:kGrey100Color]
  ];
}

NSArray<UIColor*>* LargeIncognitoColorsPalette() {
  return @[
    [UIColor colorNamed:kGrey100Color], [UIColor colorNamed:kGrey700Color]
  ];
}
