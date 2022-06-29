// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/icons/settings_icon.h"

#import "ios/chrome/browser/ui/icons/chrome_symbol.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSInteger kSettingsRootSymbolImagePointSize = 18;

namespace {

// The default configuration with the given `pointSize` for the Settings root
// screen.
UIImageSymbolConfiguration* kDefaultSettingsRootSymbolConfiguration =
    [UIImageSymbolConfiguration
        configurationWithPointSize:kSettingsRootSymbolImagePointSize
                            weight:UIImageSymbolWeightRegular
                             scale:UIImageSymbolScaleMedium];

}  // namespace

UIImage* DefaultSettingsRootSymbol(NSString* symbolName) {
  return DefaultSymbolWithConfiguration(
      symbolName, kDefaultSettingsRootSymbolConfiguration);
}

UIImage* CustomSettingsRootSymbol(NSString* symbolName) {
  return CustomSymbolWithConfiguration(symbolName,
                                       kDefaultSettingsRootSymbolConfiguration);
}
