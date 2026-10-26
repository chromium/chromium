// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_UI_WATERMARK_VIEW_UTILS_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_UI_WATERMARK_VIEW_UTILS_H_

#import <CoreGraphics/CoreGraphics.h>

#import <optional>

// Struct to hold style customization for the WatermarkView.
struct WatermarkStyle {
  std::optional<float> fill_opacity;
  std::optional<float> outline_opacity;
  std::optional<int> font_size;
};

// Calculates the expanded bounding size required to fully cover a viewport of
// `size` after rotating it by `angle` radians.
CGSize GetWatermarkExpandedSizeForRotation(CGSize size, CGFloat angle);

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_UI_WATERMARK_VIEW_UTILS_H_
