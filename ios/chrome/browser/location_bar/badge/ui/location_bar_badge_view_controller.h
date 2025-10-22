// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_UI_LOCATION_BAR_BADGE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_UI_LOCATION_BAR_BADGE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/badges/ui_bundled/badge_view_visibility_delegate.h"
#import "ios/chrome/browser/badges/ui_bundled/incognito_badge_view_visibility_delegate.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/ui/contextual_panel_entrypoint_consumer.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/ui/contextual_panel_entrypoint_visibility_delegate.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_ui_element.h"
#import "ios/chrome/browser/location_bar/badge/ui/location_bar_badge_consumer.h"
#import "ios/chrome/browser/reader_mode/ui/reader_mode_chip_visibility_delegate.h"

@protocol ContextualPanelEntrypointMutator;
@class LayoutGuideCenter;

// View controller for the location bar badge.
@interface LocationBarBadgeViewController
    : UIViewController <BadgeViewVisibilityDelegate,
                        ContextualPanelEntrypointVisibilityDelegate,
                        IncognitoBadgeViewVisibilityDelegate,
                        LocationBarBadgeConsumer,
                        ReaderModeChipVisibilityDelegate,
                        ContextualPanelEntrypointConsumer,
                        FullscreenUIElement>

// This view controller's LayoutGuideCenter.
@property(nonatomic, strong) LayoutGuideCenter* layoutGuideCenter;
// This view controller's mutator.
@property(nonatomic, weak) id<ContextualPanelEntrypointMutator> mutator;

// Returns the anchor point in window coordinates for the entrypoint's IPH,
// depending on if the omnibox is at the top or bottom. Since the entrypoint is
// usually fairly close to the edge of the screen, this returns the MAX X
// coordinate between the default bubble offset and the middle X of the
// entrypoint.
- (CGPoint)helpAnchorUsingBottomOmnibox:(BOOL)isBottomOmnibox;

@end

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_UI_LOCATION_BAR_BADGE_VIEW_CONTROLLER_H_
