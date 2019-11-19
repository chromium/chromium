// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_COLORS_DYNAMIC_COLOR_UTIL_H_
#define IOS_CHROME_COMMON_COLORS_DYNAMIC_COLOR_UTIL_H_

#import <UIKit/UIKit.h>

namespace color {

// Returns a color that will correctly display on both iOS 12 and iOS 13 on a
// page that uses forced dark mode to style itself (e.g. incognito pages).
// Because dark mode is not supported on <= 12, on those versions, the color
// must be manually set to a dark-specific color, instead of using the dynamic
// color.
// Thus, on iOS 13, |dynamicColor| will always be used. On iOS <= 12,
// |dynamicColor| will be used if |forceDark| is false (and will always display
// its light variant), and |darkColor| will be used otherwise.
UIColor* DarkModeDynamicColor(UIColor* dynamicColor,
                              BOOL forceDark,
                              UIColor* darkColor);

}  // namespace color

#endif  // IOS_CHROME_COMMON_COLORS_DYNAMIC_COLOR_UTIL_H_
