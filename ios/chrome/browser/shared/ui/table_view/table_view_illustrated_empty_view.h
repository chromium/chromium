// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_TABLE_VIEW_ILLUSTRATED_EMPTY_VIEW_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_TABLE_VIEW_ILLUSTRATED_EMPTY_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/chrome_empty_table_view_background.h"

@class TableViewIllustratedEmptyView;

// Protocol that delegates of `TableViewIllustratedEmptyView` must implement.
// Delegates get notified when links in the `TableViewIllustratedEmptyView`'s
// subtitle are tapped.
@protocol TableViewIllustratedEmptyViewDelegate

// Invoked when a link in `view`'s subtitle is tapped.
- (void)tableViewIllustratedEmptyView:(TableViewIllustratedEmptyView*)view
                   didTapSubtitleLink:(NSURL*)URL;

@end

// Displays an UIImage on top of a message over a clearBackground.
@interface TableViewIllustratedEmptyView
    : UIView <ChromeEmptyTableViewBackground>

// Convenience initializer for a view that displays a large `image`, a `title`
// and a `subtitle`.
- (instancetype)initWithFrame:(CGRect)frame
                        image:(UIImage*)image
                        title:(NSString*)title
                     subtitle:(NSString*)subtitle;

// Designated initializer for a view that displays a large `image`, a `title`
// and an `attributedSubtitle`.
- (instancetype)initWithFrame:(CGRect)frame
                        image:(UIImage*)image
                        title:(NSString*)title
           attributedSubtitle:(NSAttributedString*)attributedSubtitle
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

// Delegate to notify when a link in the subtitle is tapped.
@property(nonatomic, weak) id<TableViewIllustratedEmptyViewDelegate> delegate;

// The empty view's accessibility identifier.
+ (NSString*)accessibilityIdentifier;

// Returns an NSDictionary suitable for use as the default/base attributes of
// a subtitle in a TableViewIllustratedEmptyView. The appearance will match the
// result of passing an NSString* as `subtitle` in the convenience initializer
// above.
+ (NSDictionary*)defaultTextAttributesForSubtitle;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_TABLE_VIEW_ILLUSTRATED_EMPTY_VIEW_H_
