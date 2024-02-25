// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/cells/table_view_central_account_item.h"

#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/browser/ui/settings/cells/settings_cells_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@implementation TableViewCentralAccountItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewCentralAccountCell class];
    self.accessibilityTraits |= UIAccessibilityTraitButton;
  }
  return self;
}

#pragma mark - TableViewItem

- (void)configureCell:(TableViewCentralAccountCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  CHECK(self.avatarImage);
  CHECK(self.email);

  [super configureCell:cell withStyler:styler];

  CGSize tableViewCentralAccountAvartarSize =
      GetSizeForIdentityAvatarSize(IdentityAvatarSize::Large);
  CHECK_EQ(tableViewCentralAccountAvartarSize.width,
           self.avatarImage.size.width);
  CHECK_EQ(tableViewCentralAccountAvartarSize.height,
           self.avatarImage.size.height);

  cell.avatarImageView.image = self.avatarImage;
  cell.nameLabel.text = self.name ? self.name : self.email;
  cell.emailLabel.text = self.name ? self.email : nil;
  cell.backgroundColor = styler.tableViewBackgroundColor;
}

@end

@implementation TableViewCentralAccountCell

@synthesize avatarImageView = _avatarImageView;
@synthesize nameLabel = _textLabel;
@synthesize emailLabel = _detailTextLabel;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.isAccessibilityElement = YES;
    [self addSubviews];
    [self setViewConstraints];
    self.userInteractionEnabled = NO;
    self.contentView.alpha = 1;
  }
  return self;
}

// Create and add subviews.
- (void)addSubviews {
  UIView* contentView = self.contentView;
  contentView.clipsToBounds = YES;

  _avatarImageView = [[UIImageView alloc] init];
  _avatarImageView.translatesAutoresizingMaskIntoConstraints = NO;
  _avatarImageView.layer.masksToBounds = YES;
  _avatarImageView.contentMode = UIViewContentModeScaleAspectFit;
  // Creates the image rounded corners.
  _avatarImageView.layer.cornerRadius =
      GetSizeForIdentityAvatarSize(IdentityAvatarSize::Large).width / 2.0f;
  [contentView addSubview:_avatarImageView];

  _textLabel = [[UILabel alloc] init];
  _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
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

  [NSLayoutConstraint activateConstraints:@[
    // Fix image widths.
    [_avatarImageView.widthAnchor
        constraintEqualToConstant:GetSizeForIdentityAvatarSize(
                                      IdentityAvatarSize::Large)
                                      .width],
    [_avatarImageView.heightAnchor
        constraintEqualToAnchor:_avatarImageView.widthAnchor],

    // Set horizontal anchors.
    [_avatarImageView.centerXAnchor
        constraintEqualToAnchor:contentView.centerXAnchor],
    [_textLabel.centerXAnchor
        constraintEqualToAnchor:_avatarImageView.centerXAnchor],
    [_detailTextLabel.centerXAnchor
        constraintEqualToAnchor:_textLabel.centerXAnchor],

    // Set vertical anchors.
    [_avatarImageView.topAnchor constraintEqualToAnchor:contentView.topAnchor],
    [_textLabel.topAnchor
        constraintGreaterThanOrEqualToAnchor:_avatarImageView.bottomAnchor
                                    constant:kTableViewVerticalSpacing],
    [_detailTextLabel.topAnchor
        constraintEqualToAnchor:_textLabel.bottomAnchor],
    [_detailTextLabel.bottomAnchor
        constraintEqualToAnchor:contentView.bottomAnchor],
  ]];
}

#pragma mark - UIView

- (void)layoutSubviews {
  [super layoutSubviews];
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  self.avatarImageView.image = nil;
  self.nameLabel.text = nil;
  self.emailLabel.text = nil;
}

#pragma mark - NSObject(Accessibility)

- (NSString*)accessibilityLabel {
  return self.nameLabel.text;
}

- (NSString*)accessibilityValue {
  return self.emailLabel.text;
}

- (NSArray<NSString*>*)accessibilityUserInputLabels {
  if (!self.nameLabel.text) {
    return @[];
  }
  return @[ self.nameLabel.text ];
}

@end
