// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_UI_WATERMARK_VIEW_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_UI_WATERMARK_VIEW_H_

#import <UIKit/UIKit.h>

// View containing a watermark text overlaid on top of the other content.
@interface WatermarkView : UIView

// The watermark text to display.
@property(nonatomic, copy) NSString* text;

// The watermark text fill opacity, represented as a fraction between 0.0
// and 1.0.
@property(nonatomic, assign) CGFloat fillOpacity;

// The watermark text outline opacity, represented as a fraction between 0.0
// and 1.0.
@property(nonatomic, assign) CGFloat outlineOpacity;

// The font size of the watermark text.
@property(nonatomic, assign) CGFloat fontSize;

@end

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_UI_WATERMARK_VIEW_H_
