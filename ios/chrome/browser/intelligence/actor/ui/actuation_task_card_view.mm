// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actuation_task_card_view.h"

#import "base/check.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_constants.h"
#import "ios/chrome/browser/shared/ui/elements/gradient/gradient_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/chrome_button.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

using intelligence::actor::kSpacingLarge;
using intelligence::actor::kSpacingMedium;
using intelligence::actor::kSpacingSmall;

// Size for header icons (top-left header image and top-right chevron button).
const CGFloat kHeaderIconSize = 16.0;

// Corner radius for the card container.
const CGFloat kCardCornerRadius = 20.0;

// Default animation duration for collapse/expand transitions.
const NSTimeInterval kCollapseAnimationDuration = 0.45;

}  // namespace

@implementation ActuationTaskCardView {
  GradientView* _gradientView;
  UIImageView* _headerImageView;
  UILabel* _collapsedTitleLabel;
  UILabel* _expandedTitleLabel;
  UIView* _titleContainerView;
  UIButton* _chevronButton;
  UILabel* _subtitleLabel;
  ChromeButton* _actionButton;
  UIStackView* _mainStack;

  NSLayoutConstraint* _collapsedHeightConstraint;
  NSLayoutConstraint* _expandedHeightConstraint;
}

#pragma mark - Public

- (instancetype)initWithTitle:(NSString*)title
                  buttonTitle:(NSString*)buttonTitle
                  collapsible:(BOOL)collapsible {
  DCHECK(title);
  DCHECK(buttonTitle);

  self = [super initWithFrame:CGRectZero];
  if (self) {
    _collapsible = collapsible;
    _collapsed = YES;
    [self setupSubviews];
    [self setupConstraints];

    self.title = title;
    self.buttonTitle = buttonTitle;
  }
  return self;
}

#pragma mark - Properties

- (void)setEnabled:(BOOL)enabled {
  _actionButton.enabled = enabled;
}

- (BOOL)isEnabled {
  return _actionButton.isEnabled;
}

- (void)setTitle:(NSString*)title {
  DCHECK(title);
  _collapsedTitleLabel.text = title;
  _expandedTitleLabel.text = title;
  [self setNeedsLayout];
}

- (NSString*)title {
  return _collapsedTitleLabel.text;
}

- (void)setButtonTitle:(NSString*)buttonTitle {
  DCHECK(buttonTitle);
  _actionButton.title = buttonTitle;
}

- (NSString*)buttonTitle {
  return _actionButton.title;
}

- (void)setHeaderIcon:(UIImage*)headerIcon {
  _headerImageView.image = headerIcon;
  _headerImageView.hidden = (headerIcon == nil);
}

- (UIImage*)headerIcon {
  return _headerImageView.image;
}

- (void)setSubtitle:(NSString*)subtitle {
  _subtitleLabel.text = subtitle;
  _subtitleLabel.hidden = (subtitle.length == 0);
}

- (NSString*)subtitle {
  return _subtitleLabel.text;
}

- (void)setCollapsed:(BOOL)collapsed {
  [self setCollapsed:collapsed animated:NO];
}

- (void)setCollapsed:(BOOL)collapsed animated:(BOOL)animated {
  if ((!_collapsible && collapsed) || _collapsed == collapsed) {
    return;
  }
  _collapsed = collapsed;

  if (!animated) {
    [self applyCollapseChanges];
    return;
  }

  __weak __typeof(self) weakSelf = self;
  [UIView animateWithDuration:kCollapseAnimationDuration
                        delay:0.0
       usingSpringWithDamping:1.0
        initialSpringVelocity:0.0
                      options:UIViewAnimationOptionCurveEaseInOut
                   animations:^{
                     [weakSelf applyCollapseChanges];
                   }
                   completion:nil];
}

- (void)applyCollapseChanges {
  _chevronButton.transform = _collapsed ? CGAffineTransformIdentity
                                        : CGAffineTransformMakeRotation(M_PI);

  _collapsedTitleLabel.alpha = _collapsed ? 1.0 : 0.0;
  _expandedTitleLabel.alpha = _collapsed ? 0.0 : 1.0;

  _collapsedHeightConstraint.active = _collapsed;
  _expandedHeightConstraint.active = !_collapsed;

  [self layoutIfNeeded];
}

- (void)setBackgroundGradientStartColor:(UIColor*)startColor
                               endColor:(UIColor*)endColor {
  BOOL hasGradient = startColor && endColor;
  _gradientView.hidden = !hasGradient;
  self.backgroundColor = hasGradient
                             ? [UIColor clearColor]
                             : [UIColor colorNamed:kSecondaryBackgroundColor];
  if (hasGradient) {
    [_gradientView setStartColor:startColor endColor:endColor];
  }
}

#pragma mark - UIView

- (void)layoutSubviews {
  [super layoutSubviews];
  [self updateCollapsibleStateIfNeeded];
}

#pragma mark - Actions

// Triggers the collapse/expand animation and notifies the delegate.
- (void)didTapChevronButton {
  [self setCollapsed:!_collapsed animated:YES];
  [self.delegate taskCardView:self didChangeCollapsedState:_collapsed];
}

// Dispatches action to the delegate.
- (void)didTapActionButton {
  [self.delegate taskCardViewDidTapActionButton:self];
}

#pragma mark - Private

