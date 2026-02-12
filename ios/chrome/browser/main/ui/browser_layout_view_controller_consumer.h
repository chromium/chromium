// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAIN_UI_BROWSER_LAYOUT_VIEW_CONTROLLER_CONSUMER_H_
#define IOS_CHROME_BROWSER_MAIN_UI_BROWSER_LAYOUT_VIEW_CONTROLLER_CONSUMER_H_

#import <UIKit/UIKit.h>

// Consumer protocol for the BrowserLayoutViewController.
@protocol BrowserLayoutViewControllerConsumer <NSObject>

// Updates the tab strip and background views for fullscreen.
// `offset` is the vertical translation of the tab strip.
// `alpha` is the opacity for fading elements.
- (void)updateForFullscreenOffset:(CGFloat)offset alpha:(CGFloat)alpha;

@end

#endif  // IOS_CHROME_BROWSER_MAIN_UI_BROWSER_LAYOUT_VIEW_CONTROLLER_CONSUMER_H_
