// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/cells/whats_new_table_view_item.h"

#import "base/check.h"
#import "ios/chrome/browser/ui/icons/chrome_symbol.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The size of the space between text labels in the text stack view.
const CGFloat kTextStackViewHorizontalSpacings = 6.0;
// The size of the main background image.
const CGFloat kMainBackgroundImageSize = 64;
// The size of the icon background image.
const CGFloat kIconBackgroundImageSize = 32;
// The size of the icon.
const CGFloat kIconSize = 16;
// The size of the space between the cells
const CGFloat kTableViewCellVerticalSpacing = 25.5;

}  // namespace

@implementation WhatsNewTableViewItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [WhatsNewTableViewCell class];
  }
  return self;
}

#pragma mark TableViewItem

- (void)configureCell:(WhatsNewTableViewCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];

  self.accessibilityTraits |= UIAccessibilityTraitButton;

  cell.textLabel.text = self.title;
  cell.detailTextLabel.text = self.detailText;
  cell.iconView.image = self.iconImage;
  cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;

  if (self.iconBackgroundColor) {
    cell.iconBackgroundImageView.tintColor = self.iconBackgroundColor;
  } else {
    // Hide rounded corners background image view for icon when
    // iconBackgroundColor is empty.
    cell.iconBackgroundImageView.hidden = YES;
    [cell updateImageConstraintWhenNoBackground];
  }
}

@end

#pragma mark - WhatsNewTableViewCell

@interface WhatsNewTableViewCell ()

// View containing UILabels text and detailText.
@property(nonatomic, strong) UIStackView* textStackView;
// Image width constraint.
@property(nonatomic, strong) NSLayoutConstraint* imageWidthAnchorConstraint;

@end

@implementation WhatsNewTableViewCell

@synthesize detailTextLabel = _detailTextLabel;
@synthesize textLabel = _textLabel;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.isAccessibilityElement = YES;
    UIView* contentView = self.contentView;

    // Main background view with rounded corners.
    UIImageView* mainBackgroundImageView =
        [[UIImageView alloc] initWithFrame:self.bounds];
    mainBackgroundImageView.translatesAutoresizingMaskIntoConstraints = NO;
    UIImage* backgroundImage = [[UIImage imageNamed:@"whats_new_icon_tile"]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    mainBackgroundImageView.image = backgroundImage;
    mainBackgroundImageView.tintColor =
        [UIColor colorNamed:kSecondaryBackgroundColor];
    [contentView addSubview:mainBackgroundImageView];

    // Icon's background view with rounded corners.
    _iconBackgroundImageView = [[UIImageView alloc] initWithFrame:self.bounds];
    _iconBackgroundImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _iconBackgroundImageView.image = backgroundImage;
    [contentView addSubview:_iconBackgroundImageView];

    // Icon's container view.
    UIView* iconContainerView = [[UIView alloc] init];
    iconContainerView.translatesAutoresizingMaskIntoConstraints = NO;
    [contentView addSubview:iconContainerView];

    // Icon's view.
    _iconView = [[UIImageView alloc] initWithFrame:self.bounds];
    _iconView.translatesAutoresizingMaskIntoConstraints = NO;
    _iconView.tintColor = [UIColor colorNamed:kBackgroundColor];
    _iconView.contentMode = UIViewContentModeCenter;
    [iconContainerView addSubview:_iconView];

    // Text Label.
    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    UIFont* font = [UIFont systemFontOfSize:17 weight:UIFontWeightSemibold];
    UIFontMetrics* fontMetrics =
        [UIFontMetrics metricsForTextStyle:UIFontTextStyleBody];
    _textLabel.font = [fontMetrics scaledFontForFont:font];
    _textLabel.adjustsFontForContentSizeCategory = YES;
    _textLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _textLabel.backgroundColor = UIColor.clearColor;
    _textLabel.numberOfLines = 1;

    // Detail text Label.
    _detailTextLabel = [[UILabel alloc] init];
    _detailTextLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _detailTextLabel.font = [[UIFont
        preferredFontForTextStyle:UIFontTextStyleFootnote] fontWithSize:13];
    _detailTextLabel.adjustsFontForContentSizeCategory = YES;
    _detailTextLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _detailTextLabel.backgroundColor = UIColor.clearColor;
    _detailTextLabel.numberOfLines = 2;

    // Stack View containing two UILabels.
    _textStackView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _textLabel, _detailTextLabel ]];
    _textStackView.axis = UILayoutConstraintAxisVertical;
    _textStackView.spacing = kTextStackViewHorizontalSpacings;
    _textStackView.distribution = UIStackViewDistributionFill;
    _textStackView.alignment = UIStackViewAlignmentFill;
    _textStackView.translatesAutoresizingMaskIntoConstraints = NO;
    [contentView addSubview:_textStackView];

    _imageWidthAnchorConstraint = [_iconBackgroundImageView.widthAnchor
        constraintEqualToConstant:kIconBackgroundImageSize];

    [NSLayoutConstraint activateConstraints:@[
      // Main background constraints.
      [mainBackgroundImageView.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [mainBackgroundImageView.widthAnchor
          constraintEqualToConstant:kMainBackgroundImageSize],
      [mainBackgroundImageView.heightAnchor
          constraintEqualToAnchor:mainBackgroundImageView.widthAnchor],
      [mainBackgroundImageView.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],

      // Icon background constraints.
      _imageWidthAnchorConstraint,
      [_iconBackgroundImageView.heightAnchor
          constraintEqualToAnchor:_iconBackgroundImageView.widthAnchor],

      // Icon constraints.
      [_iconView.widthAnchor constraintEqualToConstant:kIconSize],
      [_iconView.heightAnchor constraintEqualToAnchor:_iconView.widthAnchor],

      // Stack view constraints.
      [_textStackView.leadingAnchor
          constraintEqualToAnchor:mainBackgroundImageView.trailingAnchor
                         constant:kTableViewImagePadding],
      [_textStackView.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
      [_textStackView.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],
    ]];

    AddSameCenterConstraints(iconContainerView, mainBackgroundImageView);
    AddSameCenterConstraints(_iconBackgroundImageView, mainBackgroundImageView);
    AddSameConstraints(_iconView, iconContainerView);
    AddOptionalVerticalPadding(contentView, _textStackView,
                               kTableViewCellVerticalSpacing);
  }
  return self;
}

- (void)updateImageConstraintWhenNoBackground {
  self.imageWidthAnchorConstraint.constant = kIconBackgroundImageSize;
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];

  self.textLabel.text = nil;
  self.detailTextLabel.text = nil;
  self.iconView.image = nil;
  self.iconBackgroundImageView.tintColor = nil;
  self.iconBackgroundImageView.hidden = NO;
  self.imageWidthAnchorConstraint.constant = kIconSize;
}

#pragma mark - Private

- (NSString*)accessibilityLabel {
  return self.textLabel.text;
}

- (NSString*)accessibilityValue {
  return self.detailTextLabel.text;
}

@end
