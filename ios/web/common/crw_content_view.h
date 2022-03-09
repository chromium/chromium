// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_COMMON_CRW_CONTENT_VIEW_H_
#define IOS_WEB_COMMON_CRW_CONTENT_VIEW_H_

#import <UIKit/UIKit.h>

// UIViews conforming to CRWScrollableContent (i.e. CRWContentViews) are used
// to display content within a WebState.
@protocol CRWScrollableContent <NSObject>

// The scroll view used to display the content.  If |scrollView| is non-nil,
// it will be used to back the CRWContentViewScrollViewProxy and is expected to
// be a subview of the CRWContentView.
@property(nonatomic, strong, readonly) UIScrollView* scrollView;

// Adds an offset to the content view.  This updates the location of the scroll
// view relative to the receiver, and does not update the scroll view's content
// offset.
@property(nonatomic, assign) CGPoint contentOffset;

// Adds an inset to content view. Implementations of this protocol can
// implement this method using UIScrollView.contentInset (where applicable) or
// via resizing a subview's frame. Can be used as a workaround for WKWebView
// bug, where UIScrollView.content inset does not work (rdar://23584409).
@property(nonatomic, assign) UIEdgeInsets contentInset;

@optional

// Whether or not the content view should use the content inset when setting
// |contentInset|.
@property(nonatomic, assign) BOOL shouldUseViewContentInset;

@end

// Convenience type for content views.
typedef UIView<CRWScrollableContent> CRWContentView;

#endif  // IOS_WEB_COMMON_CRW_CONTENT_VIEW_H_
