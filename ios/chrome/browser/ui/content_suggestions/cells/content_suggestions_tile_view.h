// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_TILE_VIEW_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_TILE_VIEW_H_

#import <UIKit/UIKit.h>

// A generic Content Suggestions tile view. Provides a title label and an image container on a
// squircle-shaped background. Concrete subclasses of this are used to display
// most visited tiles and shortcut tiles on NTP and other places.
@interface ContentSuggestionsTileView : UIView <UIPointerInteractionDelegate>

// Initializer that will lay itself out as placeholder tile with no text or
// favicon if `isPlaceholder` is YES.
- (instancetype)initWithFrame:(CGRect)frame placeholder:(BOOL)isPlaceholder;

// Container for the image view. Used in subclasses.
@property(nonatomic, strong, readonly) UIView* imageContainerView;

// Title of the Most Visited.
@property(nonatomic, strong, readonly) UILabel* titleLabel;

// The view displaying the background image (squircle) for the tile image.
@property(nonatomic, strong, readonly) UIImageView* imageBackgroundView;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_TILE_VIEW_H_