// Helper to create title labels with standardized properties.
- (UILabel*)createTitleLabelWithNumberOfLines:(NSInteger)numberOfLines {
  UILabel* label = [[UILabel alloc] init];
  label.numberOfLines = numberOfLines;
  label.font = CreateDynamicFont(UIFontTextStyleSubheadline, UIFontWeightBold);
  label.textColor = [UIColor colorNamed:kTextPrimaryColor];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  return label;
}

// Synchronizes top-right chevron button visibility with title text truncation.
- (void)updateCollapsibleStateIfNeeded {
  if (!_collapsible) {
    _chevronButton.hidden = YES;
    return;
  }
  CGFloat containerWidth = _titleContainerView.bounds.size.width;
  if (containerWidth <= 0) {
    return;
  }

  CGSize targetSize = CGSizeMake(containerWidth, CGFLOAT_MAX);
  CGFloat expandedHeight = [_expandedTitleLabel sizeThatFits:targetSize].height;
  CGFloat collapsedHeight =
      [_collapsedTitleLabel sizeThatFits:targetSize].height;

  BOOL isTruncated = expandedHeight > (collapsedHeight + 0.5);
  if (_chevronButton.hidden == isTruncated) {
    _chevronButton.hidden = !isTruncated;
  }
}

// Instantiates and adds all subviews and layout stack hierarchies.
- (void)setupSubviews {
  self.layer.cornerRadius = kCardCornerRadius;
  self.clipsToBounds = YES;
  self.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];

  _gradientView =
      [[GradientView alloc] initWithStartColor:[UIColor clearColor]
                                      endColor:[UIColor clearColor]
                                    startPoint:CGPointZero
                                      endPoint:CGPointMake(1.0, 1.0)
                                  gradientType:GradientLayerType::kLinear];
  _gradientView.translatesAutoresizingMaskIntoConstraints = NO;
  _gradientView.userInteractionEnabled = NO;
  _gradientView.hidden = YES;
  [self addSubview:_gradientView];

  _headerImageView = [[UIImageView alloc] init];
  _headerImageView.contentMode = UIViewContentModeScaleAspectFit;
  _headerImageView.translatesAutoresizingMaskIntoConstraints = NO;
  _headerImageView.hidden = YES;

  _collapsedTitleLabel = [self createTitleLabelWithNumberOfLines:3];
  _expandedTitleLabel = [self createTitleLabelWithNumberOfLines:0];
  _expandedTitleLabel.alpha = 0.0;

  _titleContainerView = [[UIView alloc] init];
  _titleContainerView.clipsToBounds = YES;
  _titleContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  [_titleContainerView addSubview:_collapsedTitleLabel];
  [_titleContainerView addSubview:_expandedTitleLabel];

  _chevronButton = [UIButton buttonWithType:UIButtonTypeSystem];
  UIImage* chevronImage =
      SymbolTemplateWithPointSize(SymbolChevronDown, kHeaderIconSize);
  [_chevronButton setImage:chevronImage forState:UIControlStateNormal];
  _chevronButton.tintColor = [UIColor colorNamed:kTextSecondaryColor];
  _chevronButton.translatesAutoresizingMaskIntoConstraints = NO;
  _chevronButton.hidden = YES;
  [_chevronButton addTarget:self
                     action:@selector(didTapChevronButton)
           forControlEvents:UIControlEventTouchUpInside];

  UIStackView* headerStack = [[UIStackView alloc] initWithArrangedSubviews:@[
    _headerImageView, _titleContainerView, _chevronButton
  ]];
  headerStack.alignment = UIStackViewAlignmentTop;
  headerStack.spacing = kSpacingSmall;

  _subtitleLabel = [[UILabel alloc] init];
  _subtitleLabel.numberOfLines = 0;
  _subtitleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  _subtitleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  _subtitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _subtitleLabel.hidden = YES;

  _actionButton = [[ChromeButton alloc] initWithStyle:ChromeButtonStylePrimary];
  _actionButton.translatesAutoresizingMaskIntoConstraints = NO;
  [_actionButton addTarget:self
                    action:@selector(didTapActionButton)
          forControlEvents:UIControlEventTouchUpInside];

  _mainStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ headerStack, _subtitleLabel, _actionButton ]];
  _mainStack.axis = UILayoutConstraintAxisVertical;
  _mainStack.spacing = kSpacingMedium;
  _mainStack.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:_mainStack];
}

// Configures and activates layout constraints across subviews.
- (void)setupConstraints {
  AddSameConstraints(_gradientView, self);

  AddSameConstraintsWithInsets(
      _mainStack, self,
      NSDirectionalEdgeInsetsMake(kSpacingLarge, kSpacingLarge, kSpacingLarge,
                                  kSpacingLarge));

  LayoutSides sides =
      LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kTrailing;
  AddSameConstraintsToSides(_collapsedTitleLabel, _titleContainerView, sides);
  AddSameConstraintsToSides(_expandedTitleLabel, _titleContainerView, sides);

  _collapsedHeightConstraint = [_titleContainerView.bottomAnchor
      constraintEqualToAnchor:_collapsedTitleLabel.bottomAnchor];
  _collapsedHeightConstraint.active = _collapsed;
  _expandedHeightConstraint = [_titleContainerView.bottomAnchor
      constraintEqualToAnchor:_expandedTitleLabel.bottomAnchor];
  _expandedHeightConstraint.active = !_collapsed;

  AddSquareConstraints(_headerImageView, kHeaderIconSize);
  AddSquareConstraints(_chevronButton, kHeaderIconSize);
}

@end
