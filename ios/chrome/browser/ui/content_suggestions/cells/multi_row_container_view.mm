// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/multi_row_container_view.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const CGFloat kSeparatorHeight = 0.5;

}  // namespace

@implementation MultiRowContainerView

- (instancetype)initWithViews:(NSArray<UIView*>*)views {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    UIStackView* rowsStackView = [[UIStackView alloc] init];
    rowsStackView.spacing = AlignValueToPixel(8.5);
    rowsStackView.axis = UILayoutConstraintAxisVertical;
    rowsStackView.translatesAutoresizingMaskIntoConstraints = NO;
    rowsStackView.alignment = UIStackViewAlignmentLeading;
    // Ensures that rows have similar height.
    rowsStackView.distribution = UIStackViewDistributionFillProportionally;
    NSUInteger index = 0;
    for (UIView* view in views) {
      [rowsStackView addArrangedSubview:view];
      if (index < [views count] - 1) {
        UIView* separator = [[UIView alloc] init];
        separator.backgroundColor = [UIColor colorNamed:kSeparatorColor];
        [rowsStackView addArrangedSubview:separator];
        [NSLayoutConstraint activateConstraints:@[
          [separator.heightAnchor
              constraintEqualToConstant:AlignValueToPixel(kSeparatorHeight)],
          [separator.leadingAnchor
              constraintEqualToAnchor:rowsStackView.leadingAnchor],
          [separator.trailingAnchor
              constraintEqualToAnchor:rowsStackView.trailingAnchor],
        ]];
      }
      index++;
    }
    [self addSubview:rowsStackView];
    AddSameConstraints(rowsStackView, self);
  }
  return self;
}

@end
