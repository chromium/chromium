// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/cells/cvc_item.h"

#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Padding used on the leading and trailing edges of the cell.
const CGFloat kHorizontalPadding = 16;
// Padding used on the top and bottom edges of the cell.
const CGFloat kVerticalPadding = 16;
// Spacing between elements.
const CGFloat kUISpacing = 5;
// Spacing around the CVC container.
const CGFloat kUICVCSpacing = 20;
// Height of the different text fields.
const CGFloat kTextFieldHeight = 50;
// Width of the date text fields.
const CGFloat kDateTextFieldWidth = 40;
}

@interface CVCCell ()<UITextFieldDelegate>
@property(nonatomic, strong) UILabel* dateSeparator;
@property(nonatomic, strong) UIView* dateContainerView;
@property(nonatomic, strong) UIView* CVCContainerView;
@property(nonatomic, strong)
    NSLayoutConstraint* CVCContainerLeadingConstraintWithDate;
@property(nonatomic, strong)
    NSLayoutConstraint* CVCContainerLeadingConstraintWithoutDate;
@end

@implementation CVCItem

@synthesize instructionsText = _instructionsText;
@synthesize errorMessage = _errorMessage;
@synthesize monthText = _monthText;
@synthesize yearText = _yearText;
@synthesize CVCText = _CVCText;
@synthesize showDateInput = _showDateInput;
@synthesize showNewCardButton = _showNewCardButton;
@synthesize showCVCInputError = _showCVCInputError;
@synthesize CVCImageResourceID = _CVCImageResourceID;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [CVCCell class];
  }
  return self;
}

#pragma mark CollectionViewItem

- (void)configureCell:(CVCCell*)cell {
  [super configureCell:cell];
  cell.instructionsTextLabel.text = self.instructionsText;
  cell.errorLabel.text = self.errorMessage;

  cell.monthInput.text = self.monthText;
  cell.yearInput.text = self.yearText;
  cell.CVCInput.text = self.CVCText;

  cell.dateContainerView.hidden = !self.showDateInput;

  cell.CVCContainerLeadingConstraintWithDate.active = self.showDateInput;
  cell.CVCContainerLeadingConstraintWithoutDate.active = !self.showDateInput;

  cell.buttonForNewCard.hidden = !self.showNewCardButton;

  cell.CVCImageView.image = NativeImage(self.CVCImageResourceID);
}

@end

@implementation CVCCell

@synthesize instructionsTextLabel = _instructionsTextLabel;
@synthesize errorLabel = _errorLabel;
@synthesize monthInput = _monthInput;
@synthesize yearInput = _yearInput;
@synthesize CVCInput = _CVCInput;
@synthesize buttonForNewCard = _buttonForNewCard;
@synthesize dateSeparator = _dateSeparator;
@synthesize CVCImageView = _CVCImageView;
@synthesize dateContainerView = _dateContainerView;
@synthesize CVCContainerView = _CVCContainerView;
@synthesize CVCContainerLeadingConstraintWithDate =
    _CVCContainerLeadingConstraintWithDate;
