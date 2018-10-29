// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_CONTAINER_TOOLBAR_HEIGHT_RANGE_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_CONTAINER_TOOLBAR_HEIGHT_RANGE_H_

#import <QuartzCore/QuartzCore.h>

namespace toolbar_container {

// A simple container object used to store the height range for a collapsible
// toolbar.
class HeightRange {
 public:
  HeightRange() = default;
  HeightRange(CGFloat min_height, CGFloat max_height);

  // The max and min heights.
  CGFloat min_height() const { return min_height_; }
  CGFloat max_height() const { return max_height_; }

  // Returns the delta between the max and min height.
  CGFloat delta() const { return max_height_ - min_height_; }

  // Returns the height value at the given interpolation value.
  CGFloat GetInterpolatedHeight(CGFloat progress) const;

  // Operators.
  bool operator==(const HeightRange& other) const;
  bool operator!=(const HeightRange& other) const;
  HeightRange operator+(const HeightRange& other) const;
  HeightRange operator-(const HeightRange& other) const;
  HeightRange& operator+=(const HeightRange& other);
  HeightRange& operator-=(const HeightRange& other);

 private:
  // The min and max heights.
  CGFloat min_height_ = 0.0;
  CGFloat max_height_ = 0.0;
};

}  // namespace toolbar_container

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_CONTAINER_TOOLBAR_HEIGHT_RANGE_H_
