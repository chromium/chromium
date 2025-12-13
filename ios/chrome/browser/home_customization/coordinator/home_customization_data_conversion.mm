// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/coordinator/home_customization_data_conversion.h"

#import "ios/chrome/browser/home_customization/model/home_background_data.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_framing_coordinates.h"

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
