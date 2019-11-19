// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_NEW_TAB_BUTTON_H_
#define IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_NEW_TAB_BUTTON_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/tab_grid/tab_grid_paging.h"

@interface TabGridNewTabButton : UIButton

@property(nonatomic, assign) TabGridPage page;

// Init with image for regular/incognito page.
- (instancetype)initWithRegularImage:(UIImage*)regularImage
                      incognitoImage:(UIImage*)incognitoImage
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_NEW_TAB_BUTTON_H_
