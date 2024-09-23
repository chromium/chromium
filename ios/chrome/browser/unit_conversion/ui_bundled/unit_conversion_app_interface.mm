// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/unit_conversion/ui_bundled/unit_conversion_app_interface.h"

#import "ios/chrome/browser/shared/public/commands/unit_conversion_commands.h"
#import "ios/chrome/test/app/chrome_test_util.h"

@implementation UnitConversionAppInterface

+ (void)presentUnitConversionFeature {
  id<UnitConversionCommands> handler =
      chrome_test_util::HandlerForActiveBrowser();
  [handler presentUnitConversionForSourceUnit:[NSUnitMass kilograms]
                              sourceUnitValue:20
                                     location:CGPointZero];
}

+ (void)stopPresentingUnitConversionFeature {
  id<UnitConversionCommands> handler =
      chrome_test_util::HandlerForActiveBrowser();
  [handler hideUnitConversion];
}
@end
