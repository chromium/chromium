// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/bricks/layout_brick.h"

#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Default spacing applied between elements in the stackView.
constexpr CGFloat kStackViewSpacing = 16;

}  // namespace

@implementation LayoutBrick

+ (UIStackView*)contentStackWithSubviews:(NSArray<UIView*>*)subviews {
  UIStackView* stackView =
      [[UIStackView alloc] initWithArrangedSubviews:subviews];
  stackView.axis = UILayoutConstraintAxisVertical;
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.spacing = kStackViewSpacing;
  return stackView;
}

@end
