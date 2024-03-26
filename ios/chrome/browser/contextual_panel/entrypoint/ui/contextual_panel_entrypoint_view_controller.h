// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_ENTRYPOINT_UI_CONTEXTUAL_PANEL_ENTRYPOINT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_ENTRYPOINT_UI_CONTEXTUAL_PANEL_ENTRYPOINT_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/contextual_panel/entrypoint/ui/contextual_panel_entrypoint_consumer.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_element.h"

@protocol ContextualPanelEntrypointMutator;

// View controller for ContextualPanelEntrypoint.
@interface ContextualPanelEntrypointViewController
    : UIViewController <ContextualPanelEntrypointConsumer, FullscreenUIElement>

// This view controller's mutator.
@property(nonatomic, weak) id<ContextualPanelEntrypointMutator> mutator;

// Allows to hide or unhide the entrypoint view. It will always hide the view
// when `display` is NO, but only conditionally unhide the view when `display`
// is YES, depending on whether it should currently be shown or not.
- (void)displayEntrypointView:(BOOL)display;

@end

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_ENTRYPOINT_UI_CONTEXTUAL_PANEL_ENTRYPOINT_VIEW_CONTROLLER_H_
