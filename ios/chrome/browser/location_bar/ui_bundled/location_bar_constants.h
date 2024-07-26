// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_CONSTANTS_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_CONSTANTS_H_

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

// The brightness of the location bar placeholder text in regular mode,
// on an iPhone.
extern const CGFloat kiPhoneLocationBarPlaceholderColorBrightness;

// FullScreen progress threshold in which to toggle between full screen on and
// off mode for the leading views in the location bar.
extern const double kFullscreenProgressThreshold;

// Last button in accessory view for keyboard, commonly used TLD.
extern NSString* const kDotComTLD;

// Accessibility identifier of the share button.
extern NSString* const kOmniboxShareButtonIdentifier;

// Accessibility identifier of the voice search button.
extern NSString* const kOmniboxVoiceSearchButtonIdentifier;

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_CONSTANTS_H_
