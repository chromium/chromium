// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/best_features/ui/best_features_cell.h"

#import "ios/chrome/browser/first_run/public/best_features_item.h"
#import "ios/chrome/browser/first_run/ui_bundled/best_features/ui/best_features_constants.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// The size of the space between text labels in the text stack view.
const CGFloat kTextStackViewHorizontalSpacings = 4.0;
// Shadow radius for the icon background.
const CGFloat kIconBackgroundShadowRadius = 2.0;
// Shadow opacity for the icon background.
const CGFloat kIconBackgroundShadowOpacity = 0.3;
// Corner radius for the icon background.
const CGFloat kIconBackgroundCornerRadius = 10.0;
// Width for the icon background.
const CGFloat kIconBackgroundWidth = 37.0;
// The size of the space between the cells
const CGFloat kTableViewCellVerticalSpacing = 18.0;
// Trailing horizontal margin for the content view.
const CGFloat kContentViewTrailingMargin = 13.0;

}  // namespace

@implementation BestFeaturesCell {
  // Background view for the icon.
  UIView* _iconBackgroundView;
  // Image view for the icon.
  UIImageView* _iconImageView;
  // Label for the main text.
  UILabel* _textLabel;
  // Label for the detail text.
  UILabel* _detailTextLabel;
}

@synthesize detailTextLabel = _detailTextLabel;
@synthesize textLabel = _textLabel;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.isAccessibilityElement = YES;
    UIView* contentView = self.contentView;

    // View to appear behind/contain the icon.
    _iconBackgroundView = [[UIView alloc] init];
    _iconBackgroundView.translatesAutoresizingMaskIntoConstraints = NO;
    _iconBackgroundView.layer.shadowColor = [UIColor blackColor].CGColor;
    _iconBackgroundView.layer.shadowRadius = kIconBackgroundShadowRadius;
    _iconBackgroundView.layer.shadowOpacity = kIconBackgroundShadowOpacity;
    _iconBackgroundView.layer.shadowOffset = CGSizeMake(0, 2);
    _iconBackgroundView.layer.cornerRadius = kIconBackgroundCornerRadius;
    _iconBackgroundView.layer.masksToBounds = NO;
    [contentView addSubview:_iconBackgroundView];

    // Icon view.
    _iconImageView = [[UIImageView alloc] init];
    _iconImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _iconImageView.contentMode = UIViewContentModeCenter;
    [_iconBackgroundView addSubview:_iconImageView];

    // Text label.
    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.font =
        CreateDynamicFont(UIFontTextStyleBody, UIFontWeightRegular);
    _textLabel.adjustsFontForContentSizeCategory = YES;
    _textLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _textLabel.backgroundColor = UIColor.clearColor;
    _textLabel.numberOfLines = 2;

    // Detail text label.
    _detailTextLabel = [[UILabel alloc] init];
    _detailTextLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _detailTextLabel.font =
        CreateDynamicFont(UIFontTextStyleFootnote, UIFontWeightRegular);
    _detailTextLabel.adjustsFontForContentSizeCategory = YES;
    _detailTextLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _detailTextLabel.backgroundColor = UIColor.clearColor;
    _detailTextLabel.numberOfLines = 1;

    // Stack View containing the text labels.
    UIStackView* textStackView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _textLabel, _detailTextLabel ]];
    textStackView.axis = UILayoutConstraintAxisVertical;
    textStackView.spacing = kTextStackViewHorizontalSpacings;
    textStackView.distribution = UIStackViewDistributionFill;
    textStackView.alignment = UIStackViewAlignmentFill;
    textStackView.translatesAutoresizingMaskIntoConstraints = NO;
    [contentView addSubview:textStackView];

    // Adjust the cell separator inset to align with the start of the text
    // content. The right inset aligns with the content view's trailing margin.
    self.separatorInset =
        UIEdgeInsetsMake(0,
                         kTableViewHorizontalSpacing + kIconBackgroundWidth +
                             kTableViewImagePadding,
                         0, kContentViewTrailingMargin);

    [NSLayoutConstraint activateConstraints:@[
      // Icon background constraints.
      [_iconBackgroundView.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_iconBackgroundView.widthAnchor
          constraintEqualToConstant:kIconBackgroundWidth],
      [_iconBackgroundView.heightAnchor
          constraintEqualToAnchor:_iconBackgroundView.widthAnchor],
      [_iconBackgroundView.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],

      // Stack view constraints.
      [textStackView.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor
                         constant:-kContentViewTrailingMargin],
      [textStackView.leadingAnchor
          constraintEqualToAnchor:_iconBackgroundView.trailingAnchor
                         constant:kTableViewImagePadding],
      [textStackView.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],

      // Text label constraints.
      [_textLabel.topAnchor constraintEqualToAnchor:textStackView.topAnchor],
    ]];

    AddSameCenterConstraints(_iconBackgroundView, _iconImageView);
    AddOptionalVerticalPadding(contentView, textStackView,
                               kTableViewCellVerticalSpacing);
  }
  return self;
}

- (void)setBestFeaturesItem:(BestFeaturesItem*)item {
  _textLabel.text = item.title;
  _textLabel.accessibilityIdentifier =
      [kBestFeaturesCellAccessibilityPrefix stringByAppendingString:item.title];
  _detailTextLabel.text = item.caption;
  _iconImageView.image = item.iconImage;
  _iconBackgroundView.backgroundColor = item.iconBackgroundColor;
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];

  _textLabel.text = nil;
  _detailTextLabel.text = nil;
  _iconImageView.image = nil;
  _iconBackgroundView.backgroundColor = nil;
}

#pragma mark - UIAccessibility

- (NSString*)accessibilityLabel {
  return self.textLabel.text;
}

- (NSString*)accessibilityValue {
  return self.detailTextLabel.text;
}

@end
