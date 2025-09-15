// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_GRID_EMPTY_THUMBNAIL_VIEW_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_GRID_EMPTY_THUMBNAIL_VIEW_H_

#import <UIKit/UIKit.h>

typedef NS_ENUM(NSInteger, EmptyThumbnailLayoutType);

// Enum for the different types of empty thumbnails.
typedef NS_ENUM(NSInteger, EmptyThumbnailType) {
  // Used for the grid cell.
  EmptyThumbnailTypeGridCell,
  // Used for the tab group cell.
  EmptyThumbnailTypeGroupCell,
};

// A view that displays an empty thumbnail.
@interface GridEmptyThumbnailView : UIView

// The current layout type of the view.
@property(nonatomic, assign) EmptyThumbnailLayoutType layoutType;

// Initializes the view for a `type` configuration.
- (instancetype)initWithType:(EmptyThumbnailType)type NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_GRID_EMPTY_THUMBNAIL_VIEW_H_
