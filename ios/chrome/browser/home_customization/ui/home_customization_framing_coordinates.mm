// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_framing_coordinates.h"

#import "base/check.h"
#import "base/values.h"

namespace {
// Keys for Value serialization.
const char kXKey[] = "x";
const char kYKey[] = "y";
const char kWidthKey[] = "width";
const char kHeightKey[] = "height";
}  // namespace

@implementation HomeCustomizationFramingCoordinates

- (instancetype)initWithVisibleRect:(CGRect)visibleRect {
  self = [super init];
  if (self) {
    _visibleRect = visibleRect;
  }
  return self;
}

- (base::Value::Dict)toValue {
  base::Value::Dict dict;
  dict.Set(kXKey, self.visibleRect.origin.x);
  dict.Set(kYKey, self.visibleRect.origin.y);
  dict.Set(kWidthKey, self.visibleRect.size.width);
  dict.Set(kHeightKey, self.visibleRect.size.height);
  return dict;
}

+ (instancetype)fromValue:(const base::Value::Dict&)dict {
  std::optional<double> x = dict.FindDouble(kXKey);
  std::optional<double> y = dict.FindDouble(kYKey);
  std::optional<double> width = dict.FindDouble(kWidthKey);
  std::optional<double> height = dict.FindDouble(kHeightKey);

  if (!x.has_value() || !y.has_value() || !width.has_value() ||
      !height.has_value()) {
    return nil;
  }

  CGRect visibleRect =
      CGRectMake(x.value(), y.value(), width.value(), height.value());
  return [[self alloc] initWithVisibleRect:visibleRect];
}

- (id)copyWithZone:(NSZone*)zone {
  return [[HomeCustomizationFramingCoordinates alloc]
      initWithVisibleRect:self.visibleRect];
}

@end
