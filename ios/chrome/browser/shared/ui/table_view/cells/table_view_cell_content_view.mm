// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell_content_view.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell_content_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Constant to achieve the 75/25 ratio between the title/subtitle (75) and the
// trailing label (25).
constexpr CGFloat kTitleSubtitleToTrailingWidthRatio = 3;

}  // namespace

// Container for the title and subtitle labels. This container's intrinsic width
// is calculated based on the widest of the two labels to ensure proper layout
// and alignment within the cell, especially for the 75/25 width constraint with
// the trailing label.
@interface TableViewCellContentViewLabelContainer : UIStackView
@end

@implementation TableViewCellContentViewLabelContainer {
  UILabel* _topLabel;
  UILabel* _bottomLabel;
}

- (instancetype)initWithTopLabel:(UILabel*)topLabel
                     bottomLabel:(UILabel*)bottomLabel {
  self = [super init];
  if (self) {
    _topLabel = topLabel;
    _bottomLabel = bottomLabel;

    self.axis = UILayoutConstraintAxisVertical;
    self.distribution = UIStackViewDistributionFill;

    [self addArrangedSubview:_topLabel];
    [self addArrangedSubview:_bottomLabel];
  }
  return self;
}

- (CGSize)intrinsicContentSize {
  // Make sure to have a number of line of 1 for the labels when getting their
  // intrinsic size. Otherwise, they will choose to use several lines and have a
  // narrower width.
  CGFloat numberOfLines = _topLabel.numberOfLines;
  _topLabel.numberOfLines = 1;
  CGFloat topLeftWidth = [_topLabel intrinsicContentSize].width;
  _topLabel.numberOfLines = numberOfLines;

  numberOfLines = _bottomLabel.numberOfLines;
  _bottomLabel.numberOfLines = 1;
  CGFloat bottomLeftWidth = [_bottomLabel intrinsicContentSize].width;
  _bottomLabel.numberOfLines = numberOfLines;

  CGFloat maxWidth = MAX(topLeftWidth, bottomLeftWidth);

  return CGSizeMake(maxWidth, UIViewNoIntrinsicMetric);
}

@end

@implementation TableViewCellContentView {
  TableViewCellContentConfiguration* _configuration;

  // The leading content view.
  UIView<UIContentView>* _leadingContentView;
  // The container for the leading content view.
  UIView* _leadingContentViewContainer;

  // The labels.
  UILabel* _title;
  UILabel* _subtitle;
  UILabel* _trailingLabel;

  // The container for the text.
  UIView* _titleSubtitleContainer;
  UIStackView* _allTextStack;
}

- (instancetype)initWithConfiguration:
    (TableViewCellContentConfiguration*)configuration {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _configuration = [configuration copy];
    [self setupViews];
    [self applyConfiguration];
    [self updateForContentSizeChange];

    [self registerForTraitChanges:@[ UITraitPreferredContentSizeCategory.class ]
                       withAction:@selector(updateForContentSizeChange)];
  }
  return self;
}

#pragma mark - UIContentView

- (id<UIContentConfiguration>)configuration {
  return _configuration;
}

- (void)setConfiguration:(id<UIContentConfiguration>)configuration {
  TableViewCellContentConfiguration* chromeConfiguration =
      base::apple::ObjCCastStrict<TableViewCellContentConfiguration>(
          configuration);
  _configuration = [chromeConfiguration copy];
  [self applyConfiguration];
}

- (BOOL)supportsConfiguration:(id<UIContentConfiguration>)configuration {
  return
      [configuration isMemberOfClass:TableViewCellContentConfiguration.class];
}

#pragma mark - UIView

- (void)layoutSubviews {
  if (UIContentSizeCategoryIsAccessibilityCategory(
          self.traitCollection.preferredContentSizeCategory)) {
    CGFloat cellWidth = self.bounds.size.width;
    // Make sure that the multiline labels width isn't changed when the
    // accessory is set.
    CGFloat maxTextWidth = cellWidth - (kTableViewAccessoryWidth +
                                        2 * kTableViewHorizontalSpacing);
    _subtitle.preferredMaxLayoutWidth = maxTextWidth;
    _title.preferredMaxLayoutWidth = maxTextWidth;
  }
  [super layoutSubviews];
}

