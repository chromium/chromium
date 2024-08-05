// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_CONTEXTUAL_PANEL_ENTRYPOINT_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_CONTEXTUAL_PANEL_ENTRYPOINT_COMMANDS_H_

// Commands for the Contextual Panel's Entrypoint.
@protocol ContextualPanelEntrypointCommands

// Notifies the Contextual Panel Entrypoint that the IPH (in-product help) was
// dismissed.
- (void)contextualPanelEntrypointIPHWasDismissed;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_CONTEXTUAL_PANEL_ENTRYPOINT_COMMANDS_H_
