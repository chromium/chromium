// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/chip_button.h"

#import "base/apple/foundation_util.h"
#import "base/ios/ios_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {
// Top and bottom padding for the button.
static const CGFloat kChipVerticalPadding = 14;
// Left and right padding for the button.
static const CGFloat kChipHorizontalPadding = 14;
// Vertical margins for the button. How much bigger the tap target is.
static const CGFloat kChipVerticalMargin = 4;
// Font size for the button's title.
static const CGFloat kFontSize = 14;
}  // namespace

@interface ChipButton ()

// Gray rounded background view which gives the aspect of a chip.
@property(strong, nonatomic) UIView* backgroundView;

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

- (void)layoutSubviews {
  [super layoutSubviews];
  self.backgroundView.layer.cornerRadius =
      self.backgroundView.bounds.size.height / 2.0;
}

- (void)setHighlighted:(BOOL)highlighted {
  [super setHighlighted:highlighted];
  self.backgroundView.backgroundColor =
      highlighted ? [UIColor colorNamed:kGrey300Color]
                  : [UIColor colorNamed:kGrey100Color];
}

- (void)setEnabled:(BOOL)enabled {
  [super setEnabled:enabled];
  self.backgroundView.hidden = !enabled;
  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  buttonConfiguration.contentInsets = enabled
                                          ? [self chipNSDirectionalEdgeInsets]
                                          : NSDirectionalEdgeInsetsZero;
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
  _titleAttributes = @{NSFontAttributeName : font};

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
  return NSDirectionalEdgeInsetsMake(
      kChipVerticalPadding, kChipHorizontalPadding, kChipVerticalPadding,
      kChipHorizontalPadding);
}

- (void)initializeStyling {
  _backgroundView = [[UIView alloc] init];
  _backgroundView.userInteractionEnabled = NO;
  _backgroundView.backgroundColor = [UIColor colorNamed:kGrey100Color];
  _backgroundView.translatesAutoresizingMaskIntoConstraints = NO;

  [self addSubview:_backgroundView];
  [self sendSubviewToBack:_backgroundView];
  [NSLayoutConstraint activateConstraints:@[
    [_backgroundView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [_backgroundView.trailingAnchor
        constraintEqualToAnchor:self.trailingAnchor],
    [_backgroundView.topAnchor constraintEqualToAnchor:self.topAnchor
                                              constant:kChipVerticalMargin],
    [_backgroundView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor
                                                 constant:-kChipVerticalMargin]
  ]];

  self.translatesAutoresizingMaskIntoConstraints = NO;

  self.titleLabel.adjustsFontForContentSizeCategory = YES;

  [self updateTitleLabelFont];
  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  buttonConfiguration.contentInsets = [self chipNSDirectionalEdgeInsets];
  buttonConfiguration.baseForegroundColor =
      [UIColor colorNamed:kTextPrimaryColor];
  self.configuration = buttonConfiguration;
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
