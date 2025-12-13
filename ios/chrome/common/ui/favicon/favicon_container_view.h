// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_FAVICON_FAVICON_CONTAINER_VIEW_H_
#define IOS_CHROME_COMMON_UI_FAVICON_FAVICON_CONTAINER_VIEW_H_

#import <UIKit/UIKit.h>

@class FaviconView;

// Container view that displays a `faviconView`.
@interface FaviconContainerView : UIView

// the `faviconView` to to display.
@property(nonatomic, readonly, strong) FaviconView* faviconView;

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// Sets the favicon's background color. Can be nil to reset to original value.
- (void)setFaviconBackgroundColor:(UIColor*)color;

@end

#endif  // IOS_CHROME_COMMON_UI_FAVICON_FAVICON_CONTAINER_VIEW_H_