#pragma mark - Private

// Updates the elements based on a new configuration.
- (void)applyConfiguration {
  BOOL isLeadingImageContentViewCompatible =
      [_leadingContentView
          respondsToSelector:@selector(supportsConfiguration:)] &&
      [_leadingContentView
          supportsConfiguration:_configuration.leadingConfiguration];
  if (!isLeadingImageContentViewCompatible) {
    [_leadingContentView removeFromSuperview];
    _leadingContentView = nil;
  }
  _leadingContentViewContainer.hidden = YES;

  if (_configuration.leadingConfiguration) {
    _leadingContentViewContainer.hidden = NO;
    if (_leadingContentView) {
      _leadingContentView.configuration = _configuration.leadingConfiguration;
    } else {
      _leadingContentView =
          [_configuration.leadingConfiguration makeContentView];
      _leadingContentView.translatesAutoresizingMaskIntoConstraints = NO;
      [_leadingContentViewContainer addSubview:_leadingContentView];
      [NSLayoutConstraint activateConstraints:@[
        [_leadingContentView.leadingAnchor
            constraintEqualToAnchor:_leadingContentViewContainer.leadingAnchor],
        [_leadingContentView.trailingAnchor
            constraintEqualToAnchor:_leadingContentViewContainer
                                        .trailingAnchor],
        [_leadingContentView.centerYAnchor
            constraintEqualToAnchor:_leadingContentViewContainer.centerYAnchor],
        [_leadingContentViewContainer.heightAnchor
            constraintGreaterThanOrEqualToAnchor:_leadingContentView
                                                     .heightAnchor],
      ]];
    }
  }

  _title.hidden = !_configuration.title;
  _title.text = _configuration.title;
  _title.textColor =
      _configuration.titleColor ?: [UIColor colorNamed:kTextPrimaryColor];
  _title.numberOfLines = _configuration.titleNumberOfLines;

  _subtitle.hidden = !_configuration.subtitle;
  _subtitle.text = _configuration.subtitle;
  _subtitle.textColor =
      _configuration.subtitleColor ?: [UIColor colorNamed:kTextSecondaryColor];
  _subtitle.numberOfLines = _configuration.subtitleNumberOfLines;

  _trailingLabel.hidden = !_configuration.trailingText;
  _trailingLabel.text = _configuration.trailingText;
  _trailingLabel.textColor = _configuration.trailingTextColor
                                 ?: [UIColor colorNamed:kTextSecondaryColor];
  _trailingLabel.numberOfLines = _configuration.trailingTextNumberOfLines;
}

// Updates the elements based on a change in the content size.
- (void)updateForContentSizeChange {
  BOOL accessibilityContentSizeCategory =
      UIContentSizeCategoryIsAccessibilityCategory(
          self.traitCollection.preferredContentSizeCategory);
  if (accessibilityContentSizeCategory) {
    _allTextStack.axis = UILayoutConstraintAxisVertical;

    _trailingLabel.textAlignment = NSTextAlignmentNatural;
    _trailingLabel.numberOfLines = 0;
  } else {
    _allTextStack.axis = UILayoutConstraintAxisHorizontal;
    _trailingLabel.textAlignment =
        self.effectiveUserInterfaceLayoutDirection ==
                UIUserInterfaceLayoutDirectionLeftToRight
            ? NSTextAlignmentRight
            : NSTextAlignmentLeft;
    _trailingLabel.numberOfLines = _configuration.trailingTextNumberOfLines;
  }
}

