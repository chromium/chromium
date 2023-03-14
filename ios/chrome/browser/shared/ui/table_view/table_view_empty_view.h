// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_TABLE_VIEW_EMPTY_VIEW_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_TABLE_VIEW_EMPTY_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/chrome_empty_table_view_background.h"

// Displays an UIImage on top of a message over a clearBackground.
@interface TableViewEmptyView : UIView <ChromeEmptyTableViewBackground>

// Designated initializer for a view that displays `message` with default
// styling and `image` above the message.
- (instancetype)initWithFrame:(CGRect)frame
                      message:(NSString*)message
                        image:(UIImage*)image NS_DESIGNATED_INITIALIZER;
// Designated initializer for a view that displays an attributed `message` and
// `image` above the message.
- (instancetype)initWithFrame:(CGRect)frame
            attributedMessage:(NSAttributedString*)message
                        image:(UIImage*)image NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

// The empty view's accessibility identifier.
+ (NSString*)accessibilityIdentifier;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_TABLE_VIEW_EMPTY_VIEW_H_
