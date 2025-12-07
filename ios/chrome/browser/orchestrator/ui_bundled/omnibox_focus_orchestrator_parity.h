// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ORCHESTRATOR_UI_BUNDLED_OMNIBOX_FOCUS_ORCHESTRATOR_PARITY_H_
#define IOS_CHROME_BROWSER_ORCHESTRATOR_UI_BUNDLED_OMNIBOX_FOCUS_ORCHESTRATOR_PARITY_H_

#import "ios/chrome/browser/orchestrator/ui_bundled/omnibox_focus_orchestrator.h"

// Orchestrator for the animation occurring when the omnibox is
// focused/unfocused when the Parity flag is enabled.
@interface OmniboxFocusOrchestratorParity : OmniboxFocusOrchestrator

// Toolbar animatee, orchestrated by this object.
@property(nonatomic, weak) id<ToolbarAnimatee> toolbarAnimatee;

@property(nonatomic, weak) id<LocationBarAnimatee> locationBarAnimatee;

@property(nonatomic, weak) id<EditViewAnimatee> editViewAnimatee;

// Updates the UI elements orchestrated by this object to reflect the
// `omniboxFocused` state, and the `toolbarExpanded` state, `animated` or not.
// `isNTP` indicates whether this transition was initiated from the NTP. When
// the transition is complete, `completion` will be executed.
- (void)transitionToStateOmniboxFocused:(BOOL)omniboxFocused
                        toolbarExpanded:(BOOL)toolbarExpanded
                                trigger:(OmniboxFocusTrigger)trigger
                               animated:(BOOL)animated
                             completion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_ORCHESTRATOR_UI_BUNDLED_OMNIBOX_FOCUS_ORCHESTRATOR_PARITY_H_
