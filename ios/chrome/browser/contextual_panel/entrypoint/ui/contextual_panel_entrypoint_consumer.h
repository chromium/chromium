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

// Notify the consumer to hide the entrypoint.
- (void)hideEntrypoint;

// Notify the consumer to show the entrypoint.
- (void)showEntrypoint;

// Notify the consumer to transition to the large entrypoint for a loud moment.
- (void)transitionToLargeEntrypoint;

// Notify the consumer to transition back to the small entrypoint.
- (void)transitionToSmallEntrypoint;

@end

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_ENTRYPOINT_UI_CONTEXTUAL_PANEL_ENTRYPOINT_CONSUMER_H_
