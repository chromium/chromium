// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_VERTICAL_SIDEBAR_CELL_H_
#define IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_VERTICAL_SIDEBAR_CELL_H_

#import <UIKit/UIKit.h>

// A cell of a `VerticalSidebarView` pill, hosting one view centered with Auto
// Layout. It forwards touches anywhere in it to the hosted view, so the touch
// target is the whole cell rather than the hosted view's own bounds.
@interface VerticalSidebarCell : UIView

// `hostedView` must have a size (intrinsic or constrained). Its
// `translatesAutoresizingMaskIntoConstraints` is turned off.
- (instancetype)initWithHostedView:(UIView*)hostedView
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_VERTICAL_SIDEBAR_CELL_H_
