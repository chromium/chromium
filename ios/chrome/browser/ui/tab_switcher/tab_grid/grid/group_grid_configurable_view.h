// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GROUP_GRID_CONFIGURABLE_VIEW_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GROUP_GRID_CONFIGURABLE_VIEW_H_

#import <UIKit/UIKit.h>

// `GroupGridConfigurableView` is a UIView with 4 subviews distributed equally,
// topleading/topTrailing/bottomLeading/bottomTrailing.
@interface GroupGridConfigurableView : UIView

// Designated initializer with `spacing` to apply as vertical/horizontal gap
// between the subviews.
- (instancetype)initWithSpacing:(CGFloat)spacing NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Used to set the corner radius of the 4 subviews within the
// `GroupGridConfigurableView`.
@property(nonatomic, assign) CGFloat applicableCornerRadius;

// Adds a subview to the top leading view with the same constraints.
- (void)updateTopLeadingWithView:(UIView*)view;

// Adds a subview to the top trailing view with the same constraints.
- (void)updateTopTrailingWithView:(UIView*)view;

// Adds a subview to the bottom leading view with the same constraints.
- (void)updateBottomLeadingWithView:(UIView*)view;

// Adds a subview to the bottom trailing view with the same constraints.
- (void)updateBottomTrailingWithView:(UIView*)view;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GROUP_GRID_CONFIGURABLE_VIEW_H_
