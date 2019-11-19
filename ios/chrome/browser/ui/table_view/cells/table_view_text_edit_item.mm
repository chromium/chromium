// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/cells/table_view_text_edit_item.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_edit_item_delegate.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Minimum gap between the label and the text field.
const CGFloat kLabelAndFieldGap = 5;
// Height/width of the edit icon.
const CGFloat kEditIconLength = 18;

}  // namespace

@interface TableViewTextEditItem ()

// Whether the field has valid text.
@property(nonatomic, assign) BOOL hasValidText;

@end

@implementation TableViewTextEditItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewTextEditCell class];
    _returnKeyType = UIReturnKeyNext;
    _keyboardType = UIKeyboardTypeDefault;
    _autoCapitalizationType = UITextAutocapitalizationTypeWords;
    _hasValidText = YES;
  }
  return self;
}

#pragma mark TableViewItem

- (void)configureCell:(TableViewTextEditCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];

  NSString* textLabelFormat = self.required ? @"%@*" : @"%@";
  cell.textLabel.text =
      [NSString stringWithFormat:textLabelFormat, self.textFieldName];
  cell.textField.placeholder = self.textFieldPlaceholder;
  cell.textField.text = self.textFieldValue;
  if (self.textFieldName.length) {
    cell.textField.accessibilityIdentifier =
        [NSString stringWithFormat:@"%@_textField", self.textFieldName];
  }
  if (styler.cellBackgroundColor) {
    cell.textLabel.backgroundColor = styler.cellBackgroundColor;
    cell.textField.backgroundColor = styler.cellBackgroundColor;
  } else {
    cell.textLabel.backgroundColor = styler.tableViewBackgroundColor;
    cell.textField.backgroundColor = styler.tableViewBackgroundColor;
  }

  cell.textField.enabled = self.textFieldEnabled;

  if (self.hideIcon) {
    cell.textField.textColor = self.textFieldEnabled
                                   ? [UIColor colorNamed:kBlueColor]
                                   : UIColor.cr_secondaryLabelColor;
    [cell setIcon:TableViewTextEditItemIconTypeNone];
  } else {
    if (self.hasValidText) {
      cell.textField.textColor = UIColor.cr_secondaryLabelColor;
    }
    if (cell.textField.editing && cell.textField.text.length > 0) {
      cell.iconView.accessibilityIdentifier =
          [NSString stringWithFormat:@"%@_noIcon", self.textFieldName];
      [cell setIcon:TableViewTextEditItemIconTypeNone];
    } else if (!self.hasValidText) {
      cell.iconView.accessibilityIdentifier =
          [NSString stringWithFormat:@"%@_errorIcon", self.textFieldName];
      cell.textField.textColor = [UIColor colorNamed:kRedColor];
      [cell setIcon:TableViewTextEditItemIconTypeError];
    } else {
      cell.iconView.accessibilityIdentifier =
          [NSString stringWithFormat:@"%@_editIcon", self.textFieldName];
      [cell setIcon:TableViewTextEditItemIconTypeEdit];
    }
  }

  [cell.textField addTarget:self
                     action:@selector(textFieldChanged:)
           forControlEvents:UIControlEventEditingChanged];
  [cell.textField addTarget:self
                     action:@selector(textFieldBeginEditing:)
           forControlEvents:UIControlEventEditingDidBegin];
  [cell.textField addTarget:self
                     action:@selector(textFieldEndEditing:)
           forControlEvents:UIControlEventEditingDidEnd];
  cell.textField.returnKeyType = self.returnKeyType;
  cell.textField.keyboardType = self.keyboardType;
  cell.textField.autocapitalizationType = self.autoCapitalizationType;

  [cell setIdentifyingIcon:self.identifyingIcon];
  cell.identifyingIconButton.enabled = self.identifyingIconEnabled;
  if ([self.identifyingIconAccessibilityLabel length]) {
    cell.identifyingIconButton.accessibilityLabel =
        self.identifyingIconAccessibilityLabel;
  }

  // If the TextField or IconButton are enabled, the cell needs to make its
  // inner TextField or button accessible to voice over. In order to achieve
  // this the cell can't be an A11y element.
  cell.isAccessibilityElement =
      !(self.textFieldEnabled || self.identifyingIconEnabled);
}

#pragma mark Actions

- (void)textFieldChanged:(UITextField*)textField {
  self.textFieldValue = textField.text;
  [self.delegate tableViewItemDidChange:self];
}

- (void)textFieldBeginEditing:(UITextField*)textField {
  [self.delegate tableViewItemDidBeginEditing:self];
}

- (void)textFieldEndEditing:(UITextField*)textField {
  [self.delegate tableViewItemDidEndEditing:self];
}

#pragma mark - Public

- (void)setHasValidText:(BOOL)hasValidText {
  if (_hasValidText == hasValidText)
    return;
  _hasValidText = hasValidText;
}