@synthesize CVCContainerLeadingConstraintWithoutDate =
    _CVCContainerLeadingConstraintWithoutDate;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    UIView* contentView = self.contentView;

    _instructionsTextLabel = [[UILabel alloc] init];
    _instructionsTextLabel.font =
        [[MDCTypography fontLoader] mediumFontOfSize:14];
    _instructionsTextLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _instructionsTextLabel.numberOfLines = 0;
    _instructionsTextLabel.lineBreakMode = NSLineBreakByWordWrapping;
    _instructionsTextLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [contentView addSubview:_instructionsTextLabel];

    _errorLabel = [[UILabel alloc] init];
    _errorLabel.font = [[MDCTypography fontLoader] regularFontOfSize:12];
    _errorLabel.textColor = [UIColor colorNamed:kRedColor];
    _errorLabel.numberOfLines = 0;
    _errorLabel.lineBreakMode = NSLineBreakByWordWrapping;
    _errorLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [contentView addSubview:_errorLabel];

    _dateContainerView = [[UIView alloc] init];
    _dateContainerView.translatesAutoresizingMaskIntoConstraints = NO;
    [contentView addSubview:_dateContainerView];

    _monthInput = ios::GetChromeBrowserProvider()->CreateStyledTextField();
    _monthInput.placeholder = l10n_util::GetNSString(
        IDS_IOS_AUTOFILL_DIALOG_PLACEHOLDER_EXPIRY_MONTH);
    _monthInput.accessibilityIdentifier = @"month_textField";
    _monthInput.keyboardType = UIKeyboardTypeNumberPad;
    _monthInput.delegate = self;
    _monthInput.translatesAutoresizingMaskIntoConstraints = NO;
    [_dateContainerView addSubview:_monthInput];

    _dateSeparator = [[UILabel alloc] init];
    _dateSeparator.text =
        l10n_util::GetNSString(IDS_AUTOFILL_EXPIRATION_DATE_SEPARATOR);
    _dateSeparator.translatesAutoresizingMaskIntoConstraints = NO;
    [_dateContainerView addSubview:_dateSeparator];

    _yearInput = ios::GetChromeBrowserProvider()->CreateStyledTextField();
    _yearInput.placeholder =
        l10n_util::GetNSString(IDS_IOS_AUTOFILL_DIALOG_PLACEHOLDER_EXPIRY_YEAR);
    _yearInput.accessibilityIdentifier = @"year_textField";
    _yearInput.keyboardType = UIKeyboardTypeNumberPad;
    _yearInput.delegate = self;
    _yearInput.translatesAutoresizingMaskIntoConstraints = NO;
    [_dateContainerView addSubview:_yearInput];

    _CVCContainerView = [[UIView alloc] init];
    _CVCContainerView.translatesAutoresizingMaskIntoConstraints = NO;
    [contentView addSubview:_CVCContainerView];

    _CVCInput = ios::GetChromeBrowserProvider()->CreateStyledTextField();
    _CVCInput.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _CVCInput.placeholder =
        l10n_util::GetNSString(IDS_AUTOFILL_DIALOG_PLACEHOLDER_CVC);
    _CVCInput.accessibilityIdentifier = @"CVC_textField";
    _CVCInput.keyboardType = UIKeyboardTypeNumberPad;
    _CVCInput.delegate = self;
    _CVCInput.translatesAutoresizingMaskIntoConstraints = NO;
    [_CVCContainerView addSubview:_CVCInput];

    _CVCImageView = [[UIImageView alloc] init];
    _CVCImageView.translatesAutoresizingMaskIntoConstraints = NO;
    [_CVCContainerView addSubview:_CVCImageView];

    _buttonForNewCard = [UIButton buttonWithType:UIButtonTypeCustom];
    _buttonForNewCard.titleLabel.font =
        [[MDCTypography fontLoader] regularFontOfSize:12];
    [_buttonForNewCard
        setTitle:l10n_util::GetNSString(IDS_AUTOFILL_CARD_UNMASK_NEW_CARD_LINK)
        forState:UIControlStateNormal];
    [_buttonForNewCard setTitleColor:[UIColor colorNamed:kBlueColor]
                            forState:UIControlStateNormal];
    _buttonForNewCard.translatesAutoresizingMaskIntoConstraints = NO;
    [contentView addSubview:_buttonForNewCard];

    [NSLayoutConstraint activateConstraints:@[
      // Text label
      [_instructionsTextLabel.topAnchor
          constraintEqualToAnchor:contentView.topAnchor
                         constant:kVerticalPadding],
      [_instructionsTextLabel.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kHorizontalPadding],
      [_instructionsTextLabel.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor
                         constant:-kHorizontalPadding],

      // Date container
      [_dateContainerView.topAnchor
          constraintEqualToAnchor:_instructionsTextLabel.bottomAnchor
                         constant:kUISpacing],
      [_dateContainerView.leadingAnchor
          constraintEqualToAnchor:_instructionsTextLabel.leadingAnchor],
      [_dateContainerView.heightAnchor
          constraintEqualToConstant:kTextFieldHeight],

      // Date content - Month input
      [_monthInput.topAnchor
          constraintEqualToAnchor:_dateContainerView.topAnchor],
      [_monthInput.leadingAnchor
          constraintEqualToAnchor:_dateContainerView.leadingAnchor],
      [_monthInput.widthAnchor constraintEqualToConstant:kDateTextFieldWidth],
      [_monthInput.bottomAnchor
          constraintEqualToAnchor:_dateContainerView.bottomAnchor],

      // Date content - Separator
      [_dateSeparator.leadingAnchor
          constraintEqualToAnchor:_monthInput.trailingAnchor
                         constant:kUISpacing],
      [_dateSeparator.firstBaselineAnchor
          constraintEqualToAnchor:_monthInput.firstBaselineAnchor],

      // Date content = Year input
      [_yearInput.leadingAnchor
          constraintEqualToAnchor:_dateSeparator.trailingAnchor
                         constant:kUISpacing],
      [_yearInput.widthAnchor constraintEqualToAnchor:_monthInput.widthAnchor],
      [_yearInput.heightAnchor
          constraintEqualToAnchor:_monthInput.heightAnchor],
      [_yearInput.firstBaselineAnchor
          constraintEqualToAnchor:_monthInput.firstBaselineAnchor],
      [_dateContainerView.trailingAnchor
          constraintEqualToAnchor:_yearInput.trailingAnchor],

      // CVC container
      [_CVCContainerView.topAnchor
          constraintEqualToAnchor:_dateContainerView.topAnchor],
      [_CVCContainerView.heightAnchor
          constraintEqualToAnchor:_dateContainerView.heightAnchor],
      // The horizontal placement of this container is dynamic. The two possible
      // placements are defined below with
      // _CVCContainerLeadingConstraintWithDate and
      // _CVCContainerLeadingConstraintWithoutDate.

      // CVC content - CVC input
      [_CVCInput.leadingAnchor
          constraintEqualToAnchor:_CVCContainerView.leadingAnchor],
      [_CVCInput.firstBaselineAnchor
          constraintEqualToAnchor:_monthInput.firstBaselineAnchor],
      [_CVCInput.bottomAnchor
          constraintEqualToAnchor:_CVCContainerView.bottomAnchor],

      // CVC content - CVC image view
      [_CVCImageView.leadingAnchor
          constraintEqualToAnchor:_CVCInput.trailingAnchor
                         constant:kUISpacing],
      [_CVCImageView.centerYAnchor
          constraintEqualToAnchor:_CVCInput.centerYAnchor],
      [_CVCContainerView.trailingAnchor
          constraintEqualToAnchor:_CVCImageView.trailingAnchor],

      // "New Card?" label
      [_buttonForNewCard.leadingAnchor
          constraintEqualToAnchor:_CVCContainerView.trailingAnchor
                         constant:kUICVCSpacing],
      [_buttonForNewCard.firstBaselineAnchor
          constraintEqualToAnchor:_CVCInput.firstBaselineAnchor],

      // Error label
      [_errorLabel.topAnchor
          constraintEqualToAnchor:_dateContainerView.bottomAnchor
                         constant:kUISpacing],
      [_errorLabel.leadingAnchor
          constraintEqualToAnchor:_instructionsTextLabel.leadingAnchor],
      [_errorLabel.trailingAnchor
          constraintEqualToAnchor:_instructionsTextLabel.trailingAnchor],

      [contentView.bottomAnchor constraintEqualToAnchor:_errorLabel.bottomAnchor
                                               constant:kUISpacing],
    ]];

    _CVCContainerLeadingConstraintWithDate = [_CVCContainerView.leadingAnchor
        constraintEqualToAnchor:_dateContainerView.trailingAnchor
                       constant:kUICVCSpacing];
    _CVCContainerLeadingConstraintWithoutDate = [_CVCContainerView.leadingAnchor
        constraintEqualToAnchor:_instructionsTextLabel.leadingAnchor];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.instructionsTextLabel.text = nil;
  self.errorLabel.text = nil;
  self.monthInput.text = nil;
  [self.monthInput removeTarget:nil
                         action:nil
               forControlEvents:UIControlEventAllEvents];
  self.yearInput.text = nil;
  [self.yearInput removeTarget:nil
                        action:nil
              forControlEvents:UIControlEventAllEvents];
  self.CVCInput.text = nil;
  [self.CVCInput removeTarget:nil
                       action:nil
             forControlEvents:UIControlEventAllEvents];
  [self.buttonForNewCard removeTarget:nil
                               action:nil
                     forControlEvents:UIControlEventAllEvents];
  self.dateContainerView.hidden = YES;
  self.CVCContainerLeadingConstraintWithDate.active = NO;
  self.CVCContainerLeadingConstraintWithoutDate.active = YES;
  self.buttonForNewCard.hidden = YES;
  self.CVCImageView.image = nil;
}

