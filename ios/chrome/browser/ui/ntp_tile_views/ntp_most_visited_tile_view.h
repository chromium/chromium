// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_TILE_VIEWS_NTP_MOST_VISITED_TILE_VIEW_H_
#define IOS_CHROME_BROWSER_UI_NTP_TILE_VIEWS_NTP_MOST_VISITED_TILE_VIEW_H_

#import "ios/chrome/browser/ui/ntp_tile_views/ntp_tile_view.h"

@class FaviconView;

// NTP Tile representing a most visited website. Displays a favicon and a title.
@interface NTPMostVisitedTileView : NTPTileView

// FaviconView displaying the favicon.
@property(nonatomic, strong, readonly, nonnull) FaviconView* faviconView;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_TILE_VIEWS_NTP_MOST_VISITED_TILE_VIEW_H_
