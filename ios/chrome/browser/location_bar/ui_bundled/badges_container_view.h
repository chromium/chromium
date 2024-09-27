// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_BADGES_CONTAINER_VIEW_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_BADGES_CONTAINER_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/badges/ui_bundled/badge_view_visibility_delegate.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/ui/contextual_panel_entrypoint_visibility_delegate.h"

// Location bar badges container view, it contains location bar accessories such
// as infobar badges and entrypoints.
// This view does not itself create any badges. The embedder needs to provide
// the views to display.
@interface LocationBarBadgesContainerView
    : UIView <BadgeViewVisibilityDelegate,
              ContextualPanelEntrypointVisibilityDelegate>

// The injected view displaying infobar badges.
@property(nonatomic, strong) UIView* badgeView;
// The injected view displaying the Contextual Panel's entrypoint.
@property(nonatomic, strong) UIView* contextualPanelEntrypointView;
// A placeholder to be displayed by default when there are no visible badges.
// Set to nil to remove the view.
@property(nonatomic, strong) UIView* placeholderView;

@end

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_BADGES_CONTAINER_VIEW_H_
