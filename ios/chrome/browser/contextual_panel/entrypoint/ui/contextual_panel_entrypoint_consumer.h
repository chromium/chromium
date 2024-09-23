// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_ENTRYPOINT_UI_CONTEXTUAL_PANEL_ENTRYPOINT_CONSUMER_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_ENTRYPOINT_UI_CONTEXTUAL_PANEL_ENTRYPOINT_CONSUMER_H_

#import <UIKit/UIKit.h>

#import "base/memory/weak_ptr.h"

struct ContextualPanelItemConfiguration;

// Consumer for the ContextualPanelEntrypointViewController.
@protocol ContextualPanelEntrypointConsumer

// Update the consumer with the image to be shown on the entrypoint badge.
- (void)setEntrypointConfig:
    (base::WeakPtr<ContextualPanelItemConfiguration>)config;

// Sets whether there are infobar badges currently being shown.
- (void)setInfobarBadgesCurrentlyShown:(BOOL)infobarBadgesCurrentlyShown;

// Notify the consumer to hide the entrypoint.
- (void)hideEntrypoint;

// Notify the consumer to show the entrypoint.
- (void)showEntrypoint;

// Notify the consumer to transition to the large entrypoint for a loud moment.
- (void)transitionToLargeEntrypoint;

// Notify the consumer to transition back to the small entrypoint.
- (void)transitionToSmallEntrypoint;

// Notify the consumer to update the state of the entrypoint. When `opened` is
// passed as YES, the entrypoint gets muted colors and becomes small, otherwise,
// it returns to its default style.
- (void)transitionToContextualPanelOpenedState:(BOOL)opened;

// Notify the consumer to update the colored state of the entrypoint. When
// `colored` is YES, the entrypoint animates to blue, otherwise it animates back
// to its default color.
- (void)setEntrypointColored:(BOOL)colored;

@end

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_ENTRYPOINT_UI_CONTEXTUAL_PANEL_ENTRYPOINT_CONSUMER_H_
