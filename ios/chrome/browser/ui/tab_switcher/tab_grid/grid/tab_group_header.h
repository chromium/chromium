// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUP_HEADER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUP_HEADER_H_

#import <UIKit/UIKit.h>

// A collection view header displaying tab group information.
@interface TabGroupHeader : UICollectionReusableView

// Group's title.
@property(nonatomic, copy) NSString* title;
// Group's color.
@property(nonatomic, copy) UIColor* color;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUP_HEADER_H_
