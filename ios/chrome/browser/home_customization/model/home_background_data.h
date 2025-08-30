// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_BACKGROUND_DATA_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_BACKGROUND_DATA_H_

#include <optional>

#include "base/values.h"
#import "components/sync/protocol/theme_types.pb.h"

struct HomeUserUploadedBackground;

// C++ representation of framing coordinates for background images.
// This struct is persisted to disk via prefs. When adding new fields,
// ensure backward compatibility by providing defaults in FromDict().
struct FramingCoordinates {
  // Visible rectangle coordinates in original image space.
  double x;
  double y;
  double width;
  double height;

  // Default constructor.
  FramingCoordinates() = default;

  // Constructor with values.
  FramingCoordinates(double x_val,
                     double y_val,
                     double width_val,
                     double height_val);

  // Creates FramingCoordinates from a base::Value::Dict.
  static std::optional<FramingCoordinates> FromDict(
      const base::Value::Dict& dict);

  // Converts to base::Value::Dict for serialization.
  base::Value::Dict ToDict() const;

  // Equality operator.
  friend bool operator==(const FramingCoordinates&,
                         const FramingCoordinates&) = default;
};

// This struct is persisted to disk via prefs. When adding new fields,
// ensure backward compatibility by providing defaults in FromDict().
struct HomeUserUploadedBackground {
  std::string image_path;
  FramingCoordinates framing_coordinates;

  // Creates HomeUserUploadedBackground from a base::Value::Dict.
  static std::optional<HomeUserUploadedBackground> FromDict(
      const base::Value::Dict& dict);

  // Converts to base::Value::Dict for serialization.
  base::Value::Dict ToDict() const;

  // Equality operator.
  friend bool operator==(const HomeUserUploadedBackground&,
                         const HomeUserUploadedBackground&) = default;
};

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_BACKGROUND_DATA_H_
