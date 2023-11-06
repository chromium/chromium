// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_LEGACY_CHROME_TABLE_VIEW_STYLER_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_LEGACY_CHROME_TABLE_VIEW_STYLER_H_

#import <UIKit/UIKit.h>

@interface ChromeTableViewStyler : NSObject

// The background color for the table view.
@property(nonatomic, readwrite, strong) UIColor* tableViewBackgroundColor;
// The background color for the cell. It overrides `tableViewBackgroundColor`
// for the cell background if it is not nil.
@property(nonatomic, readwrite, strong) UIColor* cellBackgroundColor;
// Text colors.
@property(nonatomic, readwrite, strong) UIColor* cellTitleColor;
// Cell highlight color.
@property(nonatomic, readwrite, strong) UIColor* cellHighlightColor;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_LEGACY_CHROME_TABLE_VIEW_STYLER_H_
