// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_UI_ELEMENT_H_
#define IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_UI_ELEMENT_H_

#import <UIKit/UIKit.h>

@class FullscreenAnimator;

// UI elements that need to react to Fullscreen events should conform to this
// protocol to react to changes in Fullscreen state.
@protocol FullscreenUIElement<NSObject>

// Tells the UI to update its state for `progress`.  A fullscreen `progress`
// value denotes that the toolbar should be completely visible, and a `progress`
// value of 0.0 denotes that the toolbar should be completely hidden.
//
// This selector is called for every scroll offset, it's not optional, as
// checking `-respondsToSelector:` for every FullscreenUIElement at every scroll
// offset can introduce performance issues.
//
// If the implementation of this selector uses batched layout updates (e.g.
// updating the UI in `-layoutSubviews` or by updating constraints), then a
// layout pass should be forced using `-setNeedsLayout` and `-layoutIfNeeded`.
// This will ensure that the layout is updated for each scroll position rather
// than batching multiple fullscreen progress updates together.  This is
// especially important for FullscreenUIElements that do not implement
// `-animateFullscreenWithAnimator:`, as this selctor is called by
// FullscreenUIUpdater in an animation block.
- (void)updateForFullscreenProgress:(CGFloat)progress;

@optional

// Tells the UI to update its state after the max and min viewport insets have
// been updated to new values.  `progress` is the current progress value, and
// can be used to update the UI at the current progress with the new viewport
// inset range.
- (void)updateForFullscreenMinViewportInsets:(UIEdgeInsets)minViewportInsets
                           maxViewportInsets:(UIEdgeInsets)maxViewportInsets;

// Tells the UI that fullscreen is enabled or disabled.  FullscreenUIUpdater's
// default behavior if this selector is not implemented is to call
// `-updateForFullscreenProgress:` with a progress value of 1.0.
- (void)updateForFullscreenEnabled:(BOOL)enabled;

// Called when fullscreen is about to initate an animation.
// FullscreenUIUpdater's default behavior if this selector is not implemented is
// to add an animation block calling `-updateForFullscreenProgress:` with a
// progress value of `animator.finalProgress`.
- (void)animateFullscreenWithAnimator:(FullscreenAnimator*)animator;

@end

#endif  // IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_UI_ELEMENT_H_