// Setups the views.
- (void)setupViews {
  _leadingContentViewContainer = [[UIView alloc] init];
  _leadingContentViewContainer.translatesAutoresizingMaskIntoConstraints = NO;

  _title = [self createTitleLabel];
  _subtitle = [self createSubtitleLabel];
  _trailingLabel = [self createTrailingLabel];

  _allTextStack = [self createMainTextStack];

  _titleSubtitleContainer = [[TableViewCellContentViewLabelContainer alloc]
      initWithTopLabel:_title
           bottomLabel:_subtitle];
  _titleSubtitleContainer.translatesAutoresizingMaskIntoConstraints = NO;

  // The stack view forces the view to have their leading/trailing anchor equal
  // with a priority of required.
  [_allTextStack addArrangedSubview:_leadingContentViewContainer];
  [_allTextStack addArrangedSubview:_titleSubtitleContainer];
  [_allTextStack addArrangedSubview:_trailingLabel];

  [_allTextStack setCustomSpacing:kTableViewImagePadding
                        afterView:_leadingContentViewContainer];

  [self addSubview:_allTextStack];

  // The constraint ensuring the 75/25 ratio. It is higher priority than the
  // compression resistance but lower than the content hugging.
  NSLayoutConstraint* trailingTextWidthConstraint =
      [_titleSubtitleContainer.widthAnchor
          constraintEqualToAnchor:_trailingLabel.widthAnchor
                       multiplier:kTitleSubtitleToTrailingWidthRatio];
  trailingTextWidthConstraint.priority = UILayoutPriorityDefaultHigh;

  [_titleSubtitleContainer
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [_trailingLabel
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [_titleSubtitleContainer
      setContentHuggingPriority:UILayoutPriorityDefaultHigh + 1
                        forAxis:UILayoutConstraintAxisHorizontal];
  [_trailingLabel setContentHuggingPriority:UILayoutPriorityDefaultHigh + 1
                                    forAxis:UILayoutConstraintAxisHorizontal];

  NSLayoutConstraint* height =
      [self.heightAnchor constraintEqualToConstant:kChromeTableViewCellHeight];
  height.priority = UILayoutPriorityDefaultLow;

  [NSLayoutConstraint activateConstraints:@[
    [self.centerYAnchor constraintEqualToAnchor:_allTextStack.centerYAnchor],
    [self.leadingAnchor constraintEqualToAnchor:_allTextStack.leadingAnchor
                                       constant:-kTableViewHorizontalSpacing],
    [self.trailingAnchor constraintEqualToAnchor:_allTextStack.trailingAnchor
                                        constant:kTableViewHorizontalSpacing],
    [self.heightAnchor
        constraintGreaterThanOrEqualToAnchor:_allTextStack.heightAnchor
                                    constant:
                                        2 *
                                        kTableViewTwoLabelsCellVerticalSpacing],
    [self.heightAnchor
        constraintGreaterThanOrEqualToConstant:kChromeTableViewCellHeight],
    height,
    trailingTextWidthConstraint,
  ]];
}

// Returns a new title label.
- (UILabel*)createTitleLabel {
  UILabel* label = [[UILabel alloc] init];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.numberOfLines = 0;
  label.lineBreakMode = NSLineBreakByWordWrapping;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  label.adjustsFontForContentSizeCategory = YES;
  return label;
}

// Returns a new subtitle label.
- (UILabel*)createSubtitleLabel {
  UILabel* label = [[UILabel alloc] init];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.numberOfLines = 0;
  label.lineBreakMode = NSLineBreakByWordWrapping;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  label.adjustsFontForContentSizeCategory = YES;
  return label;
}

// Returns a new trailing label.
- (UILabel*)createTrailingLabel {
  UILabel* label = [[UILabel alloc] init];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  label.adjustsFontForContentSizeCategory = YES;
  return label;
}

// Returns a new stack view to contain the title/subtitle stack and the trailing
// label.
- (UIStackView*)createMainTextStack {
  UIStackView* stack = [[UIStackView alloc] init];
  stack.alignment = UIStackViewAlignmentCenter;
  stack.translatesAutoresizingMaskIntoConstraints = NO;
  return stack;
}

@end
