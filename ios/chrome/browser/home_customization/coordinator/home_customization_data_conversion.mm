// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/coordinator/home_customization_data_conversion.h"

#import "base/strings/string_number_conversions.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "ios/chrome/browser/home_customization/model/home_background_data.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_framing_coordinates.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"

namespace {

// Parses a hex color string (e.g. "#1A73E8" or "1A73E8") into a `UIColor*`.
// Returns nil if the string is not a valid 6-digit hex color.
UIColor* ColorFromHexString(std::string_view hex_string) {
  std::string_view trimmed =
      base::TrimString(hex_string, "#", base::TRIM_LEADING);
  if (trimmed.length() != 6) {
    return nil;
  }
  uint32_t rgb_value = 0;
  if (!base::HexStringToUInt(trimmed, &rgb_value)) {
    return nil;
  }
  return UIColorFromRGB(rgb_value);
}

}  // namespace

FramingCoordinates FramingCoordinatesFromHomeCustomizationFramingCoordinates(
    HomeCustomizationFramingCoordinates* coordinates) {
  CGRect rect = coordinates.visibleRect;
  return FramingCoordinates(rect.origin.x, rect.origin.y, rect.size.width,
                            rect.size.height);
}

HomeCustomizationFramingCoordinates*
HomeCustomizationFramingCoordinatesFromFramingCoordinates(
    const FramingCoordinates& coordinates) {
  CGRect visibleRect = CGRectMake(coordinates.x, coordinates.y,
                                  coordinates.width, coordinates.height);
  return [[HomeCustomizationFramingCoordinates alloc]
      initWithVisibleRect:visibleRect];
}

NSDictionary<NSString*, UIColor*>* ColorProviderDictionaryFromDict(
    const base::DictValue* dict) {
  if (!dict) {
    return nil;
  }
  NSMutableDictionary<NSString*, UIColor*>* result =
      [[NSMutableDictionary alloc] initWithCapacity:dict->size()];
  for (const auto [key, value] : *dict) {
    if (!value.is_string()) {
      continue;
    }
    UIColor* color = ColorFromHexString(value.GetString());
    if (color) {
      result[base::SysUTF8ToNSString(key)] = color;
    }
  }
  return result.count > 0 ? [result copy] : nil;
}
