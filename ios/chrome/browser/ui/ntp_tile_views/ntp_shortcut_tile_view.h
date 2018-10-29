// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_TILE_VIEWS_NTP_SHORTCUT_TILE_VIEW_H_
#define IOS_CHROME_BROWSER_UI_NTP_TILE_VIEWS_NTP_SHORTCUT_TILE_VIEW_H_

#import "ios/chrome/browser/ui/ntp_tile_views/ntp_tile_view.h"

// A tile view displaying a collection shortcut. Accepts a simple icon and
// optionally supports a badge, for example for reading list new item count.
@interface NTPShortcutTileView : NTPTileView

// View for action icon.
@property(nonatomic, strong, readonly, nonnull) UIImageView* iconView;

// Container view for |countLabel|.
@property(nonatomic, strong, readonly, nonnull) UIView* countContainer;

// Number shown in circle by top trailing side of cell.
@property(nonatomic, strong, readonly, nonnull) UILabel* countLabel;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_TILE_VIEWS_NTP_SHORTCUT_TILE_VIEW_H_
