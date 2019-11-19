// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ORCHESTRATOR_LOCATION_BAR_ANIMATEE_H_
#define IOS_CHROME_BROWSER_UI_ORCHESTRATOR_LOCATION_BAR_ANIMATEE_H_

#import <Foundation/Foundation.h>

// Protocol defining an interface to trigger changes on the location bar.
// Calling those methods should not start any animation.
@protocol LocationBarAnimatee<NSObject>

// Moves the edit view to a position where the text in it visually overlays the
// text in steady view.
- (void)offsetEditViewToMatchSteadyView;
// Call this after calling -offsetEditViewToMatchSteadyView. Moves the edit view
// to its normal position and offsets the steady view to match the normal edit
// view position, so that the text in it visually overlaying the text in edit
// view.
- (void)resetEditViewOffsetAndOffsetSteadyViewToMatch;

// See comment for -offsetEditViewToMatchSteadyView.
- (void)offsetSteadyViewToMatchEditView;
// See comment for -resetEditViewOffsetAndOffsetSteadyViewToMatch.
- (void)resetSteadyViewOffsetAndOffsetEditViewToMatch;

// Hides badge view for steady view.
- (void)hideSteadyViewBadgeView;
// Displays the badge view of the steady view.
- (void)showSteadyViewBadgeView;

- (void)setSteadyViewFaded:(BOOL)hidden;
- (void)setEditViewFaded:(BOOL)hidden;
- (void)setEditViewHidden:(BOOL)hidden;
- (void)setSteadyViewHidden:(BOOL)hidden;

// Resets tranforms of edit and steady view. Used for post-animation cleanup.
// Only resets the translation, and leaves scale intact.
- (void)resetTransforms;

@end

#endif  // IOS_CHROME_BROWSER_UI_ORCHESTRATOR_LOCATION_BAR_ANIMATEE_H_
