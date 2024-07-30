// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ORCHESTRATOR_UI_BUNDLED_LOCATION_BAR_ANIMATEE_H_
#define IOS_CHROME_BROWSER_ORCHESTRATOR_UI_BUNDLED_LOCATION_BAR_ANIMATEE_H_

#import <Foundation/Foundation.h>

// Protocol defining an interface to trigger changes on the location bar.
// Calling those methods should not start any animation.
@protocol LocationBarAnimatee<NSObject>

// Moves the text field to a position where the text in it visually overlays the
// text in steady view.
- (void)offsetTextFieldToMatchSteadyView;
// Call this after calling -offsetTextFieldToMatchSteadyView. Moves the text
// field to its normal position and offsets the steady view to match the normal
// text field position, so that the text in it visually overlaying the text in
// edit view.
- (void)resetTextFieldOffsetAndOffsetSteadyViewToMatch;

// See comment for -offsetTextFieldToMatchSteadyView.
- (void)offsetSteadyViewToMatchTextField;
// See comment for -resetTextFieldOffsetAndOffsetSteadyViewToMatch.
- (void)resetSteadyViewOffsetAndOffsetTextFieldToMatch;

// Hides the badge and entrypoint views for steady view.
- (void)hideSteadyViewBadgeAndEntrypointViews;
// Displays the badge and entrypoint views of the steady view.
- (void)showSteadyViewBadgeAndEntrypointViews;

- (void)setSteadyViewFaded:(BOOL)hidden;
- (void)setEditViewFaded:(BOOL)hidden;
- (void)setEditViewHidden:(BOOL)hidden;
- (void)setSteadyViewHidden:(BOOL)hidden;

// Resets tranforms of edit and steady view. Used for post-animation cleanup.
// Only resets the translation, and leaves scale intact.
- (void)resetTransforms;

@end

#endif  // IOS_CHROME_BROWSER_ORCHESTRATOR_UI_BUNDLED_LOCATION_BAR_ANIMATEE_H_
