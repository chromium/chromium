// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_ELEMENTS_BRANDED_NAVIGATION_ITEM_TITLE_VIEW_H_
#define IOS_CHROME_COMMON_UI_ELEMENTS_BRANDED_NAVIGATION_ITEM_TITLE_VIEW_H_

#import <UIKit/UIKit.h>

// Navigation Item title with an embedded logo.
// TODO(crbug.com/501035908): Rename this to NavigationItemTitleWithLogoView.
@interface BrandedNavigationItemTitleView : UIView

// Initializes the view with a default system font.
- (instancetype)init NS_DESIGNATED_INITIALIZER;

// Initializes the view configured for displaying a product logo and name, using
// the given branded `font`, such as Product Sans. `font` must be non-nil.
// TODO(crbug.com/501035908): Rename this to initForBrandedTitleWithFont.
- (instancetype)initWithFont:(UIFont*)font NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// The title text displayed in the view.
@property(nonatomic, copy) NSString* title;

// The logo displayed next to the title.
@property(nonatomic, strong) UIImage* imageLogo;

// The spacing between the title and the image logo.
@property(nonatomic, assign) CGFloat titleLogoSpacing;

@end

#endif  // IOS_CHROME_COMMON_UI_ELEMENTS_BRANDED_NAVIGATION_ITEM_TITLE_VIEW_H_
