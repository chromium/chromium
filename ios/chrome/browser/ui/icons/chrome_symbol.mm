// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/icons/chrome_symbol.h"

#include "base/check.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Returns the default configuration with the given |pointSize|.
UIImageConfiguration* DefaultSymbolConfigurationWithPointSize(
    NSInteger pointSize) {
  return [UIImageSymbolConfiguration
      configurationWithPointSize:pointSize
                          weight:UIImageSymbolWeightMedium
                           scale:UIImageSymbolScaleMedium];
}

// Returns a symbol named |symbolName| configured with the given
// |configuration|. |systemSymbol| is used to specify if it is a SFSymbol or a
// custom symbol.

UIImage* SymbolWithConfiguration(NSString* symbolName,
                                 UIImageConfiguration* configuration,
                                 BOOL systemSymbol) {
  UIImage* symbol;
  if (systemSymbol) {
    symbol = [UIImage systemImageNamed:symbolName
                     withConfiguration:configuration];
  } else {
    symbol = [UIImage imageNamed:symbolName
                        inBundle:nil
               withConfiguration:configuration];
  }
  DCHECK(symbol);
  return symbol;
}

}  // namespace

// Custom symbol names.
NSString* const kArrowClockWiseSymbol = @"arrow_clockwise";
NSString* const kIncognitoSymbol = @"incognito";
NSString* const kIncognitoCircleFillSymbol = @"incognito_circle_fill";
NSString* const kSquareNumberSymbol = @"square_number";
NSString* const kTranslateSymbol = @"translate";
NSString* const kCameraSymbol = @"camera";
NSString* const kCameraFillSymbol = @"camera_fill";
NSString* const kPlusCircleFillSymbol = @"plus_circle_fill";
NSString* const kArrowTriangleSlashCirclePathSymbol =
    @"arrow_triangle_slash_circlepath";
NSString* const kPopupBadgeMinusSymbol = @"popup_badge_minus";

// Default symbol names.
NSString* const kCreditCardSymbol = @"creditcard";
NSString* const kMicrophoneFillSymbol = @"mic.fill";
NSString* const kMicrophoneSymbol = @"mic";
NSString* const kEllipsisCircleFillSymbol = @"ellipsis.circle.fill";
NSString* const kPinSymbol = @"pin";
NSString* const kPinFillSymbol = @"pin.fill";
NSString* const kGearShapeSymbol = @"gearshape.fill";
NSString* const kShareSymbol = @"square.and.arrow.up";
NSString* const kXMarkSymbol = @"xmark";
NSString* const kPlusSymbol = @"plus";
NSString* const kSearchSymbol = @"magnifyingglass";
NSString* const kCheckmarkSymbol = @"checkmark";
NSString* const kArrowDownCircleFillSymbol = @"arrow.down.circle.fill";
NSString* const kWarningSymbol = @"exclamationmark.triangle";
NSString* const kWarningFillSymbol = @"exclamationmark.triangle.fill";
NSString* const kHelpFillSymbol = @"questionmark.circle";
NSString* const kCheckMarkCircleSymbol = @"checkmark.circle";
NSString* const kCheckMarkCircleFillSymbol = @"checkmark.circle.fill";
NSString* const kFailMarkCircleFillSymbol = @"exclamationmark.circle.fill";
NSString* const kTrashSymbol = @"trash";
NSString* const kInfoCircleSymbol = @"info.circle";

UIImage* DefaultSymbolWithConfiguration(NSString* symbolName,
                                        UIImageConfiguration* configuration) {
  return SymbolWithConfiguration(symbolName, configuration, true);
}

UIImage* CustomSymbolWithConfiguration(NSString* symbolName,
                                       UIImageConfiguration* configuration) {
  return SymbolWithConfiguration(symbolName, configuration, false);
}

UIImage* DefaultSymbolWithPointSize(NSString* symbolName, NSInteger pointSize) {
  return DefaultSymbolWithConfiguration(
      symbolName, DefaultSymbolConfigurationWithPointSize(pointSize));
}

UIImage* CustomSymbolWithPointSize(NSString* symbolName, NSInteger pointSize) {
  return CustomSymbolWithConfiguration(
      symbolName, DefaultSymbolConfigurationWithPointSize(pointSize));
}

UIImage* DefaultSymbolTemplateWithPointSize(NSString* symbolName,
                                            NSInteger pointSize) {
  return [DefaultSymbolWithPointSize(symbolName, pointSize)
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
}

UIImage* CustomSymbolTemplateWithPointSize(NSString* symbolName,
                                           NSInteger pointSize) {
  return [CustomSymbolWithPointSize(symbolName, pointSize)
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
}

bool UseSymbols() {
  return base::FeatureList::IsEnabled(kUseSFSymbols);
}
