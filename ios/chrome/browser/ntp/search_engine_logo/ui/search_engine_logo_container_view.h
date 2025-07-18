// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_SEARCH_ENGINE_LOGO_UI_SEARCH_ENGINE_LOGO_CONTAINER_VIEW_H_
#define IOS_CHROME_BROWSER_NTP_SEARCH_ENGINE_LOGO_UI_SEARCH_ENGINE_LOGO_CONTAINER_VIEW_H_

#import <UIKit/UIKit.h>

#include "base/ios/block_types.h"

@protocol SearchEngineLogoContainerViewDelegate;

// Enum describing whether the logo displayed is the normal Google logo or a
// doodle.
typedef NS_ENUM(short, SearchEngineLogoContainerViewStyle) {
  SEARCH_ENGINE_LOGO_CONTAINER_VIEW_STYLE_LOGO,
  SEARCH_ENGINE_LOGO_CONTAINER_VIEW_STYLE_DOODLE
};

// Container view used to display the Google logo or doodle.
@interface SearchEngineLogoContainerView : UIView

// The delegate.
@property(nonatomic, weak) id<SearchEngineLogoContainerViewDelegate> delegate;

// The logo style.  Defaults to SEARCH_ENGINE_LOGO_CONTAINER_VIEW_STYLE_LOGO.
@property(nonatomic, assign) SearchEngineLogoContainerViewStyle style;

// Whether the doodle is shown and animating.
@property(nonatomic, readonly, getter=isAnimatingDoodle) BOOL animatingDoodle;

// Shrunk version of the logo that is used.
@property(nonatomic, strong) UIImageView* shrunkLogoView;

// The alt text for the doodle.  This is used as the doodle view's a11y label.
@property(nonatomic, copy) NSString* doodleAltText;

// Setter for `style` with the option to do a crossfade animation.
- (void)setStyle:(SearchEngineLogoContainerViewStyle)style
        animated:(BOOL)animated;

// Setters for the doodle image.  Adds a crossfade animation when `animated` is
// yes. Runs the `animations` block as part of the transition animation,
// allowing other properties to be animated alongside the image crossfade.
- (void)setDoodleImage:(UIImage*)image
              animated:(BOOL)animated
            animations:(ProceduralBlock)animations;
// Sets the doodle image. The transition is animated or not accordingn to
// `animated`.
- (void)setAnimatedDoodleImage:(UIImage*)image animated:(BOOL)animated;

@end

// The delegate protocol for SearchEngineLogoContainerView.
@protocol SearchEngineLogoContainerViewDelegate <NSObject>

// Called when the doodle was tapped.
- (void)searchEngineLogoContainerViewDoodleWasTapped:
    (SearchEngineLogoContainerView*)containerView;

@end

#endif  // IOS_CHROME_BROWSER_NTP_SEARCH_ENGINE_LOGO_UI_SEARCH_ENGINE_LOGO_CONTAINER_VIEW_H_
