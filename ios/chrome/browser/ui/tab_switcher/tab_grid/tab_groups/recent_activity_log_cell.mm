// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/recent_activity_log_cell.h"

#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"

@implementation RecentActivityLogCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.isAccessibilityElement = YES;
    _iconImageView = [[UIImageView alloc] init];
    // The user's icon is smaller than its UIImageView's bounds, so center it.
    _iconImageView.contentMode = UIViewContentModeCenter;
    [_iconImageView setContentHuggingPriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];

    // The favicon image is smaller than its UIImageView's bounds, so center
    // it.
    _faviconImageView = [[UIImageView alloc] init];
    _faviconImageView.contentMode = UIViewContentModeCenter;
    [_faviconImageView
        setContentHuggingPriority:UILayoutPriorityRequired
                          forAxis:UILayoutConstraintAxisHorizontal];

    // Set font size using dynamic type.
    _titleLabel = [[UILabel alloc] init];
    _titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _titleLabel.adjustsFontForContentSizeCategory = YES;
    _titleLabel.numberOfLines = 1;
    [_titleLabel
        setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    _descriptionLabel = [[UILabel alloc] init];
    _descriptionLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _descriptionLabel.adjustsFontForContentSizeCategory = YES;
    _descriptionLabel.numberOfLines = 1;

    UIStackView* verticalStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _titleLabel, _descriptionLabel ]];
    verticalStack.translatesAutoresizingMaskIntoConstraints = NO;
    verticalStack.axis = UILayoutConstraintAxisVertical;
    verticalStack.spacing = 0;
    verticalStack.distribution = UIStackViewDistributionFill;
    verticalStack.alignment = UIStackViewAlignmentLeading;
    [self.contentView addSubview:verticalStack];

    UIStackView* horizontalStack =
        [[UIStackView alloc] initWithArrangedSubviews:@[
          _iconImageView, verticalStack, _faviconImageView
        ]];
    horizontalStack.translatesAutoresizingMaskIntoConstraints = NO;
    horizontalStack.axis = UILayoutConstraintAxisHorizontal;
    horizontalStack.spacing = kTableViewSubViewHorizontalSpacing;
    horizontalStack.distribution = UIStackViewDistributionFill;
    horizontalStack.alignment = UIStackViewAlignmentCenter;
    [self.contentView addSubview:horizontalStack];

    NSLayoutConstraint* heightConstraint = [self.contentView.heightAnchor
        constraintGreaterThanOrEqualToConstant:kChromeTableViewCellHeight];
    // Don't set the priority to required to avoid clashing with the estimated
    // height.
    heightConstraint.priority = UILayoutPriorityRequired - 1;
    [NSLayoutConstraint activateConstraints:@[
      // Horizontal Stack constraints.
      [horizontalStack.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [horizontalStack.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
      [horizontalStack.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
      [horizontalStack.topAnchor
          constraintGreaterThanOrEqualToAnchor:self.contentView.topAnchor
                                      constant:kTableViewVerticalSpacing],
      [horizontalStack.bottomAnchor
          constraintLessThanOrEqualToAnchor:self.contentView.bottomAnchor
                                   constant:-kTableViewVerticalSpacing],
      heightConstraint,
    ]];
  }
  return self;
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  _titleLabel.text = nil;
  _descriptionLabel.text = nil;
  _iconImageView.image = nil;
  _faviconImageView.image = nil;
}

@end
