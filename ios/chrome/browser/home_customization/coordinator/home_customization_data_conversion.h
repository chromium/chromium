// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_DATA_CONVERSION_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_DATA_CONVERSION_H_

#import <UIKit/UIKit.h>

#import <string_view>

namespace base {
class DictValue;
}  // namespace base

struct FramingCoordinates;
@class HomeCustomizationFramingCoordinates;

// JSON dictionary keys for light and dark mode colors inside an animation color
// mapping dictionary.
inline constexpr std::string_view kEphemeralThemeLightModeColorsKey = "light";
inline constexpr std::string_view kEphemeralThemeDarkModeColorsKey = "dark";

// Converts a `HomeCustomizationFramingCoordinates` to a `FramingCoordinates`;
FramingCoordinates FramingCoordinatesFromHomeCustomizationFramingCoordinates(
    HomeCustomizationFramingCoordinates* coordinates);

// Converts a `FramingCoordinates` to a `HomeCustomizationFramingCoordinates`.
HomeCustomizationFramingCoordinates*
HomeCustomizationFramingCoordinatesFromFramingCoordinates(
    const FramingCoordinates& coordinates);

// Converts a `base::DictValue` mapping keypath strings to hex color strings
// (e.g. "#1A73E8" or "1A73E8") into an `NSDictionary<NSString*, UIColor*>*`.
// Returns nil if `dict` is null or contains no valid hex colors.
NSDictionary<NSString*, UIColor*>* ColorProviderDictionaryFromDict(
    const base::DictValue* dict);

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_DATA_CONVERSION_H_
