// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_COMMON_CRW_VIEWPORT_CONTROLLER_H_
#define IOS_WEB_COMMON_CRW_VIEWPORT_CONTROLLER_H_

#import <UIKit/UIKit.h>

// Protocol for views that can have obscured insets and viewport limits.
@protocol CRWViewportController <NSObject>

@property(nonatomic) UIEdgeInsets obscuredContentInsets API_AVAILABLE(ios(26.0))
    ;

// The web view's minimum viewport inset.
@property(nonatomic, readonly) UIEdgeInsets minimumViewportInset;

// The web view's maximum viewport inset.
@property(nonatomic, readonly) UIEdgeInsets maximumViewportInset;

// Sets the web view's min and max viewport insets.
- (void)setMinimumViewportInset:(UIEdgeInsets)minInset
           maximumViewportInset:(UIEdgeInsets)maxInset;

@end

#endif  // IOS_WEB_COMMON_CRW_VIEWPORT_CONTROLLER_H_
