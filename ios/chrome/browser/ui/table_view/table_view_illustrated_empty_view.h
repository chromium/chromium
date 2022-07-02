// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABLE_VIEW_TABLE_VIEW_ILLUSTRATED_EMPTY_VIEW_H_
#define IOS_CHROME_BROWSER_UI_TABLE_VIEW_TABLE_VIEW_ILLUSTRATED_EMPTY_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/chrome_empty_table_view_background.h"

// Displays an UIImage on top of a message over a clearBackground.
@interface TableViewIllustratedEmptyView
    : UIView <ChromeEmptyTableViewBackground>

// Designated initializer for a view that displays a large `image`, a `title`
// and a `subtitle`.
- (instancetype)initWithFrame:(CGRect)frame
                        image:(UIImage*)image
                        title:(NSString*)title
                     subtitle:(NSString*)subtitle NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

// The empty view's accessibility identifier.
+ (NSString*)accessibilityIdentifier;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABLE_VIEW_TABLE_VIEW_ILLUSTRATED_EMPTY_VIEW_H_
