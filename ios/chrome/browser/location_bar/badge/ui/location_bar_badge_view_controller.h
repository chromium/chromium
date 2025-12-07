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
@protocol ContextualPanelEntrypointVisibilityDelegate;
@protocol LocationBarBadgeMutator;
@class IncognitoBadgeViewController;
@class LayoutGuideCenter;

// View controller for the location bar badge.
@interface LocationBarBadgeViewController
    : UIViewController <IncognitoBadgeViewVisibilityDelegate,
                        LocationBarBadgeConsumer,
                        ContextualPanelEntrypointConsumer,
                        ContextualPanelEntrypointVisibilityDelegate,
                        FullscreenUIElement>

// This view controller's LayoutGuideCenter.
@property(nonatomic, strong) LayoutGuideCenter* layoutGuideCenter;

// This view controller's Contextual Panel Entrypoint mutator.
@property(nonatomic, weak) id<ContextualPanelEntrypointMutator>
    contextualPanelEntryPointMutator;

// This view controller's Location Bar Badge mutator.
@property(nonatomic, weak) id<LocationBarBadgeMutator> mutator;

// TODO(crbug.com/429140788): Remove after migration.
// The entrypoint visibility delegate.
@property(nonatomic, weak) id<ContextualPanelEntrypointVisibilityDelegate>
    visibilityDelegate;

// The view controller displaying the incognito badge. Nil for non-incognito.
@property(nonatomic, strong)
    IncognitoBadgeViewController* incognitoBadgeViewController;

// Returns the anchor point in window coordinates for the entrypoint's IPH,
// depending on if the omnibox is at the top or bottom. Since the badge is
// usually fairly close to the edge of the screen, this returns the MAX X
// coordinate between the default bubble offset and the middle X of the
// badge.
- (CGPoint)helpAnchorUsingBottomOmnibox:(BOOL)isBottomOmnibox;

@end

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_UI_LOCATION_BAR_BADGE_VIEW_CONTROLLER_H_
