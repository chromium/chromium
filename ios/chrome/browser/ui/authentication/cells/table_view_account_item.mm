// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/cells/table_view_account_item.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/settings/cells/settings_cells_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Padding used between the text and error icon.
constexpr CGFloat kHorizontalPaddingBetweenTextAndError = 5;

// Size of the error icon image.
constexpr CGFloat KErrorIconImageSize = 22.;

}  // namespace

@implementation TableViewAccountItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewAccountCell class];
    self.accessibilityTraits |= UIAccessibilityTraitButton;
    _mode = TableViewAccountModeEnabled;
  }
  return self;
}

#pragma mark - TableViewItem

- (void)configureCell:(TableViewAccountCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];

  CHECK(self.image, base::NotFatalUntil::M123);
  cell.imageView.image = self.image;
  cell.textLabel.text = self.text;
  cell.detailTextLabel.text = self.detailText;
  if (self.shouldDisplayError) {
    cell.errorIcon.image =
        DefaultSymbolWithPointSize(kErrorCircleFillSymbol, KErrorIconImageSize);
    cell.errorIcon.tintColor = [UIColor colorNamed:kRed500Color];
  } else {
    cell.errorIcon.image = nil;
    cell.detailTextLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  }

  cell.userInteractionEnabled = self.mode == TableViewAccountModeEnabled;
  if (self.mode != TableViewAccountModeDisabled) {
    cell.contentView.alpha = 1;
    UIImageView* accessoryImage =
        base::apple::ObjCCastStrict<UIImageView>(cell.accessoryView);
    accessoryImage.tintColor =
        [accessoryImage.tintColor colorWithAlphaComponent:1];
  } else {
    cell.userInteractionEnabled = NO;
    cell.contentView.alpha = 0.5;
    UIImageView* accessoryImage =
        base::apple::ObjCCastStrict<UIImageView>(cell.accessoryView);
    accessoryImage.tintColor =
        [accessoryImage.tintColor colorWithAlphaComponent:0.5];
  }
}

@end

@implementation TableViewAccountCell

@synthesize imageView = _imageView;
@synthesize textLabel = _textLabel;
@synthesize detailTextLabel = _detailTextLabel;
@synthesize errorIcon = _errorIcon;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.isAccessibilityElement = YES;
    [self addSubviews];
    [self setViewConstraints];
  }
  return self;
}

