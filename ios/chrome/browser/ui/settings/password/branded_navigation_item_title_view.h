// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_BRANDED_NAVIGATION_ITEM_TITLE_VIEW_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_BRANDED_NAVIGATION_ITEM_TITLE_VIEW_H_

#import <UIKit/UIKit.h>

// Navigation Item title branded with the Product Sans Regular font and a logo.
@interface BrandedNavigationItemTitleView : UIView

// The title text displayed in the view.
@property(nonatomic, copy) NSString* title;

// The logo displayed next to the title.
@property(nonatomic, strong) UIImage* imageLogo;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_BRANDED_NAVIGATION_ITEM_TITLE_VIEW_H_
