// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/cells/table_view_stacked_details_item.h"

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation TableViewStackedDetailsItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewStackedDetailsCell class];
  }
  return self;
}

- (void)configureCell:(TableViewStackedDetailsCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];

  cell.titleLabel.text = self.titleText;

  [cell setDetails:self.detailTexts];
  [cell setDetailTextColor:self.detailTextColor
                               ? self.detailTextColor
                               : [UIColor colorNamed:kTextPrimaryColor]];

  cell.accessibilityLabel = [self accessibilityLabelForCell];
}

#pragma mark - Private

// Builds the accessibility label of the cell, concatenating the title and
// details texts.
- (NSString*)accessibilityLabelForCell {
  NSMutableString* accessibilityLabelText =
      [NSMutableString stringWithString:self.titleText];

  for (NSString* detailsText in self.detailTexts) {
    [accessibilityLabelText appendFormat:@", %@", detailsText];
  }

  return accessibilityLabelText;
}

@end

@interface TableViewStackedDetailsCell () {
  UIStackView* _columnsStackView;
  UIStackView* _detailsStackView;
  NSMutableArray<UILabel*>* _detailLabels;
}

@end

@implementation TableViewStackedDetailsCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];

  if (self) {
    self.isAccessibilityElement = YES;
    _detailLabels = [[NSMutableArray<UILabel*> alloc] init];
    [self addStaticViews];
    [self setTitleStyling];

    if (@available(iOS 17, *)) {
      NSArray<UITrait>* traits = TraitCollectionSetForTraits(
          @[ UITraitPreferredContentSizeCategory.self ]);
      __weak __typeof(self) weakSelf = self;
      UITraitChangeHandler handler = ^(id<UITraitEnvironment> traitEnvironment,
                                       UITraitCollection* previousCollection) {
        [weakSelf updateUIOnTraitChange:previousCollection];
      };
      [self registerForTraitChanges:traits withHandler:handler];
    }
  }

  return self;
}

#pragma mark - Public

- (void)setDetails:(NSArray<NSString*>*)detailTexts {
  [self createLabelsForDetails:detailTexts];
  [self setDetailsStyling];
  [self updateForAccessibilityContentSizeCategory:
            UIContentSizeCategoryIsAccessibilityCategory(
                self.traitCollection.preferredContentSizeCategory)];
}

- (void)setDetailTextColor:(UIColor*)detailTextColor {
  for (UILabel* detailsLabel in self.detailLabels) {
    detailsLabel.textColor = detailTextColor;
  }
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];

  _titleLabel.text = nil;

  // Discard existing details labels.
  for (UILabel* detailLabel in self.detailLabels) {
    [_detailsStackView removeArrangedSubview:detailLabel];
    [detailLabel removeFromSuperview];
  }
  [_detailLabels removeAllObjects];
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }
  [self updateUIOnTraitChange:previousTraitCollection];
}
#endif

#pragma mark - Private

// Adds the title and container views to the view hierarchy.
- (void)addStaticViews {
  _titleLabel = [[UILabel alloc] init];
  // Let the title take up any extra horizontal space.
  [_titleLabel setContentHuggingPriority:UILayoutPriorityDefaultLow
                                 forAxis:UILayoutConstraintAxisHorizontal];

  // Make sure the title is always displayed. Details will be clipped if there
  // is not enough width.
  [_titleLabel
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];

  _detailsStackView = [[UIStackView alloc] init];
  _detailsStackView.translatesAutoresizingMaskIntoConstraints = NO;
  _detailsStackView.axis = UILayoutConstraintAxisVertical;

  _columnsStackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _titleLabel, _detailsStackView ]];
  _columnsStackView.translatesAutoresizingMaskIntoConstraints = NO;

  UIView* contentView = self.contentView;
  [contentView addSubview:_columnsStackView];

  NSDirectionalEdgeInsets columnsStackViewInsets = NSDirectionalEdgeInsetsMake(
      kTableViewOneLabelCellVerticalSpacing, kTableViewHorizontalSpacing,
      kTableViewOneLabelCellVerticalSpacing, kTableViewHorizontalSpacing);

  AddSameConstraintsWithInsets(_columnsStackView, contentView,
                               columnsStackViewInsets);
}

// Set styling for title label.
- (void)setTitleStyling {
  _titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  _titleLabel.adjustsFontForContentSizeCategory = YES;
  _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
}

// Set styling for details labels.
- (void)setDetailsStyling {
  for (UILabel* detailsLabel in _detailLabels) {
    detailsLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    detailsLabel.adjustsFontForContentSizeCategory = YES;
  }
}

// Creates details labels and adds them to the view hierarchy.
- (void)createLabelsForDetails:(NSArray<NSString*>*)detailTexts {
  NSInteger missingLabels = detailTexts.count - _detailLabels.count;

  // Make sure there is one label per detail text.
  while (missingLabels > 0) {
    // Add enough labels for each details text.
    UILabel* newLabel = [[UILabel alloc] init];
    [newLabel
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:UILayoutConstraintAxisVertical];
    newLabel.textAlignment = NSTextAlignmentLeft;
    [_detailLabels addObject:newLabel];
    [_detailsStackView addArrangedSubview:newLabel];

    missingLabels--;
  }

  // If there are too many labels, remove them until they match the texts to
  // display.
  while (missingLabels < 0) {
    UILabel* lastLabel = _detailLabels.lastObject;
    [lastLabel removeFromSuperview];
    [_detailsStackView removeArrangedSubview:lastLabel];
    [_detailLabels removeLastObject];

    missingLabels++;
  }

  for (NSUInteger index = 0; index < detailTexts.count; index++) {
    _detailLabels[index].text = detailTexts[index];
  }
}

// Updates the cell such as it is laid out correctly with regard to the
// preferred content size category, if it is an
// `accessibilityContentSizeCategory` or not.
- (void)updateForAccessibilityContentSizeCategory:
    (BOOL)accessibilityContentSizeCategory {
  if (accessibilityContentSizeCategory) {
    _columnsStackView.axis = UILayoutConstraintAxisVertical;
    _columnsStackView.alignment = UIStackViewAlignmentLeading;
    _detailsStackView.alignment = UIStackViewAlignmentLeading;
    _columnsStackView.spacing = kTableViewVerticalSpacing;

  } else {
    _columnsStackView.axis = UILayoutConstraintAxisHorizontal;
    _columnsStackView.alignment = _detailLabels.count == 1
                                      ? UIStackViewAlignmentCenter
                                      : UIStackViewAlignmentTop;
    _detailsStackView.alignment = UIStackViewAlignmentTrailing;
    _columnsStackView.spacing = kTableViewSubViewHorizontalSpacing;
  }
}

// Updates the view's accessiblity properties when the device's
// UITraitPreferredContentSizeCategory is modified.
- (void)updateUIOnTraitChange:(UITraitCollection*)previousTraitCollection {
  BOOL isCurrentCategoryAccessibility =
      UIContentSizeCategoryIsAccessibilityCategory(
          self.traitCollection.preferredContentSizeCategory);
  if (isCurrentCategoryAccessibility !=
      UIContentSizeCategoryIsAccessibilityCategory(
          previousTraitCollection.preferredContentSizeCategory)) {
    [self updateForAccessibilityContentSizeCategory:
              isCurrentCategoryAccessibility];
  }
}

@end
