// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_BADGES_CONTAINER_VIEW_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_BADGES_CONTAINER_VIEW_H_

#import <UIKit/UIKit.h>

// Location bar badges container view, it contains location bar accessories such
// as infobar badges and entrypoints.
// This view does not itself create any badges. The embedder needs to provide
// the views to display.
@interface LocationBarBadgesContainerView : UIView

// The injected view displaying infobar badges.
@property(nonatomic, strong) UIView* badgeView;
// The injected view displaying the Contextual Panel's entrypoint.
@property(nonatomic, strong) UIView* contextualPanelEntrypointView;

// Elements to surface in accessibility.
- (NSMutableArray*)accessibleElements;

@end

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_BADGES_CONTAINER_VIEW_H_
