// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar_container/toolbar_height_range.h"

#import <algorithm>

#import "ios/chrome/common/ui/util/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace toolbar_container {

HeightRange::HeightRange(CGFloat min_height, CGFloat max_height)
    : min_height_(min_height), max_height_(max_height) {}

CGFloat HeightRange::GetInterpolatedHeight(CGFloat progress) const {
  progress = std::min(static_cast<CGFloat>(1.0), progress);
  progress = std::max(static_cast<CGFloat>(0.0), progress);
  return min_height() + progress * delta();
}

bool HeightRange::operator==(const HeightRange& other) const {
  return AreCGFloatsEqual(min_height(), other.min_height()) &&
         AreCGFloatsEqual(max_height(), other.max_height());
}

bool HeightRange::operator!=(const HeightRange& other) const {
  return !(*this == other);
}

HeightRange HeightRange::operator+(const HeightRange& other) const {
  return HeightRange(min_height() + other.min_height(),
                     max_height() + other.max_height());
}

HeightRange HeightRange::operator-(const HeightRange& other) const {
  return HeightRange(min_height() - other.min_height(),
                     max_height() - other.max_height());
}

HeightRange& HeightRange::operator+=(const HeightRange& other) {
  min_height_ += other.min_height();
  max_height_ += other.max_height();
  return *this;
}

HeightRange& HeightRange::operator-=(const HeightRange& other) {
  min_height_ -= other.min_height();
  max_height_ -= other.max_height();
  return *this;
}

}  // namespace toolbar_container
