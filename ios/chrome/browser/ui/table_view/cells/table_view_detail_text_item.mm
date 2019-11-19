// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_text_item.h"

#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - TableViewDetailTextItem

@implementation TableViewDetailTextItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewDetailTextCell class];
  }
  return self;
}

#pragma mark - TableViewItem

- (void)configureCell:(TableViewDetailTextCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  cell.textLabel.text = self.text;
  cell.detailTextLabel.text = self.detailText;
  cell.isAccessibilityElement = YES;
  if ([self.accessibilityLabel length] != 0) {
    cell.accessibilityLabel = self.accessibilityLabel;
  } else {
    if (self.detailText.length == 0) {
      cell.accessibilityLabel = self.text;
    } else {
      cell.accessibilityLabel =
          [NSString stringWithFormat:@"%@, %@", self.text, self.detailText];
    }
  }

  // Styling.
  if (self.textColor) {
    cell.textLabel.textColor = self.textColor;
  } else if (styler.cellTitleColor) {
    cell.textLabel.textColor = styler.cellTitleColor;
  } else {
    cell.textLabel.textColor = UIColor.cr_labelColor;
  }
  if (self.detailTextColor) {
    cell.detailTextLabel.textColor = self.detailTextColor;
  } else {
    cell.detailTextLabel.textColor = UIColor.cr_secondaryLabelColor;
  }
  cell.textLabel.textAlignment =
      self.textAlignment ? self.textAlignment : NSTextAlignmentNatural;
  cell.detailTextLabel.textAlignment =
      self.textAlignment ? self.textAlignment : NSTextAlignmentNatural;
}

@end

#pragma mark - TableViewDetailTextCell

@implementation TableViewDetailTextCell

@synthesize textLabel = _textLabel;
@synthesize detailTextLabel = _detailTextLabel;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    UIView* containerView = [[UIView alloc] initWithFrame:CGRectZero];
    containerView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:containerView];

    _textLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _textLabel.adjustsFontForContentSizeCategory = YES;
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [containerView addSubview:_textLabel];

    _detailTextLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _detailTextLabel.font =
        [UIFont preferredFontForTextStyle:kTableViewSublabelFontStyle];
    _detailTextLabel.adjustsFontForContentSizeCategory = YES;
    _detailTextLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [containerView addSubview:_detailTextLabel];

    NSLayoutConstraint* heightConstraint = [self.contentView.heightAnchor
        constraintGreaterThanOrEqualToConstant:kChromeTableViewCellHeight];
    // Don't set the priority to required to avoid clashing with the estimated
    // height.
    heightConstraint.priority = UILayoutPriorityRequired - 1;

    [NSLayoutConstraint activateConstraints:@[
      // Minimal height.
      heightConstraint,

      // Container.
      [containerView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [containerView.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
      [containerView.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],

      // Labels.
      [_textLabel.leadingAnchor
          constraintEqualToAnchor:containerView.leadingAnchor],
      [_textLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:containerView.trailingAnchor],
      [_textLabel.topAnchor constraintEqualToAnchor:containerView.topAnchor],
      [_textLabel.bottomAnchor
          constraintEqualToAnchor:_detailTextLabel.topAnchor],
      [_detailTextLabel.leadingAnchor
          constraintEqualToAnchor:_textLabel.leadingAnchor],
      [_detailTextLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:containerView.trailingAnchor],
      [_detailTextLabel.bottomAnchor
          constraintLessThanOrEqualToAnchor:containerView.bottomAnchor],
    ]];

    // Make sure there are top and bottom margins of at least |margin|.
    AddOptionalVerticalPadding(self.contentView, containerView,
                               kTableViewTwoLabelsCellVerticalSpacing);
  }
  return self;
}

#pragma mark - UIReusableView

- (void)prepareForReuse {
  [super prepareForReuse];
  self.textLabel.text = nil;
  self.detailTextLabel.text = nil;
}

#pragma mark - UIView

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  BOOL isCurrentCategoryAccessibility =
      UIContentSizeCategoryIsAccessibilityCategory(
          self.traitCollection.preferredContentSizeCategory);
  if (isCurrentCategoryAccessibility !=
      UIContentSizeCategoryIsAccessibilityCategory(
          previousTraitCollection.preferredContentSizeCategory)) {
    if (isCurrentCategoryAccessibility) {
      self.textLabel.numberOfLines = 0;
      self.detailTextLabel.numberOfLines = 0;
    } else {
      self.textLabel.numberOfLines = 1;
      self.detailTextLabel.numberOfLines = 1;
    }
  }
}

- (void)layoutSubviews {
  if (UIContentSizeCategoryIsAccessibilityCategory(
          self.traitCollection.preferredContentSizeCategory)) {
    // Make sure that the multiline labels width isn't changed when the
    // accessory is set.
    self.detailTextLabel.preferredMaxLayoutWidth =
        self.bounds.size.width -
        (kTableViewAccessoryWidth + 2 * kTableViewHorizontalSpacing);
    self.textLabel.preferredMaxLayoutWidth =
        self.bounds.size.width -
        (kTableViewAccessoryWidth + 2 * kTableViewHorizontalSpacing);
  }
  [super layoutSubviews];
}

@end
