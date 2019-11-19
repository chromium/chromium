// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_CELL_UTILS_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_CELL_UTILS_H_

#import <UIKit/UIKit.h>

namespace {

// Left and right margins of the cell content and buttons.
static const CGFloat kButtonHorizontalMargin = 16;

// Left and right margins for the chips.
static const CGFloat kChipsHorizontalMargin = -1;

// The multiplier for the base system spacing at the top margin.
static const CGFloat TopSystemSpacingMultiplier = 2;

// The multiplier for the base system spacing at the bottom margin.
static const CGFloat BottomSystemSpacingMultiplier = 2.26;

// Options for |AppendHorizontalConstraintsForViews|.
typedef NS_OPTIONS(NSUInteger, AppendConstraints) {
  AppendConstraintsNone = 0,
  // Add an equal constraint to the baselines.
  AppendConstraintsHorizontalSyncBaselines = 1 << 0,
  // The views can be constraint smaller than the guide.
  AppendConstraintsHorizontalEqualOrSmallerThanGuide = 1 << 1,
};

}  // namespace

// Creates a blank button in chip style, for the given |action| and |target|.
UIButton* CreateChipWithSelectorAndTarget(SEL action, id target);

// Adds vertical constraints to given list, laying |views| vertically (based on
// firstBaselineAnchor for the buttons or labels) inside |container|, starting
// at its topAnchor. Constrainst are not activated.  Default multipliers are
// applied.
void AppendVerticalConstraintsSpacingForViews(
    NSMutableArray<NSLayoutConstraint*>* constraints,
    NSArray<UIView*>* views,
    UIView* container);

// Adds vertical constraints like |AppendVerticalConstraintsSpacingForViews|
// above but using given mutipliers at top, bottom and in-between rows.
void AppendVerticalConstraintsSpacingForViews(
    NSMutableArray<NSLayoutConstraint*>* constraints,
    NSArray<UIView*>* views,
    UIView* container,
    CGFloat topSystemSpacingMultiplier,
    CGFloat BottomSystemSpacingMultiplier);

// Adds constraints to the given list, for the given |views|, so as to lay them
// out horizontally, parallel to the |guide| view. Constraints are not
// activated.
void AppendHorizontalConstraintsForViews(
    NSMutableArray<NSLayoutConstraint*>* constraints,
    NSArray<UIView*>* views,
    UIView* guide);

// Adds constraints like |AppendHorizontalConstraintsForViews| above but also
// applies the given constant |margin| at both ends of the whole row.
void AppendHorizontalConstraintsForViews(
    NSMutableArray<NSLayoutConstraint*>* constraints,
    NSArray<UIView*>* views,
    UIView* guide,
    CGFloat margin);

// Adds constraints like |AppendHorizontalConstraintsForViews| above
// but with given |options|.
void AppendHorizontalConstraintsForViews(
    NSMutableArray<NSLayoutConstraint*>* constraints,
    NSArray<UIView*>* views,
    UIView* guide,
    CGFloat margin,
    AppendConstraints options);

// Adds all baseline anchor constraints for the given |views| to match the first
// one. Constrainst are not activated.
void AppendEqualBaselinesConstraints(
    NSMutableArray<NSLayoutConstraint*>* constraints,
    NSArray<UIView*>* views);

// Creates a blank label with autoresize mask off and adjustable font size.
UILabel* CreateLabel();

// Creates a gray horizontal line separator, with the same margin as the other
// components here. The gray line is added to the given |container| and proper
// constraints are enabled to keep the line at the bottom of the container and
// within the horizontal safe area.
UIView* CreateGraySeparatorForContainer(UIView* container);

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_CELL_UTILS_H_
