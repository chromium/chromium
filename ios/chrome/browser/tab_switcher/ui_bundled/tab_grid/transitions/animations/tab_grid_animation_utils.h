// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TRANSITIONS_ANIMATIONS_TAB_GRID_ANIMATION_UTILS_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TRANSITIONS_ANIMATIONS_TAB_GRID_ANIMATION_UTILS_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

// Creates and returns a CAShapeLayer mask with a rectangular path matching the
// provided `frame`.
CAShapeLayer* CreateTabGridAnimationMaskWithFrame(CGRect frame);

// Creates and returns a UIBezierPath for a rounded rectangle matching `frame`,
// inset vertically by `top_inset` and `bottom_inset`.
UIBezierPath* CreateTabGridAnimationRoundedRectPathWithInsets(
    CGRect frame,
    CGFloat bottom_inset,
    CGFloat top_inset,
    CGFloat corner_radius);

// Creates and returns a UIView with a background color matching `is_incognito`.
UIView* CreateTabGridAnimationBackgroundView(bool is_incognito);

// Creates a background view for a toolbar snapshot, adds the snapshot to it,
// and positions it within the animated view.
UIView* CreateToolbarSnapshotBackgroundView(UIView* snapshot_view,
                                            UIView* animated_view,
                                            BOOL is_incognito,
                                            BOOL align_to_bottom,
                                            CGRect reference_frame);

// Calculates the relative center of `frame` within `view` and sets it as the
// `anchorPoint` for `view.layer`.
void SetAnchorPointToFrameCenter(UIView* view, CGRect frame);

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TRANSITIONS_ANIMATIONS_TAB_GRID_ANIMATION_UTILS_H_
