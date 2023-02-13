// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/cells/table_view_text_button_item.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
// Default Text alignment.
const NSTextAlignment kDefaultTextAlignment = NSTextAlignmentCenter;
// Default Text alignment.
const UIControlContentHorizontalAlignment kDefaultContentHorizontalAlignment =
    UIControlContentHorizontalAlignmentCenter;
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
    _buttonContentHorizontalAlignment = kDefaultContentHorizontalAlignment;
    _boldButtonText = YES;
    _dimBackgroundWhenDisabled = YES;
  }
  return self;
}

- (void)configureCell:(TableViewCell*)tableCell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:tableCell withStyler:styler];
  TableViewTextButtonCell* cell =
      base::mac::ObjCCastStrict<TableViewTextButtonCell>(tableCell);
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

  [cell.button setTitle:self.buttonText forState:UIControlStateNormal];
  [cell disableButtonIntrinsicWidth:self.disableButtonIntrinsicWidth];
  // Decide cell.button titleColor in order:
  //   1. self.buttonTextColor;
  //   2. [UIColor colorNamed:kSolidButtonTextColor]
  if (self.buttonTextColor) {
    [cell.button setTitleColor:self.buttonTextColor
                      forState:UIControlStateNormal];
  } else {
    [cell.button setTitleColor:[UIColor colorNamed:kSolidButtonTextColor]
                      forState:UIControlStateNormal];
  }
  cell.button.contentHorizontalAlignment =
      self.buttonContentHorizontalAlignment;
  if (self.buttonContentHorizontalAlignment ==
      UIControlContentHorizontalAlignmentLeft) {
    cell.button.contentEdgeInsets = UIEdgeInsetsMake(
        kButtonTitleVerticalContentInset, 0, kButtonTitleVerticalContentInset,
        kButtonTitleHorizontalContentInset);
  }
  cell.button.accessibilityIdentifier = self.buttonAccessibilityIdentifier;
  // Decide cell.button.backgroundColor in order:
  //   1. self.buttonBackgroundColor
  //   2. [UIColor colorNamed:kBlueColor]
  if (self.buttonBackgroundColor) {
    cell.button.backgroundColor = self.buttonBackgroundColor;
  } else {
    cell.button.backgroundColor = [UIColor colorNamed:kBlueColor];
  }
  cell.button.enabled = self.enabled;
  if (!self.enabled && self.dimBackgroundWhenDisabled) {
    cell.button.backgroundColor = [cell.button.backgroundColor
        colorWithAlphaComponent:kDisabledButtonAlpha];
  }
  if (!self.boldButtonText) {
    [cell.button.titleLabel
        setFont:[UIFont preferredFontForTextStyle:UIFontTextStyleBody]];
    cell.button.titleLabel.adjustsFontForContentSizeCategory = YES;
  }
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
        [UIFont preferredFontForTextStyle:kTableViewSublabelFontStyle];
    self.textLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];

    // Create button.
    self.button = [UIButton buttonWithType:UIButtonTypeSystem];
    self.button.translatesAutoresizingMaskIntoConstraints = NO;
    [self.button.titleLabel
        setFont:[UIFont preferredFontForTextStyle:UIFontTextStyleBody]];
    self.button.titleLabel.adjustsFontForContentSizeCategory = YES;
    self.button.titleLabel.numberOfLines = 0;
    self.button.titleLabel.lineBreakMode = NSLineBreakByWordWrapping;
    self.button.titleLabel.textAlignment = NSTextAlignmentCenter;
    self.button.layer.cornerRadius = kButtonCornerRadius;
    self.button.clipsToBounds = YES;
    self.button.contentEdgeInsets = UIEdgeInsetsMake(
        kButtonTitleVerticalContentInset, kButtonTitleHorizontalContentInset,
        kButtonTitleVerticalContentInset, kButtonTitleHorizontalContentInset);

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
  [self.button setTitleColor:[UIColor colorNamed:kSolidButtonTextColor]
                    forState:UIControlStateNormal];
  self.button.contentHorizontalAlignment = kDefaultContentHorizontalAlignment;
  self.textLabel.textAlignment = kDefaultTextAlignment;
  [self disableButtonIntrinsicWidth:NO];
}

@end
