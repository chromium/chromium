// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_BRANDED_NAVIGATION_ITEM_TITLE_VIEW_H_
#define IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_BRANDED_NAVIGATION_ITEM_TITLE_VIEW_H_

#import <UIKit/UIKit.h>

// Navigation Item title branded with the Product Sans Regular font and a logo.
@interface BrandedNavigationItemTitleView : UIView

// The title text displayed in the view.
@property(nonatomic, copy) NSString* title;

// The logo displayed next to the title.
@property(nonatomic, strong) UIImage* imageLogo;

// The spacing between the title and the image logo.
@property(nonatomic, assign) CGFloat titleLogoSpacing;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_BRANDED_NAVIGATION_ITEM_TITLE_VIEW_H_
