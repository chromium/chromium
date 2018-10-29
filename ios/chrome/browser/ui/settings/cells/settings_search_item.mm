// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/settings_search_item.h"

#include "base/mac/foundation_util.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#include "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/common/ui_util/constraints_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Margin for the search view.
const CGFloat kHorizontalMargin = 16.0f;
const CGFloat kVerticalMargin = 2.0f;
// Horizontal total padding for search icon.
const CGFloat kHorizontalIconPadding = 20.0f;
// Icon tint white transparency.
const CGFloat kIconTintAlpha = 0.3f;
// Background white transparency.
const CGFloat kBackgroundAlpha = 0.1f;
// Input text corner radius.
const CGFloat kCornerRadius = 12.0f;
// Input field disabled alpha.
const CGFloat kDisabledAlpha = 0.6f;
// Cancel button animation duration.
const CGFloat kCancelButtonAnimationDuration = 0.2f;
}  // namespace

@interface SettingsSearchCell ()<UITextFieldDelegate>
// Cancel button for dismissing the search view.
@property(nonatomic, strong) UIButton* cancelButton;
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* cancelUnfocusedConstraint;
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* cancelFocusedConstraint;
@end

@implementation SettingsSearchItem

@synthesize delegate = _delegate;
@synthesize placeholder = _placeholder;
@synthesize enabled = _enabled;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [SettingsSearchCell class];
    self.accessibilityTraits |= UIAccessibilityTraitSearchField;
    self.enabled = YES;
  }
  return self;
}

#pragma mark - CollectionViewItem

- (void)configureCell:(MDCCollectionViewCell*)cell {
  SettingsSearchCell* searchCell =
      base::mac::ObjCCastStrict<SettingsSearchCell>(cell);
  [super configureCell:searchCell];
  searchCell.textField.placeholder = self.placeholder;
  searchCell.textField.enabled = self.isEnabled;
  searchCell.textField.alpha = self.isEnabled ? 1.0f : kDisabledAlpha;
  searchCell.delegate = self.delegate;
}

@end

@implementation SettingsSearchCell

