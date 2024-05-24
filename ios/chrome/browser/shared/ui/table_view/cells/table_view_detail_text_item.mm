// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

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

  cell.allowMultilineDetailText = self.allowMultilineDetailText;
  if (self.allowMultilineDetailText) {
    cell.detailTextLabel.numberOfLines = 0;
  }

  // Styling.
  if (self.textColor) {
    cell.textLabel.textColor = self.textColor;
  } else if (styler.cellTitleColor) {
    cell.textLabel.textColor = styler.cellTitleColor;
  } else {
    cell.textLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  }
  if (self.detailTextColor) {
    cell.detailTextLabel.textColor = self.detailTextColor;
  } else {
    cell.detailTextLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  }
  cell.textLabel.textAlignment =
      self.textAlignment ? self.textAlignment : NSTextAlignmentNatural;
  cell.detailTextLabel.textAlignment =
      self.textAlignment ? self.textAlignment : NSTextAlignmentNatural;

  // Accessory symbol.
  switch (self.accessorySymbol) {
    case TableViewDetailTextCellAccessorySymbolChevron:
      cell.accessoryView = [[UIImageView alloc]
          initWithImage:DefaultSymbolTemplateWithPointSize(
                            kChevronForwardSymbol, kSymbolAccessoryPointSize)];
      break;
    case TableViewDetailTextCellAccessorySymbolExternalLink:
      cell.accessoryView = [[UIImageView alloc]
          initWithImage:DefaultSymbolTemplateWithPointSize(
                            kExternalLinkSymbol, kSymbolAccessoryPointSize)];
      break;
    case TableViewDetailTextCellAccessorySymbolNone:
      cell.accessoryView = nil;
      break;
  }
  if (cell.accessoryView) {
    // Hard code color until other use cases arise.
    cell.accessoryView.tintColor = [UIColor colorNamed:kTextQuaternaryColor];
  }
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
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
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

    // Make sure there are top and bottom margins of at least `margin`.
    AddOptionalVerticalPadding(self.contentView, containerView,
                               kTableViewTwoLabelsCellVerticalSpacing);
  }
  return self;
}

#pragma mark - Properties

- (void)setAllowMultilineDetailText:(BOOL)allowMultilineDetailText {
  _allowMultilineDetailText = allowMultilineDetailText;

  _detailTextLabel.numberOfLines = _allowMultilineDetailText ? 0 : 1;
}

#pragma mark - UIReusableView

- (void)prepareForReuse {
  [super prepareForReuse];
  self.textLabel.text = nil;
  self.detailTextLabel.text = nil;
  self.accessoryView = nil;
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
    if (self.allowMultilineDetailText) {
      self.detailTextLabel.numberOfLines = 0;
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
