// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/cells/table_view_account_item.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/settings/ui_bundled/cells/settings_cells_constants.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Padding used between the text and status icon.
constexpr CGFloat kHorizontalPaddingBetweenTextAndStatus = 5;

// Size of the error icon image.
constexpr CGFloat kErrorIconImageSize = 22.;

// Point size of enterprise icon in the bottom view.
constexpr CGFloat kEnterpriseIconPointSize = 20;
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

  CHECK(self.image);
  cell.imageView.image = self.image;
  cell.textLabel.text = self.text;
  cell.detailTextLabel.text = self.detailText;
  if (self.shouldDisplayError) {
    [cell setStatusViewWithImage:DefaultSymbolWithPointSize(
                                     kErrorCircleFillSymbol,
                                     kErrorIconImageSize)];
  } else {
    [cell setStatusView:nil];
    cell.detailTextLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  }
  [cell showManagementIcon:self.managed];

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

  // When not set, the screen readers will read this cell as "text, detailText".
  // Add a custom accessibility label for managed accounts to append "managed by
  // your organization" so that the screen readers read this cell as "text,
  // detailText, managed by your organization".
  if (AreSeparateProfilesForManagedAccountsEnabled() && self.managed) {
    cell.accessibilityLabel =
        self.text && self.detailText
            ? l10n_util::GetNSStringF(
                  IDS_IOS_SIGNIN_ACCOUNT_PICKER_CHOOSE_ACCOUNT_ITEM_DESCRIPTION_WITH_NAME_AND_EMAIL_MANAGED,
                  base::SysNSStringToUTF16(self.text),
                  base::SysNSStringToUTF16(self.detailText))
            : l10n_util::GetNSStringF(
                  IDS_IOS_SIGNIN_ACCOUNT_PICKER_CHOOSE_ACCOUNT_ITEM_DESCRIPTION_WITH_EMAIL_MANAGED,
                  base::SysNSStringToUTF16(self.text ? self.text
                                                     : self.detailText));
  }
}

@end

@implementation TableViewAccountCell {
  // Status icon that will be displayed on the trailing side of the cell.
  // It is similar to the accessoryView. But accessoryView’s move to
  // top-trailing part of the cell on iOS 18. According to some online comment,
  // it seems `accessoryView` generates conflicting constraints when some
  // content of the cell uses `translatesAutoresizingMaskIntoConstraints = NO`.
  UIView* _statusView;
  // Container for the status view.
  UIView* _statusContainerView;

  UIImageView* _managementIconView;

  NSLayoutConstraint* _managementIconTrailingNoStatusMarginConstraint;
  NSLayoutConstraint* _managementIconTrailingWithStatusMarginConstraint;
}

@synthesize imageView = _imageView;
@synthesize textLabel = _textLabel;
@synthesize detailTextLabel = _detailTextLabel;

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

  _managementIconView = [[UIImageView alloc] init];
  _managementIconView.image = SymbolWithPalette(
      CustomSymbolWithPointSize(kEnterpriseSymbol, kEnterpriseIconPointSize),
      @[ [UIColor colorNamed:kStaticGrey600Color] ]);
  _managementIconView.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:_managementIconView];

  _statusContainerView = [[UIView alloc] init];
  _statusContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  [_statusContainerView
      setContentHuggingPriority:UILayoutPriorityRequired
                        forAxis:UILayoutConstraintAxisHorizontal];
  [_statusContainerView
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [contentView addSubview:_statusContainerView];

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
  [self setStatusView:nil];
}

