// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_button_item.h"

#import "base/apple/foundation_util.h"
#import "base/ios/ios_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"

namespace {
// Alpha value for the disabled action button.
const CGFloat kDisabledButtonAlpha = 0.5;
// Vertical spacing between stackView and cell contentView.
const CGFloat kStackViewVerticalSpacing = 9.0;
// Horizontal spacing between stackView and cell contentView.
const CGFloat kStackViewHorizontalSpacing = 16.0;
// SubView spacing within stackView.
const CGFloat kStackViewSubViewSpacing = 13.0;
// Horizontal Inset between button contents and edge.
const CGFloat kButtonTitleHorizontalContentInset = 40.0;
// Vertical Inset between button contents and edge.
const CGFloat kButtonTitleVerticalContentInset = 8.0;
// Button corner radius.
const CGFloat kButtonCornerRadius = 8;
// The size of the checkmark symbol in the confirmation state on the
// item's button.
const CGFloat kSymbolConfirmationCheckmarkPointSize = 22;
// Default Text alignment.
const NSTextAlignment kDefaultTextAlignment = NSTextAlignmentCenter;
}  // namespace

@implementation TableViewTextButtonItem
@synthesize buttonAccessibilityIdentifier = _buttonAccessibilityIdentifier;
@synthesize buttonBackgroundColor = _buttonBackgroundColor;
@synthesize buttonText = _buttonText;
@synthesize text = _text;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewTextButtonCell class];
    _enabled = YES;
    _textAlignment = kDefaultTextAlignment;
    _boldButtonText = YES;
    _dimBackgroundWhenDisabled = YES;
    _showsActivityIndicator = NO;
    _showsCheckmark = NO;
  }
  return self;
}

- (void)configureCell:(TableViewCell*)tableCell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:tableCell withStyler:styler];
  TableViewTextButtonCell* cell =
      base::apple::ObjCCastStrict<TableViewTextButtonCell>(tableCell);
  [cell setSelectionStyle:UITableViewCellSelectionStyleNone];

  cell.textLabel.text = self.text;
  // Decide cell.textLabel.textColor in order:
  //   1. styler.cellTitleColor
  //   2. [UIColor colorNamed:kTextSecondaryColor]
  if (styler.cellTitleColor) {
    cell.textLabel.textColor = styler.cellTitleColor;
  } else {
    cell.textLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  }
  [cell enableItemSpacing:[self.text length]];
  [cell disableButtonIntrinsicWidth:self.disableButtonIntrinsicWidth];
  cell.textLabel.textAlignment = self.textAlignment;

  UIButtonConfiguration* buttonConfiguration = cell.button.configuration;
  UIFont* font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  NSDictionary* attributes = @{NSFontAttributeName : font};
  NSMutableAttributedString* attributedString =
      [[NSMutableAttributedString alloc] initWithString:self.buttonText
                                             attributes:attributes];
  buttonConfiguration.attributedTitle = attributedString;

  // Decide cell.button titleColor in order:
  //   1. self.buttonTextColor;
  //   2. [UIColor colorNamed:kSolidButtonTextColor]
  if (self.buttonTextColor) {
    buttonConfiguration.baseForegroundColor = self.buttonTextColor;
  } else {
    buttonConfiguration.baseForegroundColor =
        [UIColor colorNamed:kSolidButtonTextColor];
  }

  // Decide cell.button.backgroundColor in order:
  //   1. self.buttonBackgroundColor
  //   2. [UIColor colorNamed:kBlueColor]
  if (self.buttonBackgroundColor) {
    buttonConfiguration.background.backgroundColor = self.buttonBackgroundColor;
  } else {
    buttonConfiguration.background.backgroundColor =
        [UIColor colorNamed:kBlueColor];
  }

  if (!self.enabled && self.dimBackgroundWhenDisabled) {
    buttonConfiguration.background.backgroundColor =
        [buttonConfiguration.background.backgroundColor
            colorWithAlphaComponent:kDisabledButtonAlpha];
  }

  buttonConfiguration.showsActivityIndicator = self.showsActivityIndicator;
  if (self.showsActivityIndicator) {
    __weak __typeof(self) weakSelf = self;
    buttonConfiguration.activityIndicatorColorTransformer =
        ^UIColor*(UIColor* color) {
          return weakSelf.activityIndicatorColor
                     ? weakSelf.activityIndicatorColor
                     : [UIColor colorNamed:kSolidWhiteColor];
        };
  }

  if (self.showsCheckmark) {
    buttonConfiguration.image = DefaultSymbolWithPointSize(
        kCheckmarkCircleFillSymbol, kSymbolConfirmationCheckmarkPointSize);

    __weak __typeof(self) weakSelf = self;
    buttonConfiguration.imageColorTransformer = ^UIColor*(UIColor* color) {
      return weakSelf.checkmarkColor ? weakSelf.checkmarkColor
                                     : [UIColor colorNamed:kBlue700Color];
    };
  }

  if (self.buttonAccessibilityLabel) {
    cell.button.accessibilityLabel = self.buttonAccessibilityLabel;
  } else {
    cell.button.accessibilityLabel = nil;
  }

  cell.button.configuration = buttonConfiguration;

  [cell disableButtonIntrinsicWidth:self.disableButtonIntrinsicWidth];
  cell.button.accessibilityIdentifier = self.buttonAccessibilityIdentifier;
  cell.button.enabled = self.enabled;
}

