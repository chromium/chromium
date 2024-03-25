// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_cell_utils.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/chip_button.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
// Horizontal spacing between views in `AppendHorizontalConstraintsForViews`.
constexpr CGFloat kHorizontalSpacing = 16;
}  // namespace

const CGFloat kCellHorizontalMargin = 16;
const CGFloat kChipsHorizontalMargin = -1;
const CGFloat TopSystemSpacingMultiplier = 2;
const CGFloat BottomSystemSpacingMultiplier = 2.26;

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
    UILayoutGuide* layout_guide) {
  AppendHorizontalConstraintsForViews(constraints, views, layout_guide, 0);
}

void AppendHorizontalConstraintsForViews(
    NSMutableArray<NSLayoutConstraint*>* constraints,
    NSArray<UIView*>* views,
    UILayoutGuide* layout_guide,
    CGFloat margin) {
  AppendHorizontalConstraintsForViews(constraints, views, layout_guide, margin,
                                      0);
}

void AppendHorizontalConstraintsForViews(
    NSMutableArray<NSLayoutConstraint*>* constraints,
    NSArray<UIView*>* views,
    UILayoutGuide* layout_guide,
    CGFloat margin,
    AppendConstraints options) {
  if (views.count == 0) {
    return;
  }

  NSLayoutXAxisAnchor* previous_anchor = layout_guide.leadingAnchor;

  BOOL is_first_view = YES;
  for (UIView* view in views) {
    CGFloat spacing = is_first_view ? margin : kHorizontalSpacing;
    [constraints
        addObject:[view.leadingAnchor constraintEqualToAnchor:previous_anchor
                                                     constant:spacing]];
    [view setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                          forAxis:
                                              UILayoutConstraintAxisHorizontal];
    [view setContentHuggingPriority:UILayoutPriorityDefaultHigh
                            forAxis:UILayoutConstraintAxisHorizontal];
    previous_anchor = view.trailingAnchor;
    is_first_view = NO;
  }

  if (options & AppendConstraintsHorizontalEqualOrSmallerThanGuide) {
    [constraints
        addObject:[views.lastObject.trailingAnchor
                      constraintLessThanOrEqualToAnchor:layout_guide
                                                            .trailingAnchor
                                               constant:-margin]];

  } else {
    [constraints
        addObject:[views.lastObject.trailingAnchor
                      constraintEqualToAnchor:layout_guide.trailingAnchor
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
                                           constant:kCellHorizontalMargin],
    [safeArea.trailingAnchor constraintEqualToAnchor:grayLine.trailingAnchor
                                            constant:kCellHorizontalMargin],
  ]];

  return grayLine;
}

UILayoutGuide* AddLayoutGuideToContentView(UIView* content_view) {
  UILayoutGuide* layout_guide = [[UILayoutGuide alloc] init];
  [content_view addLayoutGuide:layout_guide];

  id<LayoutGuideProvider> safe_area = content_view.safeAreaLayoutGuide;
  [NSLayoutConstraint activateConstraints:@[
    [layout_guide.topAnchor constraintEqualToAnchor:content_view.topAnchor],
    [layout_guide.bottomAnchor
        constraintEqualToAnchor:content_view.bottomAnchor],
    [layout_guide.leadingAnchor constraintEqualToAnchor:safe_area.leadingAnchor
                                               constant:kCellHorizontalMargin],
    [layout_guide.trailingAnchor
        constraintEqualToAnchor:safe_area.trailingAnchor
                       constant:-kCellHorizontalMargin],
  ]];

  return layout_guide;
}