// Set constraints on subviews.
- (void)setViewConstraints {
  UIView* contentView = self.contentView;

  // This view is used to center the two leading textLabels.
  UIView* verticalCenteringView = [[UIView alloc] init];
  verticalCenteringView.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:verticalCenteringView];

  _managementIconTrailingNoStatusMarginConstraint =
      [_managementIconView.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing];
  _managementIconTrailingWithStatusMarginConstraint =
      [_managementIconView.trailingAnchor
          constraintEqualToAnchor:_statusContainerView.leadingAnchor
                         constant:-kTableViewVerticalSpacing];
  [NSLayoutConstraint activateConstraints:@[
    // Set leading anchors.
    [_imageView.leadingAnchor
        constraintEqualToAnchor:contentView.leadingAnchor
                       constant:kTableViewHorizontalSpacing],
    [_detailTextLabel.leadingAnchor
        constraintEqualToAnchor:_textLabel.leadingAnchor],
    // Leading and width on the vertical centering view does not matter. It’s
    // here to remove ambiguity.
    [verticalCenteringView.widthAnchor constraintEqualToConstant:0],
    [verticalCenteringView.leadingAnchor
        constraintEqualToAnchor:_imageView.trailingAnchor],

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
    [_statusContainerView.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],
    [_managementIconView.centerYAnchor
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
    [_statusContainerView.trailingAnchor
        constraintEqualToAnchor:contentView.trailingAnchor
                       constant:-kTableViewHorizontalSpacing],
    _managementIconTrailingNoStatusMarginConstraint,
    [_detailTextLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:_managementIconView.leadingAnchor
                                 constant:
                                     -kHorizontalPaddingBetweenTextAndStatus],
    [_textLabel.leadingAnchor
        constraintEqualToAnchor:_imageView.trailingAnchor
                       constant:kTableViewOneLabelCellVerticalSpacing],
    [_textLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:_managementIconView.leadingAnchor
                                 constant:
                                     -kHorizontalPaddingBetweenTextAndStatus],

  ]];
  _statusContainerView.hidden = YES;
  _managementIconView.hidden = YES;

  // This is needed so the image doesn't get pushed out if both text and detail
  // are long.
  [_textLabel
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [_detailTextLabel
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisHorizontal];
}

- (void)setStatusView:(UIView*)statusView {
  _statusContainerView.hidden = statusView == nil;
  if (statusView == nil) {
    _managementIconTrailingWithStatusMarginConstraint.active = NO;
    _managementIconTrailingNoStatusMarginConstraint.active = YES;
    _statusView = nil;
    // Hide the status view but don’t delete it so that constraints still holds.
    return;
  }
  _managementIconTrailingWithStatusMarginConstraint.active = YES;
  _managementIconTrailingNoStatusMarginConstraint.active = NO;
  [_statusView removeFromSuperview];
  _statusView = statusView;
  [_statusContainerView addSubview:statusView];
  AddSameConstraints(_statusContainerView, statusView);
}

- (void)setStatusViewWithImage:(UIImage*)statusImage {
  CHECK(statusImage);

  UIImageView* statusIcon = [[UIImageView alloc] init];
  statusIcon.tintColor = [UIColor colorNamed:kRed500Color];
  statusIcon.translatesAutoresizingMaskIntoConstraints = NO;
  statusIcon.image = statusImage;
  [NSLayoutConstraint activateConstraints:@[
    [statusIcon.widthAnchor constraintEqualToConstant:kTableViewIconImageSize],
    [statusIcon.heightAnchor constraintEqualToAnchor:statusIcon.widthAnchor],
  ]];
  [self setStatusView:statusIcon];
}

// Shows or hides the enterprise building icon.
- (void)showManagementIcon:(BOOL)show {
  // Hide the icon while the separate profile for managed accounts feature is
  // not enabled, or if we explicitely hide the icon.
  _managementIconView.hidden =
      !AreSeparateProfilesForManagedAccountsEnabled() || !show;
  [_managementIconView
      setContentCompressionResistancePriority:_managementIconView.hidden
                                                  ? UILayoutPriorityDefaultLow
                                                  : UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  self.imageView.image = nil;
  self.textLabel.text = nil;
  self.detailTextLabel.text = nil;
  self.accessibilityLabel = nil;
  self.textLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  self.detailTextLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  [self setStatusView:nil];
  self.userInteractionEnabled = YES;
  self.contentView.alpha = 1;
  UIImageView* accessoryImage =
      base::apple::ObjCCastStrict<UIImageView>(self.accessoryView);
  accessoryImage.tintColor =
      [accessoryImage.tintColor colorWithAlphaComponent:1];
}

#pragma mark - NSObject(Accessibility)

- (NSString*)accessibilityLabel {
  // The accessibilityLabel property may have been set by the owner of this
  // cell.
  if (super.accessibilityLabel) {
    // If the value was explicitly set, use it.
    return super.accessibilityLabel;
  }
  // Default value.
  return self.textLabel.text;
}

- (NSString*)accessibilityValue {
  UIImageView* statusIcon = base::apple::ObjCCast<UIImageView>(_statusView);
  if (statusIcon.image != nil) {
    return l10n_util::GetNSString(
        IDS_IOS_ITEM_ACCOUNT_ERROR_BADGE_ACCESSIBILITY_HINT);
  }
  return nil;
}

- (NSArray<NSString*>*)accessibilityUserInputLabels {
  NSMutableArray<NSString*>* userInputLabels = [[NSMutableArray alloc] init];
  if (self.textLabel.text) {
    [userInputLabels addObject:self.textLabel.text];
  }

  return userInputLabels;
}

@end
