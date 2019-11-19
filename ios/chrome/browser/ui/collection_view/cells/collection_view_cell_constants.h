// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COLLECTION_VIEW_CELLS_COLLECTION_VIEW_CELL_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_COLLECTION_VIEW_CELLS_COLLECTION_VIEW_CELL_CONSTANTS_H_

#import <CoreGraphics/CoreGraphics.h>

// The amount of padding at the top and bottom of UIKit-style cells.
const CGFloat kUIKitVerticalPadding = 16;

// The font size to use for main text in UIKit-style cells.
const CGFloat kUIKitMainFontSize = 17;

// The font size to use for detail text that is on the trailing edge
// of the top line.
const CGFloat kUIKitDetailFontSize = 17;

// The font size to use for detail text that is on its own line.
const CGFloat kUIKitMultilineDetailFontSize = 12;

// The font size and color to use for footer text in UIKit-style cells.
const CGFloat kUIKitFooterFontSize = 13;

// The tint color to use for switches in UIKit-style cells.
const int kUIKitSwitchTintColor = 0x1A73E8;

// The font size and color to use for headers in UIKit-style cells.
const int kUIKitHeaderTextColor = 0x000000;
const CGFloat kUIKitHeaderTextAlpha = 0.5;
const CGFloat kUIKitHeaderFontSize = 13;

// The color to use for separators between UIKit-style cells.
const int kUIKitSeparatorColor = 0xC8C7CC;

#endif  // IOS_CHROME_BROWSER_UI_COLLECTION_VIEW_CELLS_COLLECTION_VIEW_CELL_CONSTANTS_H_
