// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_TABLE_VIEW_TABLE_VIEW_URL_CELL_FAVICON_BADGE_VIEW_H_
#define IOS_CHROME_COMMON_UI_TABLE_VIEW_TABLE_VIEW_URL_CELL_FAVICON_BADGE_VIEW_H_

#import <UIKit/UIKit.h>

// View used to display the favicon badge image.  This class automatically
// updates `hidden` to YES when its `image` is set to nil, rather than the
// default UIImageView behavior which applies a default highlight to the view
// for nil images.
@interface TableViewURLCellFaviconBadgeView : UIImageView

// The accessibility identifier of the badge view.
+ (NSString*)accessibilityIdentifier;

@end

#endif  // IOS_CHROME_COMMON_UI_TABLE_VIEW_TABLE_VIEW_URL_CELL_FAVICON_BADGE_VIEW_H_
