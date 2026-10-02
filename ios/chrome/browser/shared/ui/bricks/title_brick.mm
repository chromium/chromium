// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/bricks/title_brick.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/ui/brick_configurations/title_brick_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Spacing between title and subtitle.
constexpr CGFloat kVerticalSpacing = 16;

// Creates and configures a UILabel with the desired string.
UILabel* ConfiguredLabelWithText(NSString* text) {
  UILabel* label = [[UILabel alloc] init];
  label.numberOfLines = 0;
  label.text = text;
  label.textAlignment = NSTextAlignmentCenter;
  label.textColor = [UIColor colorNamed:kTextPrimaryColor];
  label.adjustsFontForContentSizeCategory = YES;
  label.translatesAutoresizingMaskIntoConstraints = NO;
  return label;
}

}  // namespace

@implementation TitleBrick {
  // This view's configuration object.
  TitleBrickConfiguration* _config;
}

- (instancetype)initWithConfiguration:
    (TitleBrickConfiguration*)titleBrickConfiguration {
  CHECK(titleBrickConfiguration.title.length > 0 ||
        titleBrickConfiguration.subtitle.length > 0);
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _config = titleBrickConfiguration;
    if (!_config.titleStyle) {
      _config.titleStyle = UIFontTextStyleTitle1;
    }
    if (!_config.subtitleStyle) {
      _config.subtitleStyle = UIFontTextStyleBody;
    }

    UIStackView* stackView = [[UIStackView alloc] init];
    stackView.axis = UILayoutConstraintAxisVertical;
    stackView.alignment = UIStackViewAlignmentCenter;
    stackView.spacing = kVerticalSpacing;
    stackView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:stackView];

    AddSameConstraints(stackView, self);

    if (_config.title.length > 0) {
      UILabel* titleView = ConfiguredLabelWithText(_config.title);
      titleView.accessibilityTraits = UIAccessibilityTraitHeader;

      UIFontDescriptor* descriptor = [UIFontDescriptor
          preferredFontDescriptorWithTextStyle:_config.titleStyle];
      UIFont* baseFont = [UIFont systemFontOfSize:descriptor.pointSize
                                           weight:UIFontWeightBold];
      UIFontMetrics* fontMetrics =
          [UIFontMetrics metricsForTextStyle:_config.titleStyle];
      titleView.font = [fontMetrics scaledFontForFont:baseFont];

      [stackView addArrangedSubview:titleView];
    }

    if (_config.subtitle.length > 0) {
      UILabel* subtitleView = ConfiguredLabelWithText(_config.subtitle);
      subtitleView.font =
          [UIFont preferredFontForTextStyle:_config.subtitleStyle];
      subtitleView.textColor = [UIColor colorNamed:kTextSecondaryColor];

      [stackView addArrangedSubview:subtitleView];
    }
  }
  return self;
}

@end
