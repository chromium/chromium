// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/icons/settings_icon.h"

#import "ios/chrome/browser/ui/icons/chrome_symbol.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const CGFloat kSettingsRootSymbolImagePointSize = 18;

// Custom symbol names.
NSString* const kSyncDisabledSymbol = @"arrow_triangle_slash_circlepath";

// Default symbol names.
NSString* const kSyncErrorSymbol =
    @"exclamationmark.arrow.triangle.2.circlepath";
NSString* const kSyncEnabledSymbol = @"arrow.triangle.2.circlepath";
NSString* const kDefaultBrowserSymbol = @"app.badge.checkmark";
NSString* const kPrivacySecuritySymbol = @"checkerboard.shield";

namespace {

// The default configuration with the given `pointSize` for the Settings root
// screen.
UIImageSymbolConfiguration* kDefaultSettingsRootSymbolConfiguration =
    [UIImageSymbolConfiguration
        configurationWithPointSize:kSettingsRootSymbolImagePointSize
                            weight:UIImageSymbolWeightRegular
                             scale:UIImageSymbolScaleMedium];

}  // namespace

UIImage* DefaultSettingsRootSymbol(NSString* symbol_name) {
  return DefaultSymbolWithConfiguration(
      symbol_name, kDefaultSettingsRootSymbolConfiguration);
}

UIImage* CustomSettingsRootSymbol(NSString* symbol_name) {
  return CustomSymbolWithConfiguration(symbol_name,
                                       kDefaultSettingsRootSymbolConfiguration);
}
