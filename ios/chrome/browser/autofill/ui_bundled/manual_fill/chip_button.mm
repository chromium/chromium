// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/chip_button.h"

#import "base/apple/foundation_util.h"
#import "base/ios/ios_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// Padding for the button. Used when the Keyboard Accessory Upgrade feature is
// disabled.
constexpr CGFloat kChipPadding = 14;

// Leading and trailing padding for the button. Used when the
// Keyboard Accessory Upgrade feature is enabled.
constexpr CGFloat kChipHorizontalPadding = 12;

// Top and bottom padding for the button. Used when the
// Keyboard Accessory Upgrade feature is enabled.
constexpr CGFloat kChipVerticalPadding = 11.5;

// Minimal height and width for the button.
constexpr CGFloat kChipMinSize = 44;

// Font size for the button's title.
constexpr CGFloat kFontSize = 14;

// Line spacing for the button's title.
constexpr CGFloat kLineSpacing = 6;

// Returns the horizontal padding to use.
CGFloat GetChipHorizontalPadding() {
  return IsKeyboardAccessoryUpgradeEnabled() ? kChipHorizontalPadding
                                             : kChipPadding;
}

// Returns the vertical padding to use.
CGFloat GetChipVerticalPadding() {
  return IsKeyboardAccessoryUpgradeEnabled() ? kChipVerticalPadding
                                             : kChipPadding;
}

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
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(updateTitleLabelFont)
               name:UIContentSizeCategoryDidChangeNotification
             object:nil];
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
  // `titleAttributes` shouldn't be accessed if the Keyboard Accessory Upgrade
  // is disabled.
  CHECK(IsKeyboardAccessoryUpgradeEnabled());

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
  if (IsKeyboardAccessoryUpgradeEnabled()) {
    UIButtonConfiguration* buttonConfiguration = self.configuration;
    NSAttributedString* attributedTitle =
        [[NSAttributedString alloc] initWithString:title
                                        attributes:self.titleAttributes];
    buttonConfiguration.attributedTitle = attributedTitle;
    self.configuration = buttonConfiguration;
  } else {
    [super setTitle:title forState:state];
  }
}

#pragma mark - Private

- (NSDirectionalEdgeInsets)chipNSDirectionalEdgeInsets {
  CGFloat horizontalPadding = GetChipHorizontalPadding();
  CGFloat verticalPadding = GetChipVerticalPadding();

  return NSDirectionalEdgeInsetsMake(verticalPadding, horizontalPadding,
                                     verticalPadding, horizontalPadding);
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

  if (IsKeyboardAccessoryUpgradeEnabled()) {
    [NSLayoutConstraint activateConstraints:@[
      [self.heightAnchor constraintGreaterThanOrEqualToConstant:kChipMinSize],
      [self.widthAnchor constraintGreaterThanOrEqualToConstant:kChipMinSize],
    ]];
  }

  self.translatesAutoresizingMaskIntoConstraints = NO;

  self.titleLabel.adjustsFontForContentSizeCategory = YES;

  [self updateTitleLabelFont];

}

- (void)updateTitleLabelFont {
  // With the Keyboard Accessory Upgrade feature, the title's font is applied
  // through the button's `attributedTitle`.
  if (IsKeyboardAccessoryUpgradeEnabled()) {
    return;
  }

  UIFont* font = [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  UIFontDescriptor* boldFontDescriptor = [font.fontDescriptor
      fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitBold];
  DCHECK(boldFontDescriptor);
  self.titleLabel.font = [UIFont fontWithDescriptor:boldFontDescriptor size:0];
}

@end
