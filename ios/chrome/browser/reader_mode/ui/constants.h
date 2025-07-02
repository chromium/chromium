// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_UI_CONSTANTS_H_
#define IOS_CHROME_BROWSER_READER_MODE_UI_CONSTANTS_H_

#import <UIKit/UIKit.h>

#import <vector>

// The accessibility identifier of the Reader Mode content view.
extern NSString* const kReaderModeViewAccessibilityIdentifier;

// The accessibility identifier of the Reader Mode chip view.
extern NSString* const kReaderModeChipViewAccessibilityIdentifier;

// The accessibility identifier of the Reader Mode options view.
extern NSString* const kReaderModeOptionsViewAccessibilityIdentifier;

// The accessibility identifier for the font family button.
extern NSString* const
    kReaderModeOptionsFontFamilyButtonAccessibilityIdentifier;

// Reader mode color themes helpers.
UIColor* ReaderModeLightBackgroundColor();
UIColor* ReaderModeLightTextColor();
UIColor* ReaderModeDarkBackgroundColor();
UIColor* ReaderModeDarkTextColor();
UIColor* ReaderModeSepiaBackgroundColor();
UIColor* ReaderModeSepiaTextColor();

// Reader mode font scale multipliers.
std::vector<CGFloat> ReaderModeFontScaleMultipliers();

#endif  // IOS_CHROME_BROWSER_READER_MODE_UI_CONSTANTS_H_
