// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/home_customization/model/framing_coordinates.h"

#include "base/logging.h"

namespace {
// Keys for dictionary serialization
const char kXKey[] = "x";
const char kYKey[] = "y";
const char kWidthKey[] = "width";
const char kHeightKey[] = "height";
}  // namespace

FramingCoordinates::FramingCoordinates(double x_val,
                                       double y_val,
                                       double width_val,
                                       double height_val)
    : x(x_val), y(y_val), width(width_val), height(height_val) {}

std::optional<FramingCoordinates> FramingCoordinates::FromDict(
    const base::Value::Dict& dict) {
  std::optional<double> x_val = dict.FindDouble(kXKey);
  std::optional<double> y_val = dict.FindDouble(kYKey);
  std::optional<double> width_val = dict.FindDouble(kWidthKey);
  std::optional<double> height_val = dict.FindDouble(kHeightKey);

  if (!x_val || !y_val || !width_val || !height_val) {
    return std::nullopt;
  }

  return FramingCoordinates(*x_val, *y_val, *width_val, *height_val);
}

base::Value::Dict FramingCoordinates::ToDict() const {
  base::Value::Dict dict;
  dict.Set(kXKey, x);
  dict.Set(kYKey, y);
  dict.Set(kWidthKey, width);
  dict.Set(kHeightKey, height);
  return dict;
}

bool FramingCoordinates::operator==(const FramingCoordinates& other) const {
  return x == other.x && y == other.y && width == other.width &&
         height == other.height;
}

bool FramingCoordinates::operator!=(const FramingCoordinates& other) const {
  return !(*this == other);
}
