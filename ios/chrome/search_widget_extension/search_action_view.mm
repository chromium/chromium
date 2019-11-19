// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/search_widget_extension/search_action_view.h"

#import <NotificationCenter/NotificationCenter.h>

#include "base/logging.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#import "ios/chrome/search_widget_extension/search_widget_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const CGFloat kActionButtonSize = 55;
const CGFloat kIconSize = 35;

}  // namespace

@implementation SearchActionView

- (instancetype)initWithActionTarget:(id)target
                      actionSelector:(SEL)actionSelector
                               title:(NSString*)title
                           imageName:(NSString*)imageName {
  DCHECK(target);
  self = [super initWithFrame:CGRectZero];
  if (self) {
    self.translatesAutoresizingMaskIntoConstraints = NO;

    UIVibrancyEffect* primaryEffect =
        [UIVibrancyEffect widgetPrimaryVibrancyEffect];
    UIVibrancyEffect* secondaryEffect =
        [UIVibrancyEffect widgetSecondaryVibrancyEffect];
    UIVibrancyEffect* iconBackgroundEffect =
        [UIVibrancyEffect widgetSecondaryVibrancyEffect];
    if (@available(iOS 13, *)) {
      primaryEffect = [UIVibrancyEffect
          widgetEffectForVibrancyStyle:UIVibrancyEffectStyleLabel];
      secondaryEffect = [UIVibrancyEffect
          widgetEffectForVibrancyStyle:UIVibrancyEffectStyleSecondaryLabel];
      iconBackgroundEffect = [UIVibrancyEffect
          widgetEffectForVibrancyStyle:UIVibrancyEffectStyleTertiaryFill];
    }

    UIVisualEffectView* primaryEffectView =
        [[UIVisualEffectView alloc] initWithEffect:primaryEffect];
    UIVisualEffectView* secondaryEffectView =
        [[UIVisualEffectView alloc] initWithEffect:secondaryEffect];
    UIVisualEffectView* iconBackgroundEffectView =
        [[UIVisualEffectView alloc] initWithEffect:iconBackgroundEffect];
    for (UIVisualEffectView* effectView in @[
           primaryEffectView, secondaryEffectView, iconBackgroundEffectView
         ]) {
      effectView.translatesAutoresizingMaskIntoConstraints = NO;
      effectView.userInteractionEnabled = NO;
    }

    UIView* circleView = [[UIView alloc] initWithFrame:CGRectZero];
    circleView.translatesAutoresizingMaskIntoConstraints = NO;
    if (@available(iOS 13, *)) {
      circleView.backgroundColor = UIColor.whiteColor;
    } else {
      circleView.backgroundColor = [UIColor colorWithWhite:0 alpha:0.05];
    }
    circleView.layer.cornerRadius = kActionButtonSize / 2;
    [iconBackgroundEffectView.contentView addSubview:circleView];
    AddSameConstraints(iconBackgroundEffectView.contentView, circleView);

    UILabel* labelView = [[UILabel alloc] initWithFrame:CGRectZero];
    labelView.translatesAutoresizingMaskIntoConstraints = NO;
    labelView.text = title;
    labelView.numberOfLines = 0;
    labelView.textAlignment = NSTextAlignmentCenter;
    labelView.font = [UIFont preferredFontForTextStyle:UIFontTextStyleCaption2];
    labelView.isAccessibilityElement = NO;
    [labelView
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:UILayoutConstraintAxisVertical];
    [secondaryEffectView.contentView addSubview:labelView];
    AddSameConstraints(secondaryEffectView.contentView, labelView);

    UIStackView* stack = [[UIStackView alloc] initWithArrangedSubviews:@[
      iconBackgroundEffectView, secondaryEffectView
    ]];
    stack.axis = UILayoutConstraintAxisVertical;
    stack.spacing = kIconSpacing;
    stack.alignment = UIStackViewAlignmentCenter;
    stack.translatesAutoresizingMaskIntoConstraints = NO;
    stack.userInteractionEnabled = NO;
    [self addSubview:stack];
    AddSameConstraints(self, stack);

    UIImage* iconImage = [UIImage imageNamed:imageName];
    iconImage =
        [iconImage imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    UIImageView* icon = [[UIImageView alloc] initWithImage:iconImage];
    icon.translatesAutoresizingMaskIntoConstraints = NO;
    [primaryEffectView.contentView addSubview:icon];
    AddSameConstraints(primaryEffectView.contentView, icon);
    [self addSubview:primaryEffectView];

    [NSLayoutConstraint activateConstraints:@[
      [circleView.widthAnchor constraintEqualToConstant:kActionButtonSize],
      [circleView.heightAnchor constraintEqualToConstant:kActionButtonSize],
      [icon.widthAnchor constraintEqualToConstant:kIconSize],
      [icon.heightAnchor constraintEqualToConstant:kIconSize],
      [icon.centerXAnchor constraintEqualToAnchor:circleView.centerXAnchor],
      [icon.centerYAnchor constraintEqualToAnchor:circleView.centerYAnchor],
    ]];

    self.userInteractionEnabled = YES;
    [self addTarget:target
                  action:actionSelector
        forControlEvents:UIControlEventTouchUpInside];
    self.accessibilityLabel = title;

    self.highlightableViews = @[ circleView, labelView, icon ];
  }
  return self;
}

@end
