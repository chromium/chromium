// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/colors/dynamic_color_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace color {

UIColor* DarkModeDynamicColor(UIColor* dynamicColor,
                              BOOL forceDark,
                              UIColor* darkColor) {
  if (@available(iOS 13, *)) {
    return dynamicColor;
  }
  return forceDark ? darkColor : dynamicColor;
}

}  // namespace color