// Implements -layoutSubviews as per instructions in documentation for
// +[MDCCollectionViewCell cr_preferredHeightForWidth:forItem:].
- (void)layoutSubviews {
  [super layoutSubviews];

  // Adjust the text labels preferredMaxLayoutWidth when the parent's width
  // changes, for instance on screen rotation.
  CGFloat parentWidth = CGRectGetWidth(self.contentView.bounds);
  CGFloat labelsPreferredMaxLayoutWidth = parentWidth - 2 * kHorizontalPadding;
  self.instructionsTextLabel.preferredMaxLayoutWidth =
      labelsPreferredMaxLayoutWidth;
  self.errorLabel.preferredMaxLayoutWidth = labelsPreferredMaxLayoutWidth;

  // Re-layout with the new preferred width to allow the labels to adjust their
  // height.
  [super layoutSubviews];
}

#pragma mark - UITextFieldDelegate

// Limit month input to 2 characters. Year input is limited to 4 characters
// (both 2-digit and 4-digit years are accepted). CVC input is limited to 4
// characters since CVCs are never longer than that.
- (BOOL)textField:(UITextField*)textField
    shouldChangeCharactersInRange:(NSRange)range
                replacementString:(NSString*)string {
  if (textField == self.CVCInput || textField == self.yearInput) {
    return ([textField.text length] + [string length] - range.length) <= 4;
  } else if (textField == self.monthInput) {
    return ([textField.text length] + [string length] - range.length) <= 2;
  }
  return YES;
}

@end
