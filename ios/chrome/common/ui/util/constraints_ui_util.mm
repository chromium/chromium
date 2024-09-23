// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#import <UIKit/UIKit.h>

#import "base/check.h"

@implementation ComposedEdgeLayoutGuide
- (NSLayoutXAxisAnchor*)leadingAnchor {
  return self.leadingAnchorProvider.leadingAnchor
             ?: self.baseLayoutGuide.leadingAnchor;
}
- (NSLayoutXAxisAnchor*)trailingAnchor {
  return self.trailingAnchorProvider.trailingAnchor
             ?: self.baseLayoutGuide.trailingAnchor;
}
- (NSLayoutYAxisAnchor*)topAnchor {
  return self.topAnchorProvider.topAnchor ?: self.baseLayoutGuide.topAnchor;
}
- (NSLayoutYAxisAnchor*)bottomAnchor {
  return self.bottomAnchorProvider.bottomAnchor
             ?: self.baseLayoutGuide.bottomAnchor;
}
@end

void ApplyVisualConstraints(NSArray* constraints,
                            NSDictionary* subviewsDictionary) {
  ApplyVisualConstraintsWithMetricsAndOptions(constraints, subviewsDictionary,
                                              nil, 0);
}

void ApplyVisualConstraintsWithMetrics(NSArray* constraints,
                                       NSDictionary* subviewsDictionary,
                                       NSDictionary* metrics) {
  ApplyVisualConstraintsWithMetricsAndOptions(constraints, subviewsDictionary,
                                              metrics, 0);
}

void ApplyVisualConstraintsWithMetricsAndOptions(
    NSArray* constraints,
    NSDictionary* subviewsDictionary,
    NSDictionary* metrics,
    NSLayoutFormatOptions options) {
  NSArray* layoutConstraints = VisualConstraintsWithMetricsAndOptions(
      constraints, subviewsDictionary, metrics, options);
  [NSLayoutConstraint activateConstraints:layoutConstraints];
}

NSArray* VisualConstraintsWithMetrics(NSArray* constraints,
                                      NSDictionary* subviewsDictionary,
                                      NSDictionary* metrics) {
  return VisualConstraintsWithMetricsAndOptions(constraints, subviewsDictionary,
                                                metrics, 0);
}

NSArray* VisualConstraintsWithMetricsAndOptions(
    NSArray* constraints,
    NSDictionary* subviewsDictionary,
    NSDictionary* metrics,
    NSLayoutFormatOptions options) {
  NSMutableArray* layoutConstraints = [NSMutableArray array];
  for (NSString* constraint in constraints) {
    DCHECK([constraint isKindOfClass:[NSString class]]);
    [layoutConstraints addObjectsFromArray:
                           [NSLayoutConstraint
                               constraintsWithVisualFormat:constraint
                                                   options:options
                                                   metrics:metrics
                                                     views:subviewsDictionary]];
  }
  return [layoutConstraints copy];
}

void AddSameCenterConstraints(id<LayoutGuideProvider> view1,
                              id<LayoutGuideProvider> view2) {
  AddSameCenterXConstraint(view1, view2);
  AddSameCenterYConstraint(view1, view2);
}

void AddSameCenterXConstraint(id<LayoutGuideProvider> view1,
                              id<LayoutGuideProvider> view2) {
  [view1.centerXAnchor constraintEqualToAnchor:view2.centerXAnchor].active =
      YES;
}

void AddSameCenterXConstraint(UIView* unused_parentView,
                              id<LayoutGuideProvider> subview1,
                              id<LayoutGuideProvider> subview2) {
  AddSameCenterXConstraint(subview1, subview2);
}

void AddSameCenterYConstraint(id<LayoutGuideProvider> view1,
                              id<LayoutGuideProvider> view2) {
  [view1.centerYAnchor constraintEqualToAnchor:view2.centerYAnchor].active =
      YES;
}

void AddSameCenterYConstraint(UIView* unused_parentView,
                              id<LayoutGuideProvider> subview1,
                              id<LayoutGuideProvider> subview2) {
  AddSameCenterYConstraint(subview1, subview2);
}

void AddSameConstraints(id<EdgeLayoutGuideProvider> view1,
                        id<EdgeLayoutGuideProvider> view2) {
  [NSLayoutConstraint activateConstraints:@[
    [view1.leadingAnchor constraintEqualToAnchor:view2.leadingAnchor],
    [view1.trailingAnchor constraintEqualToAnchor:view2.trailingAnchor],
    [view1.topAnchor constraintEqualToAnchor:view2.topAnchor],
    [view1.bottomAnchor constraintEqualToAnchor:view2.bottomAnchor]
  ]];
}

