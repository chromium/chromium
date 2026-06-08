// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_MUTATOR_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_MUTATOR_H_

// Protocol for handling toolbar button actions.
@protocol ToolbarMutator

// Exits fullscreen mode.
- (void)exitFullscreen;

// Navigates back.
- (void)goBack;

// Navigates forward.
- (void)goForward;

// Reloads the current page.
- (void)reload;

// Stops loading the current page.
- (void)stop;

// Called when the tab group indicator visibility is updated.
- (void)tabGroupIndicatorVisibilityUpdated:(BOOL)visible;

// Called when the assistant button is tapped.
- (void)assistantButtonTapped;

// Records the different users action when the user taps the tools menu.
- (void)recordUserActionsForToolsMenuTapped;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_MUTATOR_H_
