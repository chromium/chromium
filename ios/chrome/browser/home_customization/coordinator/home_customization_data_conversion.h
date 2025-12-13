// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_DATA_CONVERSION_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_DATA_CONVERSION_H_

struct FramingCoordinates;
@class HomeCustomizationFramingCoordinates;

// Converts a `HomeCustomizationFramingCoordinates` to a `FramingCoordinates`;
FramingCoordinates FramingCoordinatesFromHomeCustomizationFramingCoordinates(
    HomeCustomizationFramingCoordinates* coordinates);

// Converts a `FramingCoordinates` to a `HomeCustomizationFramingCoordinates`.
HomeCustomizationFramingCoordinates*
HomeCustomizationFramingCoordinatesFromFramingCoordinates(
    const FramingCoordinates& coordinates);

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_DATA_CONVERSION_H_
