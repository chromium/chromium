// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/table_view/favicon_table_view_cell.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_container_view.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/table_view/table_view_url_cell_favicon_badge_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#pragma mark - TableViewURLCell

@interface FaviconTableViewCell ()
// Container View for the faviconView.
@property(nonatomic, strong) FaviconContainerView* faviconContainerView;
@end

@implementation FaviconTableViewCell

@synthesize textLabel = _textLabel;
@synthesize detailTextLabel = _detailTextLabel;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];

  if (self) {
    _faviconContainerView = [[FaviconContainerView alloc] init];
    _faviconBadgeView = [[TableViewURLCellFaviconBadgeView alloc] init];
    _textLabel = [[UILabel alloc] init];
    _detailTextLabel = [[UILabel alloc] init];

    // Set font sizes using dynamic type.
    _textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _textLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _textLabel.adjustsFontForContentSizeCategory = YES;
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _detailTextLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _detailTextLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _detailTextLabel.adjustsFontForContentSizeCategory = YES;
    _detailTextLabel.translatesAutoresizingMaskIntoConstraints = NO;

    UIView* contentView = self.contentView;

    // Use stack views to layout the subviews except for the favicon.
    UIStackView* verticalStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _textLabel, _detailTextLabel ]];
    verticalStack.axis = UILayoutConstraintAxisVertical;
    verticalStack.translatesAutoresizingMaskIntoConstraints = NO;

    _faviconContainerView.translatesAutoresizingMaskIntoConstraints = NO;
    _faviconBadgeView.translatesAutoresizingMaskIntoConstraints = NO;
    [contentView addSubview:_faviconContainerView];
    [contentView addSubview:_faviconBadgeView];
    [contentView addSubview:verticalStack];

    NSLayoutConstraint* heightConstraint = [contentView.heightAnchor
        constraintGreaterThanOrEqualToConstant:kChromeTableViewCellHeight];
    // Don't set the priority to required to avoid clashing with the estimated
    // height.
    heightConstraint.priority = UILayoutPriorityRequired - 1;

    [NSLayoutConstraint activateConstraints:@[
      heightConstraint,
      [_faviconContainerView.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_faviconContainerView.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],
      [_faviconBadgeView.centerXAnchor
          constraintEqualToAnchor:_faviconContainerView.trailingAnchor],
      [_faviconBadgeView.centerYAnchor
          constraintEqualToAnchor:_faviconContainerView.topAnchor],
      [verticalStack.leadingAnchor
          constraintEqualToAnchor:_faviconContainerView.trailingAnchor
                         constant:kTableViewSubViewHorizontalSpacing],
      [verticalStack.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
      [verticalStack.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor]
    ]];
  }
  return self;
}

- (FaviconView*)faviconView {
  return self.faviconContainerView.faviconView;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.textLabel.text = nil;
  self.detailTextLabel.text = nil;
  [self.faviconView configureWithAttributes:nil];
  self.faviconBadgeView.image = nil;
}

@end
