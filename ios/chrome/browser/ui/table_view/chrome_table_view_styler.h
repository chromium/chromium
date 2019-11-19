// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABLE_VIEW_CHROME_TABLE_VIEW_STYLER_H_
#define IOS_CHROME_BROWSER_UI_TABLE_VIEW_CHROME_TABLE_VIEW_STYLER_H_

#import <UIKit/UIKit.h>

@interface ChromeTableViewStyler : NSObject

// The background color for the table view.
@property(nonatomic, readwrite, strong) UIColor* tableViewBackgroundColor;
// The background color for the cell. It overrides |tableViewBackgroundColor|
// for the cell background if it is not nil.
@property(nonatomic, readwrite, strong) UIColor* cellBackgroundColor;
// Text colors.
@property(nonatomic, readwrite, strong) UIColor* cellTitleColor;
@property(nonatomic, readwrite, strong) UIColor* headerFooterTitleColor;
// Cell highlight color.
@property(nonatomic, readwrite, strong) UIColor* cellHighlightColor;
// Color of cell separator line. If not set, defaults to the default UIKit
// color.
@property(nonatomic, readwrite, strong) UIColor* cellSeparatorColor;

// TODO (crbug.com/981889): Remove with iOS 12.
// Color overrides. These should not be in general use, but
// are necessary to provide colors on pre-iOS 13 devices for screens that are
// always in dark mode. They can be removed then.
@property(nonatomic, readwrite, strong) UIColor* cellDetailColor;
@property(nonatomic, readwrite, strong) UIColor* headerFooterDetailColor;
@property(nonatomic, readwrite, strong) UIColor* tintColor;
@property(nonatomic, readwrite, strong) UIColor* solidButtonTextColor;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABLE_VIEW_CHROME_TABLE_VIEW_STYLER_H_
