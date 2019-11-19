// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_TILE_VIEWS_NTP_TILE_VIEW_H_
#define IOS_CHROME_BROWSER_UI_NTP_TILE_VIEWS_NTP_TILE_VIEW_H_

#import <UIKit/UIKit.h>

// A generic NTP tile view. Provides a title label and an image container on a
// squircle-shaped background. Concrete subclasses of this are used to display
// most visited tiles and shortcut tiles on NTP and other places.
@interface NTPTileView : UIView

// Container for the image view. Used in subclasses.
@property(nonatomic, strong, readonly, nonnull) UIView* imageContainerView;

// Title of the Most Visited.
@property(nonatomic, strong, readonly, nonnull) UILabel* titleLabel;

// The view displaying the background image (squircle) for the tile image.
@property(nonatomic, strong, readonly, nonnull)
    UIImageView* imageBackgroundView;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_TILE_VIEWS_NTP_TILE_VIEW_H_
