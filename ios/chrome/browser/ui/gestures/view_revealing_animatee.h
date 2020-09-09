// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_GESTURES_VIEW_REVEALING_ANIMATEE_H_
#define IOS_CHROME_BROWSER_UI_GESTURES_VIEW_REVEALING_ANIMATEE_H_

// Protocol defining an interface to handle animations from the view revealing
// pan gesture handler.
@protocol ViewRevealingAnimatee <NSObject>

// Called before a view reveal animation. Takes as argument whether the view is
// currently revealed or not.
- (void)willAnimateViewReveal:(BOOL)viewRevealed;

// Called inside an animation block to animate the revealing of the view. Takes
// as argument whether the view is currently revealed or not.
- (void)animateViewReveal:(BOOL)viewRevealed;

// Called inside the completion block of a view reveal animation. Takes as
// argument whether the view is currently revealed or not.
- (void)didAnimateViewReveal:(BOOL)viewRevealed;

@end

#endif  // IOS_CHROME_BROWSER_UI_GESTURES_VIEW_REVEALING_ANIMATEE_H_
