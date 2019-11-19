// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SCANNER_PREVIEW_OVERLAY_VIEW_H_
#define IOS_CHROME_BROWSER_UI_SCANNER_PREVIEW_OVERLAY_VIEW_H_

#import <UIKit/UIKit.h>

// A subclass of UIView containing the preview overlay. It is responsible for
// redrawing the preview overlay and the viewport border every time the size
// of the preview changes. This UIView should be a rectangle, with its width
// and height being the maximum of the width and height of its parent.
@interface PreviewOverlayView : UIView

// Initialises using viewport size.
- (instancetype)initWithFrame:(CGRect)frame
                 viewportSize:(CGSize)viewportSize NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SCANNER_PREVIEW_OVERLAY_VIEW_H_