@synthesize delegate = _delegate;
@synthesize textField = _textField;
@synthesize cancelButton = _cancelButton;
@synthesize cancelUnfocusedConstraint = _cancelUnfocusedConstraint;
@synthesize cancelFocusedConstraint = _cancelFocusedConstraint;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    UIView* contentView = self.contentView;
    contentView.clipsToBounds = YES;
    contentView.backgroundColor = [UIColor clearColor];

    UIImage* searchIcon = [[ChromeIcon searchIcon]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    UIImageView* searchIconView =
        [[UIImageView alloc] initWithImage:searchIcon];
    CGFloat iconFrameWidth = kHorizontalIconPadding + searchIcon.size.width;
    searchIconView.tintColor = [UIColor colorWithWhite:0 alpha:kIconTintAlpha];
    searchIconView.frame =
        CGRectMake(0.0, 0.0, iconFrameWidth, searchIcon.size.height);
    searchIconView.contentMode = UIViewContentModeCenter;

    _textField = [[UITextField alloc] init];
    _textField.accessibilityIdentifier = @"SettingsSearchCellTextField";
    _textField.contentVerticalAlignment =
        UIControlContentVerticalAlignmentCenter;
    _textField.backgroundColor =
        [UIColor colorWithWhite:0 alpha:kBackgroundAlpha];
    _textField.textColor =
        [UIColor colorWithWhite:0 alpha:[MDCTypography body1FontOpacity]];
    _textField.font = [MDCTypography subheadFont];
    [[_textField layer] setCornerRadius:kCornerRadius];
    [_textField setLeftViewMode:UITextFieldViewModeAlways];
    _textField.leftView = searchIconView;
    _textField.clearButtonMode = UITextFieldViewModeAlways;
    [_textField setRightViewMode:UITextFieldViewModeNever];
    _textField.autocorrectionType = UITextAutocorrectionTypeNo;
    _textField.autocapitalizationType = UITextAutocapitalizationTypeNone;
    _textField.spellCheckingType = UITextSpellCheckingTypeNo;
    _textField.returnKeyType = UIReturnKeySearch;
    _textField.translatesAutoresizingMaskIntoConstraints = NO;
    [_textField setDelegate:self];

    NSString* cancelButtonLabel = l10n_util::GetNSString(IDS_CANCEL);
    _cancelButton = [UIButton buttonWithType:UIButtonTypeSystem];
    _cancelButton.contentHorizontalAlignment =
        UIControlContentHorizontalAlignmentRight;
    [_cancelButton setTitle:cancelButtonLabel forState:UIControlStateNormal];
    [_cancelButton addTarget:self
                      action:@selector(didTapCancelButton)
            forControlEvents:UIControlEventTouchUpInside];
    _cancelButton.alpha = 0;
    _cancelButton.translatesAutoresizingMaskIntoConstraints = NO;
    _cancelButton.accessibilityElementsHidden = YES;

    [contentView addSubview:_textField];
    [contentView addSubview:_cancelButton];

    // Set text field fixed constraints for top, bottom and left side.
    AddSameConstraintsToSidesWithInsets(
        _textField, contentView,
        LayoutSides::kLeading | LayoutSides::kBottom | LayoutSides::kTop,
        ChromeDirectionalEdgeInsetsMake(kVerticalMargin, kHorizontalMargin,
                                        kVerticalMargin, 0));

    // Set cancel button fixed constraints for top and bottom.
    AddSameConstraintsToSidesWithInsets(
        _cancelButton, contentView, LayoutSides::kBottom | LayoutSides::kTop,
        ChromeDirectionalEdgeInsetsMake(kVerticalMargin, 0, kVerticalMargin,
                                        0));

    // And these are constraints that we will use for animating the slide in/out
    // of the cancel button. We have constraints for unfocused mode,
    // where text field extends to the right (minus margin) of the content view
    // and cancel button is 'hidden' by being left aligned at the right of the
    // content view (where we want it to start it's slide in).
    _cancelUnfocusedConstraint = @[
      [_textField.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor
                         constant:-kHorizontalMargin],
      [_cancelButton.leadingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor],
    ];
    // And when focused, we'll attach the text field's right side to the cancel
    // button's left (minus margin) and attach the cancel button's right side to
    // the content view's right (minus margin).
    _cancelFocusedConstraint = @[
      [_textField.trailingAnchor
          constraintEqualToAnchor:_cancelButton.leadingAnchor
                         constant:-kHorizontalMargin],
      [_cancelButton.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor
                         constant:-kHorizontalMargin],
    ];

    // We start unfocused.
    [NSLayoutConstraint activateConstraints:_cancelUnfocusedConstraint];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];

  self.textField.placeholder = @"";
  self.textField.enabled = YES;
  self.textField.alpha = 1.0f;
  self.delegate = nil;
}

#pragma mark - UITextFieldDelegate

// Slides cancel button in when textfield is focused.
- (void)textFieldDidBeginEditing:(UITextField*)textField {
  void (^animations)() = ^{
    self.cancelButton.accessibilityElementsHidden = NO;
    self.cancelButton.alpha = 1.0f;
    [NSLayoutConstraint deactivateConstraints:self.cancelUnfocusedConstraint];
    [NSLayoutConstraint activateConstraints:self.cancelFocusedConstraint];
    [self.contentView layoutIfNeeded];
  };

  [UIView animateWithDuration:kCancelButtonAnimationDuration
                   animations:animations];
}

// Slides cancel button out when textfield is not focused.
- (void)textFieldDidEndEditing:(UITextField*)textField {
  void (^animations)() = ^{
    self.cancelButton.alpha = 0.0f;
    self.cancelButton.accessibilityElementsHidden = YES;
    [NSLayoutConstraint deactivateConstraints:self.cancelFocusedConstraint];
    [NSLayoutConstraint activateConstraints:self.cancelUnfocusedConstraint];
    [self.contentView layoutIfNeeded];
  };

  [UIView animateWithDuration:kCancelButtonAnimationDuration
                   animations:animations];
}

- (BOOL)textField:(UITextField*)textField
    shouldChangeCharactersInRange:(NSRange)range
                replacementString:(NSString*)string {
  NSMutableString* text = [NSMutableString stringWithString:[textField text]];
  [text replaceCharactersInRange:range withString:string];
  [self.delegate didRequestSearchForTerm:text];
  return YES;
}

- (BOOL)textFieldShouldClear:(UITextField*)textField {
  [self.delegate didRequestSearchForTerm:@""];
  return YES;
}

- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  [self.textField resignFirstResponder];
  return YES;
}

#pragma mark - Actions

- (void)didTapCancelButton {
  self.textField.text = @"";
  [self.delegate didRequestSearchForTerm:@""];
  [self.textField endEditing:YES];
}

@end
