// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_UI_WATERMARK_CONSUMER_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_UI_WATERMARK_CONSUMER_H_

#import <Foundation/Foundation.h>

#import <optional>

// Struct to hold style customization for the WatermarkView.
struct WatermarkStyle {
  std::optional<float> fill_opacity;
  std::optional<float> outline_opacity;
  std::optional<int> font_size;
};

// Consumer protocol for the watermark UI to receive style and text updates.
@protocol WatermarkConsumer <NSObject>

// Updates the watermark with the given text and style.
- (void)updateWatermarkWithText:(NSString*)text style:(WatermarkStyle)style;

@end

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_UI_WATERMARK_CONSUMER_H_