@end

#pragma mark - TableViewTextEditCell

@interface TableViewTextEditCell ()

@property(nonatomic, strong) NSLayoutConstraint* iconHeightConstraint;
@property(nonatomic, strong) NSLayoutConstraint* iconWidthConstraint;
@property(nonatomic, strong) NSLayoutConstraint* textFieldTrailingConstraint;
@property(nonatomic, strong) NSLayoutConstraint* textLabelTrailingConstraint;
@property(nonatomic, strong) NSLayoutConstraint* editIconHeightConstraint;
@property(nonatomic, strong) NSLayoutConstraint* iconTrailingConstraint;

// When they are activated, the label and the text field are on one line.
// They conflict with the |accessibilityConstraints|.
@property(nonatomic, strong) NSArray<NSLayoutConstraint*>* standardConstraints;
// When they are activated, the label is on one line, the text field is on
// another line. They conflict with the |standardConstraints|.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* accessibilityConstraints;

@end

@implementation TableViewTextEditCell

@synthesize textLabel = _textLabel;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.isAccessibilityElement = YES;
    UIView* contentView = self.contentView;

    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [_textLabel setContentHuggingPriority:UILayoutPriorityDefaultHigh
                                  forAxis:UILayoutConstraintAxisHorizontal];
    _textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _textLabel.adjustsFontForContentSizeCategory = YES;
    [contentView addSubview:_textLabel];

    _textField = [[UITextField alloc] init];
    _textField.translatesAutoresizingMaskIntoConstraints = NO;
    _textField.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _textField.adjustsFontForContentSizeCategory = YES;
    [_textField
        setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    [contentView addSubview:_textField];

    _textField.autocorrectionType = UITextAutocorrectionTypeNo;
    _textField.clearButtonMode = UITextFieldViewModeWhileEditing;
    _textField.contentVerticalAlignment =
        UIControlContentVerticalAlignmentCenter;

    // Trailing icon button.
    _identifyingIconButton =
        [ExtendedTouchTargetButton buttonWithType:UIButtonTypeCustom];
    _identifyingIconButton.translatesAutoresizingMaskIntoConstraints = NO;
    [contentView addSubview:_identifyingIconButton];

    // Edit icon.
    _iconView = [[UIImageView alloc] initWithImage:[self editImage]];
    _iconView.tintColor = [UIColor colorNamed:kGrey400Color];
    _iconView.translatesAutoresizingMaskIntoConstraints = NO;
    [contentView addSubview:_iconView];

    // Set up the icons size constraints. They are activated here and updated in
    // layoutSubviews.
    _iconHeightConstraint =
        [_identifyingIconButton.heightAnchor constraintEqualToConstant:0];
    _iconWidthConstraint =
        [_identifyingIconButton.widthAnchor constraintEqualToConstant:0];
    _editIconHeightConstraint =
        [_iconView.heightAnchor constraintEqualToConstant:0];

    _textFieldTrailingConstraint = [_textField.trailingAnchor
        constraintEqualToAnchor:_iconView.leadingAnchor];
    _textLabelTrailingConstraint = [_textLabel.trailingAnchor
        constraintEqualToAnchor:_iconView.leadingAnchor];
    _iconTrailingConstraint = [_iconView.trailingAnchor
        constraintEqualToAnchor:_identifyingIconButton.leadingAnchor];

    _standardConstraints = @[
      [_textField.firstBaselineAnchor
          constraintEqualToAnchor:_textLabel.firstBaselineAnchor],
      [_textField.leadingAnchor
          constraintEqualToAnchor:_textLabel.trailingAnchor
                         constant:kLabelAndFieldGap],
    ];

    _accessibilityConstraints = @[
      [_textField.topAnchor constraintEqualToAnchor:_textLabel.bottomAnchor
                                           constant:kTableViewVerticalSpacing],
      [_textField.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      _textLabelTrailingConstraint,
    ];

    // Set up the constraints.
    [NSLayoutConstraint activateConstraints:@[
      [_textLabel.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      _textFieldTrailingConstraint,
      [_identifyingIconButton.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
      [_identifyingIconButton.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],
      [_iconView.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],
      _iconHeightConstraint,
      _iconWidthConstraint,
      _iconTrailingConstraint,
      _editIconHeightConstraint,
      [_iconView.widthAnchor constraintEqualToAnchor:_iconView.heightAnchor],
    ]];
    AddOptionalVerticalPadding(contentView, _textLabel,
                               kTableViewOneLabelCellVerticalSpacing);
    AddOptionalVerticalPadding(contentView, _textField,
                               kTableViewOneLabelCellVerticalSpacing);

    [self updateForAccessibilityContentSizeCategory:
              UIContentSizeCategoryIsAccessibilityCategory(
                  self.traitCollection.preferredContentSizeCategory)];
  }
  return self;
}

#pragma mark Public

- (void)setIcon:(TableViewTextEditItemIconType)iconType {
  switch (iconType) {
    case TableViewTextEditItemIconTypeNone:
      self.iconView.hidden = YES;
      [self.iconView setImage:nil];
      self.textFieldTrailingConstraint.constant = 0;
      self.textLabelTrailingConstraint.constant = 0;

      _editIconHeightConstraint.constant = 0;
      break;
    case TableViewTextEditItemIconTypeEdit:
      self.iconView.hidden = NO;
      [self.iconView setImage:[self editImage]];
      self.iconView.tintColor = [UIColor colorNamed:kGrey400Color];
      [self setIconTypeNoneConstraints];
      break;
    case TableViewTextEditItemIconTypeError:
      self.iconView.hidden = NO;
      [self.iconView setImage:[self errorImage]];
      self.iconView.tintColor = [UIColor colorNamed:kRedColor];
      [self setIconTypeNoneConstraints];
      break;
    default:
      NOTREACHED();
      break;
  }
}

- (void)setIdentifyingIcon:(UIImage*)icon {
  // Set Image as UIImageRenderingModeAlwaysTemplate to allow the Button tint
  // color to propagate.
  [self.identifyingIconButton
      setImage:[icon imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate]
      forState:UIControlStateNormal];
  // Set the same image for the button's disable state so it's not grayed out
  // when disabled.
  [self.identifyingIconButton setImage:icon forState:UIControlStateDisabled];
  if (icon) {
    self.iconTrailingConstraint.constant = -kLabelAndFieldGap;

    // Set the size constraints of the icon view to the dimensions of the image.
    self.iconHeightConstraint.constant = icon.size.height;
    self.iconWidthConstraint.constant = icon.size.width;
  } else {
    self.iconTrailingConstraint.constant = 0;
    self.iconHeightConstraint.constant = 0;
    self.iconWidthConstraint.constant = 0;
  }
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  BOOL isCurrentCategoryAccessibility =
      UIContentSizeCategoryIsAccessibilityCategory(
          self.traitCollection.preferredContentSizeCategory);
  if (isCurrentCategoryAccessibility !=
      UIContentSizeCategoryIsAccessibilityCategory(
          previousTraitCollection.preferredContentSizeCategory)) {
    [self updateForAccessibilityContentSizeCategory:
              isCurrentCategoryAccessibility];
  }
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  self.textLabel.text = nil;
  self.textField.text = nil;
  self.textField.returnKeyType = UIReturnKeyNext;
  self.textField.keyboardType = UIKeyboardTypeDefault;
  self.textField.autocapitalizationType = UITextAutocapitalizationTypeWords;
  self.textField.autocorrectionType = UITextAutocorrectionTypeNo;
  self.textField.clearButtonMode = UITextFieldViewModeWhileEditing;
  self.isAccessibilityElement = YES;
  self.textField.accessibilityIdentifier = nil;
  self.textField.enabled = NO;
  self.textField.delegate = nil;
  [self.textField removeTarget:nil
                        action:nil
              forControlEvents:UIControlEventAllEvents];
  [self setIdentifyingIcon:nil];
  self.identifyingIconButton.enabled = NO;
}

#pragma mark Accessibility

- (NSString*)accessibilityLabel {
  return [NSString
      stringWithFormat:@"%@, %@", self.textLabel.text, self.textField.text];
}

#pragma mark Private

// Updates the cell such as it is layouted correctly with regard to the
// preferred content size category, if it is an
// |accessibilityContentSizeCategory| or not.
- (void)updateForAccessibilityContentSizeCategory:
    (BOOL)accessibilityContentSizeCategory {
  if (accessibilityContentSizeCategory) {
    [NSLayoutConstraint deactivateConstraints:_standardConstraints];
    [NSLayoutConstraint activateConstraints:_accessibilityConstraints];
    _textField.textAlignment =
        UseRTLLayout() ? NSTextAlignmentRight : NSTextAlignmentLeft;
  } else {
    [NSLayoutConstraint deactivateConstraints:_accessibilityConstraints];
    [NSLayoutConstraint activateConstraints:_standardConstraints];
    _textField.textAlignment =
        UseRTLLayout() ? NSTextAlignmentLeft : NSTextAlignmentRight;
  }
}

// Sets constraints when the icon needs to be hidden.
- (void)setIconTypeNoneConstraints {
  self.textFieldTrailingConstraint.constant = -kLabelAndFieldGap;
  self.textLabelTrailingConstraint.constant = -kLabelAndFieldGap;

  _editIconHeightConstraint.constant = kEditIconLength;
}

// Returns the edit icon image.
- (UIImage*)editImage {
  return [[UIImage imageNamed:@"table_view_cell_edit_icon"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
}

// Returns the error icon image.
- (UIImage*)errorImage {
  return [[UIImage imageNamed:@"table_view_cell_error_icon"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
}

@end
