// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/button_stack/button_stack_utils.h"

#import "ios/chrome/common/ui/button_stack/button_stack_constants.h"
#import "ios/chrome/common/ui/util/ui_util.h"

namespace {

// Percentage width on iPad for the promo style view controller.
CGFloat PromoStyleContentWidthMultiplier(UITraitCollection* traitCollection) {
  if (traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassCompact) {
    return .9;
  }
  return .8;
}

// Optimal width for the button stack content.
constexpr CGFloat kButtonStackContentOptimalWidth = 327;

}  // namespace

UILayoutGuide* AddButtonStackContentWidthLayoutGuide(UIView* view) {
  // Create a layout guide to constrain the width of the content, while still
  // allowing the scroll view to take the full screen width.
  UILayoutGuide* widthLayoutGuide = [[UILayoutGuide alloc] init];
  [view addLayoutGuide:widthLayoutGuide];
  // Content width layout guide constraints. Constrain the width to both at
  // least 80% of the view width, and to the full view width with margins.
  // This is to accomodate the iPad layout, which cannot be isolated out using
  // the traitCollection because of the FormSheet presentation style
  // (iPad FormSheet is considered compact).
  [NSLayoutConstraint activateConstraints:@[
    [widthLayoutGuide.centerXAnchor
        constraintEqualToAnchor:view.safeAreaLayoutGuide.centerXAnchor],
    [widthLayoutGuide.widthAnchor
        constraintGreaterThanOrEqualToAnchor:view.safeAreaLayoutGuide
                                                 .widthAnchor
                                  multiplier:PromoStyleContentWidthMultiplier(
                                                 view.traitCollection)],
    [widthLayoutGuide.widthAnchor
        constraintLessThanOrEqualToAnchor:view.safeAreaLayoutGuide.widthAnchor
                                 constant:-2 * kButtonStackMargin],
  ]];
  // This constraint is added to enforce that the content width should be as
  // close to the optimal width as possible, within the range already activated
  // for "widthLayoutGuide.widthAnchor" previously, with a higher priority.
  // In this case, the content width in iPad and iPhone landscape mode should be
  // the safe layout width multiplied by PromoStyleContentWidthMultiplier(),
  // while the content width for a iPhone portrait mode should be
  // kButtonStackContentOptimalWidth.
  NSLayoutConstraint* contentLayoutGuideWidthConstraint =
      [widthLayoutGuide.widthAnchor
          constraintEqualToConstant:kButtonStackContentOptimalWidth];
  contentLayoutGuideWidthConstraint.priority = UILayoutPriorityRequired - 1;
  contentLayoutGuideWidthConstraint.active = YES;

  return widthLayoutGuide;
}
