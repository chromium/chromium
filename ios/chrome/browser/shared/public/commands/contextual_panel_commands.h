// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_CONTEXTUAL_PANEL_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_CONTEXTUAL_PANEL_COMMANDS_H_

// Commands for the Contextual Panel.
@protocol ContextualPanelCommands

// Set the Contextual Panel's entrypoint visible.
- (void)showContextualPanelEntrypoint;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_CONTEXTUAL_PANEL_COMMANDS_H_
