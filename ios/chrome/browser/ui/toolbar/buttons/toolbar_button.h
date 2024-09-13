// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_BUTTONS_TOOLBAR_BUTTON_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_BUTTONS_TOOLBAR_BUTTON_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/elements/custom_highlight_button.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_component_options.h"

@class LayoutGuideCenter;
@class ToolbarConfiguration;

using ToolbarButtonImageLoader = UIImage* (^)(void);

// UIButton subclass used as a Toolbar component.
@interface ToolbarButton : CustomHighlightableButton

// Configuration object used to get colors.
@property(nonatomic, weak) ToolbarConfiguration* toolbarConfiguration;
// Bitmask used for SizeClass visibility.
@property(nonatomic, assign) ToolbarComponentVisibility visibilityMask;
// Returns true if the ToolbarButton should be hidden in the current SizeClass.
@property(nonatomic, assign) BOOL hiddenInCurrentSizeClass;
// Returns true if the ToolbarButton should be hidden due to a current UI state
// or WebState.
@property(nonatomic, assign) BOOL hiddenInCurrentState;
// Name of the layout guide this button should be constrained to, if not nil.
// The constraints to the layout guide are only valid when the button is
// displayed. Also, they can be dropped/changed upon size class changes or
// rotations. Any view constrained to them is expected to be dismissed on such
// events.
@property(nonatomic, strong) GuideName* guideName;
// The layout guide center for this button.
@property(nonatomic, strong) LayoutGuideCenter* layoutGuideCenter;
// Whether this button is highlighted for an IPH, having a blue background. This
// color will override the spotlighted background color.
@property(nonatomic, assign) BOOL iphHighlighted;
// View used to display the view used for the spotlight effect.
@property(nonatomic, strong) UIView* spotlightView;
// Whether this button has blue dot promo.
@property(nonatomic, assign) BOOL hasBlueDot;

// Returns a ToolbarButton with a type system, using the `imageLoader` to load
// the image for normal state. Can only be used when
// `kEnableStartupImprovements` is enabled.
- (instancetype)initWithImageLoader:(ToolbarButtonImageLoader)imageLoader;
// Returns a ToolbarButton using the `imageLoader` to build image for normal
// state and `IPHHighlightedImageLoader` to load image for IPHHighlightedImage
// state. Can only be used when`kEnableStartupImprovements` is enabled.
- (instancetype)initWithImageLoader:(ToolbarButtonImageLoader)imageLoader
          IPHHighlightedImageLoader:
              (ToolbarButtonImageLoader)IPHHighlightedImageLoader;

// Checks if the ToolbarButton should be visible in the current SizeClass,
// afterwards it calls setHiddenForCurrentStateAndSizeClass if needed.
- (void)updateHiddenInCurrentSizeClass;

// Sets a new image loader. If the image was previously loaded, it reloads it.
// Otherwise, it stores the image loader block and wait for the image to be
// lazily loaded.
- (void)setImageLoader:(ToolbarButtonImageLoader)imageLoader;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_BUTTONS_TOOLBAR_BUTTON_H_
