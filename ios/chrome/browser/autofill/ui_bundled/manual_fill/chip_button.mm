// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/chip_button.h"

#import "base/apple/foundation_util.h"
#import "base/ios/ios_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// Leading and trailing padding for the button.
constexpr CGFloat kChipHorizontalPadding = 12;

// Top and bottom padding for the button.
constexpr CGFloat kChipVerticalPadding = 11.5;

// Minimal height and width for the button.
constexpr CGFloat kChipMinSize = 44;

// Font size for the button's title.
constexpr CGFloat kFontSize = 14;

// Line spacing for the button's title.
constexpr CGFloat kLineSpacing = 6;

}  // namespace

@interface ChipButton ()

// Attributes of the button's title.
@property(strong, nonatomic) NSDictionary* titleAttributes;

@end

@implementation ChipButton

- (id)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    [self initializeStyling];
  }
  return self;
}

- (id)initWithCoder:(NSCoder*)aDecoder {
  self = [super initWithCoder:aDecoder];
  if (self) {
    [self initializeStyling];
  }
  return self;
}

- (void)awakeFromNib {
  [super awakeFromNib];
  [self initializeStyling];
}

- (void)setHighlighted:(BOOL)highlighted {
  [super setHighlighted:highlighted];
  UIButtonConfiguration* buttonConfiguration = self.configuration;
  UIBackgroundConfiguration* backgroundConfiguration =
      buttonConfiguration.background;
  backgroundConfiguration.backgroundColor =
      highlighted ? [UIColor colorNamed:kGrey300Color]
                  : [UIColor colorNamed:kGrey100Color];
  buttonConfiguration.background = backgroundConfiguration;
  self.configuration = buttonConfiguration;
}

- (void)setEnabled:(BOOL)enabled {
  [super setEnabled:enabled];
  UIButtonConfiguration* buttonConfiguration = self.configuration;
  buttonConfiguration.contentInsets = enabled
                                          ? [self chipNSDirectionalEdgeInsets]
                                          : NSDirectionalEdgeInsetsZero;
  UIBackgroundConfiguration* backgroundConfiguration =
      buttonConfiguration.background;
  backgroundConfiguration.backgroundColor =
      enabled ? [UIColor colorNamed:kGrey100Color] : UIColor.clearColor;
  buttonConfiguration.background = backgroundConfiguration;
  self.configuration = buttonConfiguration;
}

#pragma mark - Getters

- (NSDictionary*)titleAttributes {
  if (_titleAttributes) {
    return _titleAttributes;
  }

  UIFont* font = [UIFont systemFontOfSize:kFontSize weight:UIFontWeightMedium];
  NSMutableParagraphStyle* paragraphStyle =
      [[NSMutableParagraphStyle alloc] init];
  paragraphStyle.lineSpacing = kLineSpacing;
  _titleAttributes = @{
    NSFontAttributeName :
        [[UIFontMetrics defaultMetrics] scaledFontForFont:font],
    NSParagraphStyleAttributeName : paragraphStyle,
  };

  return _titleAttributes;
}

#pragma mark - Setters

- (void)setTitle:(NSString*)title forState:(UIControlState)state {
  UIButtonConfiguration* buttonConfiguration = self.configuration;
  NSAttributedString* attributedTitle =
      [[NSAttributedString alloc] initWithString:title
                                      attributes:self.titleAttributes];
  buttonConfiguration.attributedTitle = attributedTitle;
  self.configuration = buttonConfiguration;
}

#pragma mark - Private

- (NSDirectionalEdgeInsets)chipNSDirectionalEdgeInsets {
  return NSDirectionalEdgeInsetsMake(
      kChipVerticalPadding, kChipHorizontalPadding, kChipVerticalPadding,
      kChipHorizontalPadding);
}

- (void)initializeStyling {
  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  buttonConfiguration.contentInsets = [self chipNSDirectionalEdgeInsets];
  buttonConfiguration.baseForegroundColor =
      [UIColor colorNamed:kTextPrimaryColor];
  buttonConfiguration.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
  UIBackgroundConfiguration* backgroundConfiguration =
      [UIBackgroundConfiguration clearConfiguration];
  backgroundConfiguration.backgroundColor = [UIColor colorNamed:kGrey100Color];
  buttonConfiguration.background = backgroundConfiguration;
  self.configuration = buttonConfiguration;

  [NSLayoutConstraint activateConstraints:@[
    [self.heightAnchor constraintGreaterThanOrEqualToConstant:kChipMinSize],
    [self.widthAnchor constraintGreaterThanOrEqualToConstant:kChipMinSize],
  ]];

  self.translatesAutoresizingMaskIntoConstraints = NO;

  self.titleLabel.adjustsFontForContentSizeCategory = YES;
}

@end
