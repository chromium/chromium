// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_UI_CONSTANTS_H_
#define IOS_CHROME_BROWSER_READER_MODE_UI_CONSTANTS_H_

#import <UIKit/UIKit.h>

// The accessibility identifier of the Reader Mode content view.
extern NSString* const kReaderModeViewAccessibilityIdentifier;

// The accessibility identifier of the Reader Mode chip view.
extern NSString* const kReaderModeChipViewAccessibilityIdentifier;

// The accessibility identifier of the Reader Mode options view.
extern NSString* const kReaderModeOptionsViewAccessibilityIdentifier;

// The accessibility identifier for the font family button.
extern NSString* const
    kReaderModeOptionsFontFamilyButtonAccessibilityIdentifier;

// The accessibility identifier for the decrease font size button.
extern NSString* const
    kReaderModeOptionsDecreaseFontSizeButtonAccessibilityIdentifier;

// The accessibility identifier for the increase font size button.
extern NSString* const
    kReaderModeOptionsIncreaseFontSizeButtonAccessibilityIdentifier;

// The accessibility identifier for the light theme button.
extern NSString* const
    kReaderModeOptionsLightThemeButtonAccessibilityIdentifier;

// The accessibility identifier for the dark theme button.
extern NSString* const kReaderModeOptionsDarkThemeButtonAccessibilityIdentifier;

// The accessibility identifier for the sepia theme button.
extern NSString* const
    kReaderModeOptionsSepiaThemeButtonAccessibilityIdentifier;

// The accessibility identifier for the close button.
extern NSString* const kReaderModeOptionsCloseButtonAccessibilityIdentifier;

// The accessibility identifier for the turn off button.
extern NSString* const kReaderModeOptionsTurnOffButtonAccessibilityIdentifier;

// Reader mode color themes helpers.
UIColor* ReaderModeLightBackgroundColor();
UIColor* ReaderModeLightTextColor();
UIColor* ReaderModeDarkBackgroundColor();
UIColor* ReaderModeDarkTextColor();
UIColor* ReaderModeSepiaBackgroundColor();
UIColor* ReaderModeSepiaTextColor();

#endif  // IOS_CHROME_BROWSER_READER_MODE_UI_CONSTANTS_H_
