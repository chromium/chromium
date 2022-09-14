// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_GESTURES_PAN_HANDLER_SCROLL_VIEW_H_
#define IOS_CHROME_BROWSER_UI_GESTURES_PAN_HANDLER_SCROLL_VIEW_H_

#import <UIKit/UIKit.h>

@class CRWWebViewScrollViewProxy;

// This private class handles forwarding updates to these properties to an
// underlying `UIScrollView` or `CRWWebViewScrollViewProxy`.
@interface PanHandlerScrollView : NSObject

@property(nonatomic) CGPoint contentOffset;
@property(nonatomic, assign) UIEdgeInsets contentInset;
@property(nonatomic, readonly) UIPanGestureRecognizer* panGestureRecognizer;
@property(nonatomic, readonly, getter=isDecelerating) BOOL decelerating;
@property(nonatomic, readonly, getter=isDragging) BOOL dragging;

- (instancetype)initWithScrollView:(UIScrollView*)scrollView;
- (instancetype)initWithWebViewScrollViewProxy:
    (CRWWebViewScrollViewProxy*)scrollViewProxy;

@end

#endif  // IOS_CHROME_BROWSER_UI_GESTURES_PAN_HANDLER_SCROLL_VIEW_H_
