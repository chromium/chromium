// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_CONTEXTUAL_PANEL_ENTRYPOINT_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_CONTEXTUAL_PANEL_ENTRYPOINT_COMMANDS_H_

// Commands for the Contextual Panel Entrypoint.
@protocol ContextualPanelEntrypointCommands

// Tells the Contextual Panel Entrypoint that the model has been updated,
// allowing the entrypoint to act accordingly and update the UI.
- (void)updateContextualPanelEntrypointForNewModelData;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_CONTEXTUAL_PANEL_ENTRYPOINT_COMMANDS_H_
