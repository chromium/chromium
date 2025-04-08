// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/recent_activity_log_cell.h"

#import "ios/chrome/common/ui/favicon/favicon_container_view.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"

@implementation RecentActivityLogCell {
  // Container view that displays the `faviconView`.
  FaviconContainerView* _faviconContainerView;
}

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.isAccessibilityElement = YES;
    _avatarView = [[UIView alloc] init];
    _uniqueIdentifier = [[NSUUID UUID] UUIDString];

    _faviconContainerView = [[FaviconContainerView alloc] init];
    _faviconContainerView.translatesAutoresizingMaskIntoConstraints = NO;

    // Set font size using dynamic type.
    _titleLabel = [[UILabel alloc] init];
    _titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _titleLabel.adjustsFontForContentSizeCategory = YES;
    _titleLabel.numberOfLines = 2;
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
          _avatarView, verticalStack, _faviconContainerView
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

#pragma mark - Getters

- (FaviconView*)faviconView {
  return _faviconContainerView.faviconView;
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  _uniqueIdentifier = nil;
  _titleLabel.text = nil;
  _descriptionLabel.text = nil;
  _faviconContainerView = nil;
  for (UIView* subview in _avatarView.subviews) {
    [subview removeFromSuperview];
  }
}

@end
