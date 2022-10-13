// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/icons/settings_icon.h"

#import "ios/chrome/browser/ui/icons/chrome_symbol.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const CGFloat kSettingsRootSymbolImagePointSize = 18;

// Custom symbol names.
NSString* const kPrivacySymbol = @"checkerboard_shield";
NSString* const kSyncDisabledSymbol = @"arrow_triangle_slash_circlepath";
NSString* const kSafetyCheckSymbol = @"checkermark_shield";
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
NSString* const kGoogleIconSymbol = @"google_icon";
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)

// Default symbol names.
NSString* const kSyncEnabledSymbol = @"arrow.triangle.2.circlepath";
NSString* const kDefaultBrowserSymbol = @"app.badge.checkmark";
NSString* const kDiscoverSymbol = @"flame";
NSString* const kBellSymbol = @"bell";

UIImage* DefaultSettingsRootSymbol(NSString* symbol_name) {
  return DefaultSymbolWithPointSize(symbol_name,
                                    kSettingsRootSymbolImagePointSize);
}

UIImage* CustomSettingsRootSymbol(NSString* symbol_name) {
  return CustomSymbolWithPointSize(symbol_name,
                                   kSettingsRootSymbolImagePointSize);
}

UIImage* CustomSettingsRootMulticolorSymbol(NSString* symbol_name) {
  return CustomMulticolorSymbol(symbol_name, kSettingsRootSymbolImagePointSize);
}
