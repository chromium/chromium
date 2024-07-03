// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_CONTEXTUAL_PANEL_ENTRYPOINT_IPH_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_CONTEXTUAL_PANEL_ENTRYPOINT_IPH_COMMANDS_H_

#import "base/feature_list.h"

// Commands for the Contextual Panel Entrypoint's IPH.
@protocol ContextualPanelEntrypointIPHCommands

// Tries to show the Contextual Panel entrypoint's IPH, and returns the result.
// `feature` is the FET feature used for impression management for the given
// infoblock's IPH.
- (BOOL)maybeShowContextualPanelEntrypointIPHWithText:(NSString*)text
                                          anchorPoint:(CGPoint)anchorPoint
                                      isBottomOmnibox:(BOOL)isBottomOmnibox
                                              feature:
                                                  (const base::Feature&)feature;

// Dismisses the Contextual Panel entrypoint's IPH.
- (void)dismissContextualPanelEntrypointIPHAnimated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_CONTEXTUAL_PANEL_ENTRYPOINT_IPH_COMMANDS_H_
