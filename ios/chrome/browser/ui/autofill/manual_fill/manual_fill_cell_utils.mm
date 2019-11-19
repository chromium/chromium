// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_cell_utils.h"

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/chip_button.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Horizontal spacing between views in |AppendHorizontalConstraintsForViews|.
constexpr CGFloat kHorizontalSpacing = 16;
}  // namespace

UIButton* CreateChipWithSelectorAndTarget(SEL action, id target) {
  UIButton* button = [ChipButton buttonWithType:UIButtonTypeCustom];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.titleLabel.adjustsFontForContentSizeCategory = YES;
  [button addTarget:target
                action:action
      forControlEvents:UIControlEventTouchUpInside];
  return button;
}

void AppendVerticalConstraintsSpacingForViews(
    NSMutableArray<NSLayoutConstraint*>* constraints,
    NSArray<UIView*>* views,
    UIView* container) {
  AppendVerticalConstraintsSpacingForViews(constraints, views, container,
                                           TopSystemSpacingMultiplier,
                                           BottomSystemSpacingMultiplier);
}

void AppendVerticalConstraintsSpacingForViews(
    NSMutableArray<NSLayoutConstraint*>* constraints,
    NSArray<UIView*>* views,
    UIView* container,
    CGFloat topSystemSpacingMultiplier,
    CGFloat bottomSystemSpacingMultiplier) {
  // Multipliers of these constraints are calculated based on a 24 base
  // system spacing.
  NSLayoutYAxisAnchor* previousAnchor = container.topAnchor;
  CGFloat multiplier = topSystemSpacingMultiplier;
  for (UIView* view in views) {
    [constraints
        addObject:[view.topAnchor
                      constraintEqualToSystemSpacingBelowAnchor:previousAnchor
                                                     multiplier:multiplier]];
    multiplier = 1.0;
    previousAnchor = view.bottomAnchor;
  }
  multiplier = bottomSystemSpacingMultiplier;
  [constraints
      addObject:[container.bottomAnchor
                    constraintEqualToSystemSpacingBelowAnchor:previousAnchor
                                                   multiplier:multiplier]];
}

void AppendHorizontalConstraintsForViews(
    NSMutableArray<NSLayoutConstraint*>* constraints,
    NSArray<UIView*>* views,
    UIView* guide) {
  AppendHorizontalConstraintsForViews(constraints, views, guide, 0);
}

void AppendHorizontalConstraintsForViews(
    NSMutableArray<NSLayoutConstraint*>* constraints,
    NSArray<UIView*>* views,
    UIView* guide,
    CGFloat margin) {
  AppendHorizontalConstraintsForViews(constraints, views, guide, margin, 0);
}

void AppendHorizontalConstraintsForViews(
    NSMutableArray<NSLayoutConstraint*>* constraints,
    NSArray<UIView*>* views,
    UIView* guide,
    CGFloat margin,
    AppendConstraints options) {
  if (views.count == 0)
    return;

  NSLayoutXAxisAnchor* previousAnchor = guide.leadingAnchor;

  BOOL isFirstView = YES;
  for (UIView* view in views) {
    CGFloat constant = isFirstView ? margin : kHorizontalSpacing;
    [constraints
        addObject:[view.leadingAnchor constraintEqualToAnchor:previousAnchor
                                                     constant:constant]];
    [view setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                          forAxis:
                                              UILayoutConstraintAxisHorizontal];
    [view setContentHuggingPriority:UILayoutPriorityDefaultHigh
                            forAxis:UILayoutConstraintAxisHorizontal];
    previousAnchor = view.trailingAnchor;
    isFirstView = NO;
  }

  if (options & AppendConstraintsHorizontalEqualOrSmallerThanGuide) {
    [constraints
        addObject:[views.lastObject.trailingAnchor
                      constraintLessThanOrEqualToAnchor:guide.trailingAnchor
                                               constant:-margin]];

  } else {
    [constraints addObject:[views.lastObject.trailingAnchor
                               constraintEqualToAnchor:guide.trailingAnchor
                                              constant:-margin]];
    // Give all remaining space to the last button, minus margin, as per UX.
    [views.lastObject
        setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    [views.lastObject
        setContentHuggingPriority:UILayoutPriorityDefaultLow
                          forAxis:UILayoutConstraintAxisHorizontal];
  }
  if (options & AppendConstraintsHorizontalSyncBaselines) {
    AppendEqualBaselinesConstraints(constraints, views);
  }
}

void AppendEqualBaselinesConstraints(
    NSMutableArray<NSLayoutConstraint*>* constraints,
    NSArray<UIView*>* views) {
  UIView* leadingView = views.firstObject;
  for (UIView* view in views) {
    DCHECK([view isKindOfClass:[UIButton class]] ||
           [view isKindOfClass:[UILabel class]]);
    if (view == leadingView)
      continue;
    [constraints
        addObject:[view.lastBaselineAnchor
                      constraintEqualToAnchor:leadingView.lastBaselineAnchor]];
  }
}

UILabel* CreateLabel() {
  UILabel* label = [[UILabel alloc] init];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.adjustsFontForContentSizeCategory = YES;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  return label;
}

UIView* CreateGraySeparatorForContainer(UIView* container) {
  UIView* grayLine = [[UIView alloc] init];
  grayLine.backgroundColor = [UIColor colorNamed:kSeparatorColor];
  grayLine.translatesAutoresizingMaskIntoConstraints = NO;
  [container addSubview:grayLine];

  id<LayoutGuideProvider> safeArea = container.safeAreaLayoutGuide;
  [NSLayoutConstraint activateConstraints:@[
    // Vertical constraints.
    [grayLine.bottomAnchor constraintEqualToAnchor:container.bottomAnchor],
    [grayLine.heightAnchor constraintEqualToConstant:1],
    // Horizontal constraints.
    [grayLine.leadingAnchor constraintEqualToAnchor:safeArea.leadingAnchor
                                           constant:kButtonHorizontalMargin],
    [safeArea.trailingAnchor constraintEqualToAnchor:grayLine.trailingAnchor
                                            constant:kButtonHorizontalMargin],
  ]];

  return grayLine;
}