// Create and add subviews.
- (void)addSubviews {
  UIView* contentView = self.contentView;
  contentView.clipsToBounds = YES;

  _imageView = [[UIImageView alloc] init];
  _imageView.translatesAutoresizingMaskIntoConstraints = NO;
  _imageView.layer.masksToBounds = YES;
  _imageView.contentMode = UIViewContentModeScaleAspectFit;
  // Creates the image rounded corners.
  _imageView.layer.cornerRadius = kTableViewIconImageSize / 2.0f;
  [contentView addSubview:_imageView];

  _errorIcon = [[UIImageView alloc] init];
  _errorIcon.translatesAutoresizingMaskIntoConstraints = NO;
  [_errorIcon setContentHuggingPriority:UILayoutPriorityRequired
                                forAxis:UILayoutConstraintAxisHorizontal];
  [_errorIcon
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [contentView addSubview:_errorIcon];

  _textLabel = [[UILabel alloc] init];
  _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  _textLabel.adjustsFontForContentSizeCategory = YES;
  _textLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  [contentView addSubview:_textLabel];

  _detailTextLabel = [[UILabel alloc] init];
  _detailTextLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _detailTextLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  _detailTextLabel.adjustsFontForContentSizeCategory = YES;
  _detailTextLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  [contentView addSubview:_detailTextLabel];
}

// Set constraints on subviews.
- (void)setViewConstraints {
  UIView* contentView = self.contentView;

  // This view is used to center the two leading textLabels.
  UIView* verticalCenteringView = [[UIView alloc] init];
  verticalCenteringView.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:verticalCenteringView];

  [NSLayoutConstraint activateConstraints:@[
    // Set leading anchors.
    [_imageView.leadingAnchor
        constraintEqualToAnchor:contentView.leadingAnchor
                       constant:kTableViewHorizontalSpacing],
    [_detailTextLabel.leadingAnchor
        constraintEqualToAnchor:_textLabel.leadingAnchor],

    // Fix image widths. The account images have been resized to fit this size.
    // Update the resize if this changes.
    [_imageView.widthAnchor constraintEqualToConstant:kTableViewIconImageSize],
    [_imageView.heightAnchor constraintEqualToAnchor:_imageView.widthAnchor],

    // Set vertical anchors.
    [_imageView.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],
    [_imageView.topAnchor
        constraintGreaterThanOrEqualToAnchor:contentView.topAnchor
                                    constant:kTableViewVerticalSpacing],
    [_imageView.bottomAnchor
        constraintLessThanOrEqualToAnchor:contentView.bottomAnchor
                                 constant:-kTableViewVerticalSpacing],
    [_textLabel.topAnchor
        constraintEqualToAnchor:verticalCenteringView.topAnchor],
    [_textLabel.bottomAnchor
        constraintEqualToAnchor:_detailTextLabel.topAnchor],
    [_detailTextLabel.bottomAnchor
        constraintEqualToAnchor:verticalCenteringView.bottomAnchor],
    [verticalCenteringView.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],
    [_errorIcon.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],
    [verticalCenteringView.topAnchor
        constraintGreaterThanOrEqualToAnchor:contentView.topAnchor
                                    constant:
                                        kTableViewTwoLabelsCellVerticalSpacing],
    [verticalCenteringView.bottomAnchor
        constraintLessThanOrEqualToAnchor:contentView.bottomAnchor
                                 constant:
                                     kTableViewTwoLabelsCellVerticalSpacing],

    // Set trailing anchors.
    [_errorIcon.trailingAnchor
        constraintEqualToAnchor:contentView.trailingAnchor
                       constant:-kTableViewHorizontalSpacing],
    [_detailTextLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:_errorIcon.leadingAnchor
                                 constant:
                                     -kHorizontalPaddingBetweenTextAndError],
    [_textLabel.leadingAnchor
        constraintEqualToAnchor:_imageView.trailingAnchor
                       constant:kTableViewOneLabelCellVerticalSpacing],
    [_textLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:_errorIcon.leadingAnchor
                                 constant:
                                     -kHorizontalPaddingBetweenTextAndError],
  ]];

  // This is needed so the image doesn't get pushed out if both text and detail
  // are long.
  [_textLabel
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [_detailTextLabel
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisHorizontal];
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  self.imageView.image = nil;
  self.textLabel.text = nil;
  self.detailTextLabel.text = nil;
  self.textLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  self.detailTextLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  self.errorIcon.image = nil;
  self.userInteractionEnabled = YES;
  self.contentView.alpha = 1;
  UIImageView* accessoryImage =
      base::apple::ObjCCastStrict<UIImageView>(self.accessoryView);
  accessoryImage.tintColor =
      [accessoryImage.tintColor colorWithAlphaComponent:1];
}

#pragma mark - NSObject(Accessibility)

- (NSString*)accessibilityLabel {
  return self.textLabel.text;
}

- (NSString*)accessibilityValue {
  if (self.errorIcon.image != nil) {
    return
        [NSString stringWithFormat:
                      @"%@, %@", self.detailTextLabel.text,
                      l10n_util::GetNSString(
                          IDS_IOS_ITEM_ACCOUNT_ERROR_BADGE_ACCESSIBILITY_HINT)];
  }
  return self.detailTextLabel.text;
}

- (NSArray<NSString*>*)accessibilityUserInputLabels {
  NSMutableArray<NSString*>* userInputLabels = [[NSMutableArray alloc] init];
  if (self.textLabel.text) {
    [userInputLabels addObject:self.textLabel.text];
  }

  return userInputLabels;
}

@end
