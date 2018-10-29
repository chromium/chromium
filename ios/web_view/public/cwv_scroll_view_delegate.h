// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_SCROLL_VIEW_DELEGATE_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_SCROLL_VIEW_DELEGATE_H_

#import <CoreGraphics/CoreGraphics.h>

NS_ASSUME_NONNULL_BEGIN

@class CWVScrollView;

// Delegete for CWVScrollView.
//
// These methods are forwarded from the internal UIScrollViewDelegate. Please
// see the <UIKit/UIScrollViewDelegate.h> documentation for details about the
// following methods.
@protocol CWVScrollViewDelegate<NSObject>
@optional
- (void)scrollViewWillBeginDragging:(CWVScrollView*)scrollView;
- (void)scrollViewWillEndDragging:(CWVScrollView*)scrollView
                     withVelocity:(CGPoint)velocity
              targetContentOffset:(inout CGPoint*)targetContentOffset;
- (void)scrollViewDidScroll:(CWVScrollView*)scrollView;
- (void)scrollViewDidEndDecelerating:(CWVScrollView*)scrollView;
- (BOOL)scrollViewShouldScrollToTop:(CWVScrollView*)scrollView;

// The equivalent in UIScrollViewDelegate also takes a parameter (UIView*)view,
// but CWVScrollViewDelegate doesn't expose it for flexibility of future
// implementation.
- (void)scrollViewWillBeginZooming:(CWVScrollView*)webViewScrollViewProxy;
@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_SCROLL_VIEW_DELEGATE_H_
