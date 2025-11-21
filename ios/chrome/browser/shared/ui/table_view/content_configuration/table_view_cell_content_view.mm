// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_view.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/chrome_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Constant to achieve the 75/25 ratio between the title/subtitle (75) and the
// trailing label (25).
constexpr CGFloat kTitleSubtitleToTrailingWidthRatio = 3;

constexpr CGFloat kLabelVerticalSpacing = 5;

// The margin for the trailing edge of the content view, when there is an
// accessory view in the cell.
constexpr CGFloat kTrailingMarginWithAccessory = 8;

}  // namespace

// Container for the title and subtitle labels. This container's intrinsic width
// is calculated based on the widest of the two labels to ensure proper layout
// and alignment within the cell, especially for the 75/25 width constraint with
// the trailing label.
@interface TableViewCellContentViewLabelContainer : UIStackView
@end

@implementation TableViewCellContentViewLabelContainer {
  UILabel* _topLabel;
  UILabel* _middleLabel;
  UILabel* _bottomLabel;
}

// Inits the container with 3 labels, one at the top, one in the middle and one
// at the bottom.
- (instancetype)initWithTopLabel:(UILabel*)topLabel
                     middleLabel:(UILabel*)middleLabel
                     bottomLabel:(UILabel*)bottomLabel {
  self = [super init];
  if (self) {
    _topLabel = topLabel;
    _middleLabel = middleLabel;
    _bottomLabel = bottomLabel;

    self.axis = UILayoutConstraintAxisVertical;
    self.distribution = UIStackViewDistributionFill;
    self.spacing = kLabelVerticalSpacing;

    [self addArrangedSubview:_topLabel];
    [self addArrangedSubview:_middleLabel];
    [self addArrangedSubview:_bottomLabel];
  }
  return self;
}

- (CGSize)intrinsicContentSize {
  CGFloat topWidth = [self maxWidthForLabel:_topLabel];
  CGFloat middleWidth = [self maxWidthForLabel:_middleLabel];
  CGFloat bottomWidth = [self maxWidthForLabel:_bottomLabel];

  CGFloat maxWidth = MAX(MAX(topWidth, middleWidth), bottomWidth);

  return CGSizeMake(maxWidth, UIViewNoIntrinsicMetric);
}

#pragma mark - Container Private

// Returns the max width for a `label`.
- (CGFloat)maxWidthForLabel:(UILabel*)label {
  // Make sure to have a number of line of 1 for the label when getting its
  // intrinsic size. Otherwise, it will choose to use several lines and have a
  // narrower width.
  CGFloat numberOfLines = label.numberOfLines;
  label.numberOfLines = 1;
  CGFloat maxWidth = [label intrinsicContentSize].width;
  label.numberOfLines = numberOfLines;
  return maxWidth;
}

@end

#pragma mark - TableViewCellContentView

