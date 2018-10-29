// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABLE_VIEW_CHROME_TABLE_VIEW_STYLER_H_
#define IOS_CHROME_BROWSER_UI_TABLE_VIEW_CHROME_TABLE_VIEW_STYLER_H_

#import <UIKit/UIKit.h>

@interface ChromeTableViewStyler : NSObject

// The BlurEffect used on the UITableView section headers.
@property(nonatomic, readwrite, strong)
    UIBlurEffect* tableViewSectionHeaderBlurEffect;

// The background color for the table view and its cells. If this is set to an
// opaque color, cells can choose to make themselves opaque and draw their own
// background as a performance optimization.
@property(nonatomic, readwrite, strong) UIColor* tableViewBackgroundColor;
// The background color for the cell. It overrides |tableViewBackgroundColor|
// for the cell background if it is not nil.
@property(nonatomic, readwrite, strong) UIColor* cellBackgroundColor;
// Text colors.
@property(nonatomic, readwrite, strong) UIColor* cellTitleColor;
@property(nonatomic, readwrite, strong) UIColor* headerFooterTitleColor;
// Cell highlight color.
@property(nonatomic, readwrite, strong) UIColor* cellHighlightColor;
// Color of cell separator line. If not set, defaults to 0xC8C7CC.
@property(nonatomic, readwrite, strong) UIColor* cellSeparatorColor;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABLE_VIEW_CHROME_TABLE_VIEW_STYLER_H_
