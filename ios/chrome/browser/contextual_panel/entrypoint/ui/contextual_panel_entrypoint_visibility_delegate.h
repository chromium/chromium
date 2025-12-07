// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_ENTRYPOINT_UI_CONTEXTUAL_PANEL_ENTRYPOINT_VISIBILITY_DELEGATE_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_ENTRYPOINT_UI_CONTEXTUAL_PANEL_ENTRYPOINT_VISIBILITY_DELEGATE_H_

#import <optional>

#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_type.h"

// A delegate for the contextual entry point visibility.
@protocol ContextualPanelEntrypointVisibilityDelegate <NSObject>

// Show/hide the contextual panel entrypoint.
- (void)setContextualPanelEntrypointHidden:(BOOL)hidden;

// Sets the type of the current contextual panel entrypoint item.
- (void)setContextualPanelItemType:
    (std::optional<ContextualPanelItemType>)itemType;

// Sets whether the contextual panel entrypoint is currently animating.
- (void)setContextualPanelCurrentlyAnimating:(BOOL)animating;

@optional

// TODO(crbug.com/458307626): Remove when migration is complete.
- (void)disableProactiveSuggestionOverlay:(BOOL)disabled;

@end

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_ENTRYPOINT_UI_CONTEXTUAL_PANEL_ENTRYPOINT_VISIBILITY_DELEGATE_H_