@implementation TableViewCellContentView {
  TableViewCellContentConfiguration* _configuration;

  // The leading content view.
  UIView<ChromeContentView>* _leadingContentView;
  // The trailing content view.
  UIView<ChromeContentView>* _trailingContentView;
  // The container for the leading content view.
  UIView* _leadingContentViewContainer;
  // The container for the trailing content view.
  UIView* _trailingContentViewContainer;

  // The labels.
  UILabel* _title;
  UILabel* _subtitle;
  UILabel* _secondSubtitle;
  UILabel* _trailingLabel;

  // The container for the text.
  UIView* _titleSubtitleContainer;
  UIStackView* _allTextStack;
  // Constraints to be applied on the text labels when using accessibility font
  // size.
  NSArray<NSLayoutConstraint*>* _accessibilityTextConstraints;

  // The main container.
  UIStackView* _mainStack;

  // The constraint for the trailing edge of the main stack.
  NSLayoutConstraint* _mainStackTrailingConstraint;
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

- (UIView*)trailingContentViewForTesting {
  return _trailingContentView;
}

#pragma mark - ChromeContentView

- (BOOL)hasCustomAccessibilityActivationPoint {
  return [_leadingContentView hasCustomAccessibilityActivationPoint] ||
         [_trailingContentView hasCustomAccessibilityActivationPoint];
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

#pragma mark - Private

// Updates the elements based on a new configuration.
- (void)applyConfiguration {
  _mainStackTrailingConstraint.constant = _configuration.hasAccessoryView
                                              ? kTrailingMarginWithAccessory
                                              : kTableViewHorizontalSpacing;

  id<ChromeContentConfiguration> leadingConfiguration =
      _configuration.leadingConfiguration;
  BOOL isLeadingImageContentViewCompatible =
      [_leadingContentView
          respondsToSelector:@selector(supportsConfiguration:)] &&
      [_leadingContentView supportsConfiguration:leadingConfiguration];
  if (!isLeadingImageContentViewCompatible) {
    [_leadingContentView removeFromSuperview];
    _leadingContentView = nil;
  }
  _leadingContentViewContainer.hidden = YES;

  if (leadingConfiguration) {
    _leadingContentViewContainer.hidden = NO;
    if (_leadingContentView) {
      _leadingContentView.configuration = leadingConfiguration;
    } else {
      _leadingContentView = [leadingConfiguration makeChromeContentView];
      _leadingContentView.translatesAutoresizingMaskIntoConstraints = NO;
      [_leadingContentViewContainer addSubview:_leadingContentView];
      AddSameConstraints(_leadingContentView, _leadingContentViewContainer);
    }
  }

  id<ChromeContentConfiguration> trailingConfiguration =
      _configuration.trailingConfiguration;
  BOOL isTrailingImageContentViewCompatible =
      [_trailingContentView
          respondsToSelector:@selector(supportsConfiguration:)] &&
      [_trailingContentView supportsConfiguration:trailingConfiguration];
  if (!isTrailingImageContentViewCompatible) {
    [_trailingContentView removeFromSuperview];
    _trailingContentView = nil;
  }
  _trailingContentViewContainer.hidden = YES;

  if (trailingConfiguration) {
    _trailingContentViewContainer.hidden = NO;
    if (_trailingContentView) {
      _trailingContentView.configuration = trailingConfiguration;
    } else {
      _trailingContentView = [trailingConfiguration makeChromeContentView];
      _trailingContentView.translatesAutoresizingMaskIntoConstraints = NO;
      [_trailingContentViewContainer addSubview:_trailingContentView];
      AddSameConstraints(_trailingContentView, _trailingContentViewContainer);
    }
  }

  _title.text = _configuration.title;
  _title.textColor =
      _configuration.titleColor ?: [UIColor colorNamed:kTextPrimaryColor];
  if (_configuration.attributedTitle) {
    _title.attributedText = _configuration.attributedTitle;
  }
  _title.hidden = !_title.text;
  _title.enabled = !_configuration.textDisabled;
  _title.lineBreakMode = _configuration.titleLineBreakMode;

  _subtitle.text = _configuration.subtitle;
  _subtitle.textColor =
      _configuration.subtitleColor ?: [UIColor colorNamed:kTextSecondaryColor];
  if (_configuration.attributedSubtitle) {
    _subtitle.attributedText = _configuration.attributedSubtitle;
  }
  _subtitle.hidden = !_subtitle.text;
  _subtitle.enabled = !_configuration.textDisabled;
  _subtitle.lineBreakMode = _configuration.subtitleLineBreakMode;

  _secondSubtitle.text = _configuration.secondSubtitle;
  _secondSubtitle.hidden = !_secondSubtitle.text;
  _secondSubtitle.textColor = [UIColor colorNamed:kTextSecondaryColor];

  _trailingLabel.text = _configuration.trailingText;
  _trailingLabel.textColor = _configuration.trailingTextColor
                                 ?: [UIColor colorNamed:kTextSecondaryColor];
  if (_configuration.attributedTrailingText) {
    _trailingLabel.attributedText = _configuration.attributedTrailingText;
  }
  _trailingLabel.hidden = !_trailingLabel.text;
  _trailingLabel.enabled = !_configuration.textDisabled;

  [self updateNumberOfLines];

  [_titleSubtitleContainer invalidateIntrinsicContentSize];
}

// Updates the number of lines of the labels based on the accessibility and the
// config.
- (void)updateNumberOfLines {
  BOOL accessibilityContentSizeCategory =
      UIContentSizeCategoryIsAccessibilityCategory(
          self.traitCollection.preferredContentSizeCategory);

  _title.numberOfLines =
      accessibilityContentSizeCategory ? 0 : _configuration.titleNumberOfLines;
  _subtitle.numberOfLines = accessibilityContentSizeCategory
                                ? 0
                                : _configuration.subtitleNumberOfLines;
  _secondSubtitle.numberOfLines =
      accessibilityContentSizeCategory
          ? 0
          : _configuration.secondSubtitleNumberOfLines;
  _trailingLabel.numberOfLines = accessibilityContentSizeCategory
                                     ? 0
                                     : _configuration.trailingTextNumberOfLines;
}

// Updates the elements based on a change in the content size.
- (void)updateForContentSizeChange {
  BOOL accessibilityContentSizeCategory =
      UIContentSizeCategoryIsAccessibilityCategory(
          self.traitCollection.preferredContentSizeCategory);
  if (accessibilityContentSizeCategory) {
    _allTextStack.axis = UILayoutConstraintAxisVertical;
    _allTextStack.alignment = UIStackViewAlignmentLeading;
    _mainStack.axis = UILayoutConstraintAxisVertical;
    _mainStack.alignment = UIStackViewAlignmentLeading;

    _trailingLabel.textAlignment = NSTextAlignmentNatural;
    [NSLayoutConstraint activateConstraints:_accessibilityTextConstraints];
  } else {
    _allTextStack.axis = UILayoutConstraintAxisHorizontal;
    _allTextStack.alignment = UIStackViewAlignmentCenter;
    _mainStack.axis = UILayoutConstraintAxisHorizontal;
    _mainStack.alignment = UIStackViewAlignmentCenter;

    _trailingLabel.textAlignment =
        self.effectiveUserInterfaceLayoutDirection ==
                UIUserInterfaceLayoutDirectionLeftToRight
            ? NSTextAlignmentRight
            : NSTextAlignmentLeft;
    [NSLayoutConstraint deactivateConstraints:_accessibilityTextConstraints];
  }

  [self updateNumberOfLines];
}

// Setups the views.
- (void)setupViews {
  _leadingContentViewContainer = [[UIView alloc] init];
  _leadingContentViewContainer.translatesAutoresizingMaskIntoConstraints = NO;

  _trailingContentViewContainer = [[UIView alloc] init];
  _trailingContentViewContainer.translatesAutoresizingMaskIntoConstraints = NO;

  _title = [self createTitleLabel];
  _subtitle = [self createSubtitleLabel];
  _secondSubtitle = [self createSubtitleLabel];
  _trailingLabel = [self createTrailingLabel];

  _allTextStack = [self createAllTextStack];

  _mainStack = [self createMainStack];

  _titleSubtitleContainer = [[TableViewCellContentViewLabelContainer alloc]
      initWithTopLabel:_title
           middleLabel:_subtitle
           bottomLabel:_secondSubtitle];
  _titleSubtitleContainer.translatesAutoresizingMaskIntoConstraints = NO;

  // The stack view forces the view to have their leading/trailing anchor equal
  // with a priority of required.
  [_allTextStack addArrangedSubview:_titleSubtitleContainer];
  [_allTextStack addArrangedSubview:_trailingLabel];

  [_mainStack addArrangedSubview:_leadingContentViewContainer];
  [_mainStack addArrangedSubview:_allTextStack];
  [_mainStack addArrangedSubview:_trailingContentViewContainer];

  [_mainStack setCustomSpacing:kTableViewImagePadding
                     afterView:_leadingContentViewContainer];

  [self addSubview:_mainStack];

  _accessibilityTextConstraints = @[
    [_title.widthAnchor constraintEqualToAnchor:_allTextStack.widthAnchor],
    [_subtitle.widthAnchor constraintEqualToAnchor:_allTextStack.widthAnchor],
    [_trailingLabel.widthAnchor
        constraintEqualToAnchor:_allTextStack.widthAnchor],
    [_allTextStack.widthAnchor constraintEqualToAnchor:_mainStack.widthAnchor],
  ];

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

  _mainStackTrailingConstraint =
      [self.trailingAnchor constraintEqualToAnchor:_mainStack.trailingAnchor];

  [NSLayoutConstraint activateConstraints:@[
    [self.centerYAnchor constraintEqualToAnchor:_mainStack.centerYAnchor],
    [self.leadingAnchor constraintEqualToAnchor:_mainStack.leadingAnchor
                                       constant:-kTableViewHorizontalSpacing],
    _mainStackTrailingConstraint,
    [self.heightAnchor
        constraintGreaterThanOrEqualToAnchor:_mainStack.heightAnchor
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
- (UIStackView*)createAllTextStack {
  UIStackView* stack = [[UIStackView alloc] init];
  stack.translatesAutoresizingMaskIntoConstraints = NO;
  return stack;
}

// Returns a new stack view to contain all the elements.
- (UIStackView*)createMainStack {
  UIStackView* stack = [[UIStackView alloc] init];
  stack.alignment = UIStackViewAlignmentCenter;
  stack.spacing = kTableViewHorizontalSpacing;
  stack.translatesAutoresizingMaskIntoConstraints = NO;
  return stack;
}

#pragma mark - UIAccessibility

- (CGPoint)accessibilityActivationPoint {
  if ([_trailingContentView hasCustomAccessibilityActivationPoint]) {
    return _trailingContentView.accessibilityActivationPoint;
  }
  if ([_leadingContentView hasCustomAccessibilityActivationPoint]) {
    return _leadingContentView.accessibilityActivationPoint;
  }
  return [super accessibilityActivationPoint];
}

@end