@end

@interface TableViewTextButtonCell ()
// StackView that contains the cell's Button and Label.
@property(nonatomic, strong) UIStackView* verticalStackView;
// Constraints used to match the Button width to the StackView.
@property(nonatomic, strong) NSArray* expandedButtonWidthConstraints;
@end

@implementation TableViewTextButtonCell
@synthesize textLabel = _textLabel;
@synthesize button = _button;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    // Create informative text label.
    self.textLabel = [[UILabel alloc] init];
    self.textLabel.numberOfLines = 0;
    self.textLabel.lineBreakMode = NSLineBreakByWordWrapping;
    self.textLabel.textAlignment = NSTextAlignmentCenter;
    self.textLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    self.textLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];

    // Create button.
    self.button = [UIButton buttonWithType:UIButtonTypeSystem];
    self.button.translatesAutoresizingMaskIntoConstraints = NO;
    self.button.layer.cornerRadius = kButtonCornerRadius;
    self.button.clipsToBounds = YES;

    UIButtonConfiguration* buttonConfiguration =
        [UIButtonConfiguration plainButtonConfiguration];
    buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(
        kButtonTitleVerticalContentInset, kButtonTitleHorizontalContentInset,
        kButtonTitleVerticalContentInset, kButtonTitleHorizontalContentInset);
    buttonConfiguration.titleLineBreakMode = NSLineBreakByWordWrapping;
    buttonConfiguration.titleAlignment =
        UIButtonConfigurationTitleAlignmentCenter;
    self.button.configuration = buttonConfiguration;

    self.button.pointerInteractionEnabled = YES;
    // This button's background color is configured whenever the cell is
    // reused. The pointer style provider used here dynamically provides the
    // appropriate style based on the background color at runtime.
    self.button.pointerStyleProvider =
        CreateOpaqueOrTransparentButtonPointerStyleProvider();

    // Vertical stackView to hold label and button.
    self.verticalStackView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ self.textLabel, self.button ]];
    self.verticalStackView.alignment = UIStackViewAlignmentCenter;
    self.verticalStackView.axis = UILayoutConstraintAxisVertical;
    self.verticalStackView.translatesAutoresizingMaskIntoConstraints = NO;

    [self.contentView addSubview:self.verticalStackView];

    // Add constraints for stackView
    [NSLayoutConstraint activateConstraints:@[
      [self.verticalStackView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kStackViewHorizontalSpacing],
      [self.verticalStackView.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kStackViewHorizontalSpacing],
      [self.verticalStackView.topAnchor
          constraintEqualToAnchor:self.contentView.topAnchor
                         constant:kStackViewVerticalSpacing],
      [self.verticalStackView.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor
                         constant:-kStackViewVerticalSpacing]
    ]];

    self.expandedButtonWidthConstraints = @[
      [self.button.leadingAnchor
          constraintEqualToAnchor:self.verticalStackView.leadingAnchor],
      [self.button.trailingAnchor
          constraintEqualToAnchor:self.verticalStackView.trailingAnchor],
    ];
  }
  return self;
}

#pragma mark - Public Methods

- (void)enableItemSpacing:(BOOL)enable {
  self.verticalStackView.spacing = enable ? kStackViewSubViewSpacing : 0;
}

- (void)disableButtonIntrinsicWidth:(BOOL)disable {
  if (disable) {
    [NSLayoutConstraint
        activateConstraints:self.expandedButtonWidthConstraints];
  } else {
    [NSLayoutConstraint
        deactivateConstraints:self.expandedButtonWidthConstraints];
  }
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  UIButtonConfiguration* buttonConfiguration = self.button.configuration;
  buttonConfiguration.baseForegroundColor =
      [UIColor colorNamed:kSolidButtonTextColor];
  buttonConfiguration.titleAlignment =
      UIButtonConfigurationTitleAlignmentCenter;
  self.button.configuration = buttonConfiguration;
  [self disableButtonIntrinsicWidth:NO];
}

@end
