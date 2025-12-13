// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_BADGES_CONTAINER_VIEW_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_BADGES_CONTAINER_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/badges/ui_bundled/badge_view_visibility_delegate.h"
#import "ios/chrome/browser/badges/ui_bundled/incognito_badge_view_visibility_delegate.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/ui/contextual_panel_entrypoint_visibility_delegate.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_placeholder_type.h"
#import "ios/chrome/browser/reader_mode/ui/reader_mode_chip_visibility_delegate.h"

@protocol PageActionMenuCommands;

// Location bar badges container view, it contains location bar accessories such
// as infobar badges and entrypoints.
// This view does not itself create any badges. The embedder needs to provide
// the views to display.
@interface LocationBarBadgesContainerView
    : UIView <BadgeViewVisibilityDelegate,
              ContextualPanelEntrypointVisibilityDelegate,
              IncognitoBadgeViewVisibilityDelegate,
              ReaderModeChipVisibilityDelegate>

// The injected view displaying the incognito badge.
@property(nonatomic, strong) UIView* incognitoBadgeView;
// The injected view displaying infobar badges.
@property(nonatomic, strong) UIView* badgeView;
// The injected view displaying the Contextual Panel's entrypoint.
@property(nonatomic, strong) UIView* contextualPanelEntrypointView;
// A placeholder to be displayed by default when there are no visible badges.
// Set to nil to remove the view.
@property(nonatomic, strong) UIView* placeholderView;
// The type of placeholder displayed in `placeholderView`.
@property(nonatomic, assign) LocationBarPlaceholderType placeholderType;
// The injected view displaying the Reading mode chip.
@property(nonatomic, strong) UIView* readerModeChipView;
// Whether the browser is in incognito mode.
@property(nonatomic, assign, getter=isIncognito) BOOL incognito;
// Transparent overlay button for unified badge interaction.
@property(nonatomic, weak) id<PageActionMenuCommands> pageActionMenuHandler;

@end

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_BADGES_CONTAINER_VIEW_H_
