// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/ui/constants.h"

NSString* const kReaderModeViewAccessibilityIdentifier =
    @"ReaderModeViewAccessibilityIdentifier";

NSString* const kReaderModeChipViewAccessibilityIdentifier =
    @"ReaderModeChipViewAccessibilityIdentifier";

NSString* const kReaderModeOptionsViewAccessibilityIdentifier =
    @"ReaderModeOptionsViewAccessibilityIdentifier";

NSString* const kReaderModeOptionsFontFamilyButtonAccessibilityIdentifier =
    @"ReaderModeOptionsFontFamilyButtonAccessibilityIdentifier";

NSString* const
    kReaderModeOptionsDecreaseFontSizeButtonAccessibilityIdentifier =
        @"ReaderModeOptionsDecreaseFontSizeButtonAccessibilityIdentifier";

NSString* const
    kReaderModeOptionsIncreaseFontSizeButtonAccessibilityIdentifier =
        @"ReaderModeOptionsIncreaseFontSizeButtonAccessibilityIdentifier";

NSString* const kReaderModeOptionsLightThemeButtonAccessibilityIdentifier =
    @"ReaderModeOptionsLightThemeButtonAccessibilityIdentifier";

NSString* const kReaderModeOptionsDarkThemeButtonAccessibilityIdentifier =
    @"ReaderModeOptionsDarkThemeButtonAccessibilityIdentifier";

NSString* const kReaderModeOptionsSepiaThemeButtonAccessibilityIdentifier =
    @"ReaderModeOptionsSepiaThemeButtonAccessibilityIdentifier";

NSString* const kReaderModeOptionsCloseButtonAccessibilityIdentifier =
    @"ReaderModeOptionsCloseButtonAccessibilityIdentifier";

NSString* const kReaderModeOptionsTurnOffButtonAccessibilityIdentifier =
    @"ReaderModeOptionsTurnOffButtonAccessibilityIdentifier";

UIColor* ReaderModeLightBackgroundColor() {
  return [UIColor whiteColor];
}

UIColor* ReaderModeLightTextColor() {
  return [UIColor colorWithRed:32 / 255.0
                         green:33 / 255.0
                          blue:36 / 255.0
                         alpha:1.0];
}

UIColor* ReaderModeDarkBackgroundColor() {
  return [UIColor colorWithRed:32 / 255.0
                         green:33 / 255.0
                          blue:36 / 255.0
                         alpha:1.0];
}

UIColor* ReaderModeDarkTextColor() {
  return [UIColor whiteColor];
}

UIColor* ReaderModeSepiaBackgroundColor() {
  return [UIColor colorWithRed:254 / 255.0
                         green:247 / 255.0
                          blue:224 / 255.0
                         alpha:1.0];
}

UIColor* ReaderModeSepiaTextColor() {
  return [UIColor colorWithRed:62 / 255.0
                         green:39 / 255.0
                          blue:35 / 255.0
                         alpha:1.0];
}