void AddSameConstraintsWithInsets(id<EdgeLayoutGuideProvider> innerView,
                                  id<EdgeLayoutGuideProvider> outerView,
                                  NSDirectionalEdgeInsets insets) {
  AddSameConstraintsToSidesWithInsets(
      innerView, outerView,
      (LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kBottom |
       LayoutSides::kTrailing),
      insets);
}

void AddSameConstraintsWithInset(id<EdgeLayoutGuideProvider> innerView,
                                 id<EdgeLayoutGuideProvider> outerView,
                                 CGFloat inset) {
  AddSameConstraintsWithInsets(
      innerView, outerView,
      NSDirectionalEdgeInsets(inset, inset, inset, inset));
}

void PinToSafeArea(id<EdgeLayoutGuideProvider> innerView, UIView* outerView) {
  AddSameConstraints(innerView, outerView.safeAreaLayoutGuide);
}

void AddSameConstraintsToSides(id<EdgeLayoutGuideProvider> view1,
                               id<EdgeLayoutGuideProvider> view2,
                               LayoutSides side_flags) {
  AddSameConstraintsToSidesWithInsets(view1, view2, side_flags,
                                      NSDirectionalEdgeInsetsZero);
}

void AddSameConstraintsToSidesWithInsets(id<EdgeLayoutGuideProvider> innerView,
                                         id<EdgeLayoutGuideProvider> outerView,
                                         LayoutSides side_flags,
                                         NSDirectionalEdgeInsets insets) {
  NSMutableArray* constraints = [[NSMutableArray alloc] init];
  if (IsLayoutSidesMaskSet(side_flags, LayoutSides::kTop)) {
    [constraints addObject:[innerView.topAnchor
                               constraintEqualToAnchor:outerView.topAnchor
                                              constant:insets.top]];
  }
  if (IsLayoutSidesMaskSet(side_flags, LayoutSides::kLeading)) {
    [constraints addObject:[innerView.leadingAnchor
                               constraintEqualToAnchor:outerView.leadingAnchor
                                              constant:insets.leading]];
  }
  if (IsLayoutSidesMaskSet(side_flags, LayoutSides::kBottom)) {
    [constraints addObject:[innerView.bottomAnchor
                               constraintEqualToAnchor:outerView.bottomAnchor
                                              constant:-insets.bottom]];
  }
  if (IsLayoutSidesMaskSet(side_flags, LayoutSides::kTrailing)) {
    [constraints addObject:[innerView.trailingAnchor
                               constraintEqualToAnchor:outerView.trailingAnchor
                                              constant:-insets.trailing]];
  }

  [NSLayoutConstraint activateConstraints:constraints];
}

void AddSizeConstraints(id<LayoutGuideProvider> view, CGSize size) {
  [NSLayoutConstraint activateConstraints:@[
    [view.widthAnchor constraintEqualToConstant:size.width],
    [view.heightAnchor constraintEqualToConstant:size.height],
  ]];
}

void AddSquareConstraints(id<LayoutGuideProvider> view, CGFloat edge) {
  AddSizeConstraints(view, CGSize(edge, edge));
}

NSArray<NSLayoutConstraint*>* AddOptionalVerticalPadding(
    id<EdgeLayoutGuideProvider> outerView,
    id<EdgeLayoutGuideProvider> innerView,
    CGFloat padding) {
  return AddOptionalVerticalPadding(outerView, innerView, innerView, padding);
}

NSArray<NSLayoutConstraint*>* AddOptionalVerticalPadding(
    id<EdgeLayoutGuideProvider> outerView,
    id<EdgeLayoutGuideProvider> topInnerView,
    id<EdgeLayoutGuideProvider> bottomInnerView,
    CGFloat padding) {
  NSLayoutConstraint* topPaddingConstraint = [topInnerView.topAnchor
      constraintGreaterThanOrEqualToAnchor:outerView.topAnchor
                                  constant:padding];
  topPaddingConstraint.priority = UILayoutPriorityDefaultLow;
  NSLayoutConstraint* bottomPaddingConstraint = [bottomInnerView.bottomAnchor
      constraintLessThanOrEqualToAnchor:outerView.bottomAnchor
                               constant:-padding];
  bottomPaddingConstraint.priority = UILayoutPriorityDefaultLow;
  NSArray<NSLayoutConstraint*>* contraints =
      @[ topPaddingConstraint, bottomPaddingConstraint ];
  [NSLayoutConstraint activateConstraints:contraints];
  return contraints;
}

NSLayoutConstraint* VerticalConstraintsWithInset(UIView* innerView,
                                                 UIView* outerView,
                                                 CGFloat inset) {
  NSLayoutConstraint* heightConstraint =
      [outerView.heightAnchor constraintEqualToAnchor:innerView.heightAnchor
                                             constant:inset];
  heightConstraint.priority = UILayoutPriorityDefaultLow;
  heightConstraint.active = YES;
  return heightConstraint;
}
