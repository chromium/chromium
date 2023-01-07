// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CONTENT_WIDGET_EXTENSION_CONTENT_WIDGET_VIEW_H_
#define IOS_CHROME_CONTENT_WIDGET_EXTENSION_CONTENT_WIDGET_VIEW_H_

#import <UIKit/UIKit.h>

@class MostVisitedTileView;
@class NTPTile;

// Protocol to be implemented by targets for user actions coming from the
// content widget view.
@protocol ContentWidgetViewDelegate

// Called when tapping a tile to open `URL`.
- (void)openURL:(NSURL*)URL;

@end

// View for the content widget. Shows 1 (compact view) or 2 (full size view)
// rows of 4 most visited tiles (favicon or fallback + title), if there are
// enough tiles to show. If there are fewer than 4 tiles, always displays a
// single row.
@interface ContentWidgetView : UIView

// The height of the widget in expanded mode.
@property(nonatomic, readonly) CGFloat widgetExpandedHeight;

// Designated initializer, creates the widget view with a `delegate` for user
// actions. `compactHeight` indicates the size to use in compact display.
// `width` is the width of the widget.
- (instancetype)initWithDelegate:(id<ContentWidgetViewDelegate>)delegate
                   compactHeight:(CGFloat)compactHeight
                           width:(CGFloat)width NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

// Updates the view to display a compact or expanded view, depending on
// `compact`. If `compact` is false, the view shows a maximum of 8 tiles. If
// `compact` is true, the view is set to show a single row of 4 tiles at most
// within the `compactHeight` passed in the constructor.
- (void)showMode:(BOOL)compact;

// Updates the displayed sites. `sites` should contain NTPTiles with continuous
// positions starting at 0.
- (void)updateSites:(NSDictionary<NSURL*, NTPTile*>*)sites;

// Returns whether all the sites can be displayed on a single row.
- (BOOL)sitesFitSingleRow;

@end

#endif  // IOS_CHROME_CONTENT_WIDGET_EXTENSION_CONTENT_WIDGET_VIEW_H_
