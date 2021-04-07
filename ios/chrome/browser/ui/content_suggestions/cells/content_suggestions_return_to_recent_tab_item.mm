// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_return_to_recent_tab_item.h"

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_gesture_commands.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/common/ui/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGSize regularCellSize = {/*width=*/343, /*height=*/72};
const CGFloat kContentViewCornerRadius = 12.0f;
const CGFloat kContentViewBorderWidth = 1.0f;
const CGFloat kIconCornerRadius = 4.0f;
const CGFloat kContentViewSubviewSpacing = 12.0f;
const CGFloat kIconWidth = 32.0f;
}

@implementation ContentSuggestionsReturnToRecentTabItem
@synthesize metricsRecorded;
@synthesize suggestionIdentifier;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [ContentSuggestionsReturnToRecentTabCell class];
  }
  return self;
}

- (void)configureCell:(ContentSuggestionsReturnToRecentTabCell*)cell {
  [super configureCell:cell];
  [cell setTitle:self.title];
  [cell setSubtitle:self.subtitle];
  cell.accessibilityLabel = self.title;
  if (self.icon) {
    [cell setIconImage:self.icon];
  }
  cell.accessibilityCustomActions = [self customActions];
}

- (CGFloat)cellHeightForWidth:(CGFloat)width {
  return [ContentSuggestionsReturnToRecentTabCell defaultSize].height;
}

// Custom action for a cell configured with this item.
- (NSArray<UIAccessibilityCustomAction*>*)customActions {
  UIAccessibilityCustomAction* openMostRecentTab =
      [[UIAccessibilityCustomAction alloc]
          initWithName:@"Open Most Recent Tab"
                target:self
              selector:@selector(openMostRecentTab)];

  return @[ openMostRecentTab ];
}

- (BOOL)openMostRecentTab {
  // TODO:(crbug.com/1173160) implement.
  return YES;
}

@end

#pragma mark - ContentSuggestionsReturnToRecentTabCell

@interface ContentSuggestionsReturnToRecentTabCell ()

// Favicon image.
@property(nonatomic, strong) UIImageView* iconImageView;

// Title of the most recent tab tile.
@property(nonatomic, strong, readonly) UILabel* titleLabel;

// Subtitle of the most recent tab tile.
@property(nonatomic, strong, readonly) UILabel* subtitleLabel;

@end

@implementation ContentSuggestionsReturnToRecentTabCell

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.isAccessibilityElement = YES;
    UIView* contentView = self.contentView;
    [contentView.layer
        setBorderColor:[UIColor colorNamed:kTertiaryBackgroundColor].CGColor];
    [contentView.layer setBorderWidth:kContentViewBorderWidth];
    contentView.layer.cornerRadius = kContentViewCornerRadius;
    contentView.layer.masksToBounds = YES;

    _titleLabel = [[UILabel alloc] init];
    _titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _titleLabel.adjustsFontForContentSizeCategory = YES;
    _titleLabel.textColor = UIColor.cr_labelColor;
    _titleLabel.backgroundColor = UIColor.clearColor;

    _subtitleLabel = [[UILabel alloc] init];
    _subtitleLabel.font =
        [UIFont preferredFontForTextStyle:kTableViewSublabelFontStyle];
    _subtitleLabel.lineBreakMode = NSLineBreakByTruncatingMiddle;
    _subtitleLabel.adjustsFontForContentSizeCategory = YES;
    _subtitleLabel.textColor = UIColor.cr_secondaryLabelColor;
    _subtitleLabel.backgroundColor = UIColor.clearColor;

    UIStackView* textStackView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _titleLabel, _subtitleLabel ]];
    textStackView.axis = UILayoutConstraintAxisVertical;
    [contentView addSubview:textStackView];

    _iconImageView = [[UIImageView alloc]
        initWithImage:[UIImage imageNamed:@"default_world_favicon_regular"]];
    _iconImageView.layer.cornerRadius = kIconCornerRadius;
    _iconImageView.layer.masksToBounds = YES;
    [contentView addSubview:_iconImageView];

    UIImageView* disclosureImageView = [[UIImageView alloc]
        initWithImage:[UIImage imageNamed:@"table_view_cell_chevron"]];
    [disclosureImageView
        setContentHuggingPriority:UILayoutPriorityDefaultHigh
                          forAxis:UILayoutConstraintAxisHorizontal];
    [contentView addSubview:disclosureImageView];

    UIStackView* horizontalStackView =
        [[UIStackView alloc] initWithArrangedSubviews:@[
          _iconImageView, textStackView, disclosureImageView
        ]];
    horizontalStackView.translatesAutoresizingMaskIntoConstraints = NO;
    horizontalStackView.axis = UILayoutConstraintAxisHorizontal;
    horizontalStackView.alignment = UIStackViewAlignmentCenter;
    horizontalStackView.spacing = kContentViewSubviewSpacing;
    [contentView addSubview:horizontalStackView];

    [NSLayoutConstraint activateConstraints:@[
      [_iconImageView.widthAnchor constraintEqualToConstant:kIconWidth],
      [_iconImageView.heightAnchor
          constraintEqualToAnchor:_iconImageView.widthAnchor],
      [horizontalStackView.topAnchor
          constraintEqualToAnchor:contentView.topAnchor],
      [horizontalStackView.bottomAnchor
          constraintEqualToAnchor:contentView.bottomAnchor],
      [horizontalStackView.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kContentViewSubviewSpacing],
      [horizontalStackView.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor
                         constant:-kContentViewSubviewSpacing],
    ]];
  }
  return self;
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (self.traitCollection.userInterfaceStyle !=
      previousTraitCollection.userInterfaceStyle) {
    // CGColors are static RGB, so the border color needs to be reset.
    [self.contentView.layer
        setBorderColor:[UIColor colorNamed:kTertiaryBackgroundColor].CGColor];
  }
}

- (void)setTitle:(NSString*)title {
  self.titleLabel.text = title;
}

- (void)setSubtitle:(NSString*)subtitle {
  self.subtitleLabel.text = subtitle;
}

+ (CGSize)defaultSize {
  return regularCellSize;
}

- (void)setIconImage:(UIImage*)image {
  _iconImageView.image = image;
  _iconImageView.hidden = image == nil;
}

@end
