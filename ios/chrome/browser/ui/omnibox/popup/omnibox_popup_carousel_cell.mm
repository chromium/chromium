// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_carousel_cell.h"

#import "base/check.h"
#import "base/i18n/rtl.h"
#import "base/notreached.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"
#import "ios/chrome/browser/ui/omnibox/popup/carousel_item.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_accessibility_identifier_constants.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_carousel_control.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ui/base/device_form_factor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

/// Maximum number of item in the Carousel.
const NSUInteger kCarouselCapacity = 10;
/// Margin of the StackView.
const CGFloat kStackMargin = 8.0f;
/// Minimum spacing between items in the StackView.
const CGFloat kMinStackSpacing = 8.0f;
/// Width of the gradient applied on the leading and trailing edge of the
/// carousel.
const CGFloat kGradientWidth = 20.0f;

/// Horizontal UIScrollView used in OmniboxPopupCarouselCell.
UIScrollView* CarouselScrollView() {
  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  scrollView.showsVerticalScrollIndicator = NO;
  scrollView.showsHorizontalScrollIndicator = NO;
  scrollView.backgroundColor =
      [UIColor colorNamed:kGroupedSecondaryBackgroundColor];
  return scrollView;
}

/// Horizontal UIStackView used in OmniboxPopupCarouselCell.
UIStackView* CarouselStackView() {
  UIStackView* stackView = [[UIStackView alloc] init];
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.axis = UILayoutConstraintAxisHorizontal;
  stackView.alignment = UIStackViewAlignmentTop;
  stackView.distribution = UIStackViewDistributionEqualSpacing;
  stackView.spacing = kMinStackSpacing;
  stackView.backgroundColor =
      [UIColor colorNamed:kGroupedSecondaryBackgroundColor];
  return stackView;
}

/// CAGradientLayer used in OmniboxPopupCarouselCell on iPad.
CAGradientLayer* CarouselGradientLayer() {
  CAGradientLayer* maskLayer = [CAGradientLayer layer];
  UIColor* opaqueColor = [[UIColor colorNamed:kGroupedSecondaryBackgroundColor]
      colorWithAlphaComponent:1.0];
  UIColor* transparentColor = [[UIColor
      colorNamed:kGroupedSecondaryBackgroundColor] colorWithAlphaComponent:0.0];
  maskLayer.colors = @[
    (id)transparentColor.CGColor, (id)opaqueColor.CGColor,
    (id)opaqueColor.CGColor, (id)transparentColor.CGColor
  ];
  // locations are computed with `kGradientWidth` in `updateGradient`.
  maskLayer.anchorPoint = CGPointZero;
  maskLayer.startPoint = CGPointMake(0, 0.5);
  maskLayer.endPoint = CGPointMake(1, 0.5);
  return maskLayer;
}

}  // namespace

@interface OmniboxPopupCarouselCell ()

/// Horizontal UIScrollView for the Carousel.
@property(nonatomic, strong) UIScrollView* scrollView;
/// Horizontal UIStackView containing CarouselItems.
@property(nonatomic, strong) UIStackView* suggestionsStackView;
/// Indicates whether the view's marginLayoutGuide should be used.
@property(nonatomic, assign, readonly) BOOL shouldApplyLayoutMarginsGuide;
/// GradientLayer applied at the right edge of `scrollView`.
@property(nonatomic, strong) CAGradientLayer* gradientLayer;

#pragma mark Dynamic Spacing
/// Number of that that can be fully visible. Apply dynamic spacing only when
/// the number of tiles exceeds `visibleTilesCapacity`.
@property(nonatomic, assign) NSInteger visibleTilesCapacity;
/// Spacing between tiles to have half a tile visible on the trailing edge,
/// indicating a scrollable view.
@property(nonatomic, assign) CGFloat dynamicSpacing;

@end

@implementation OmniboxPopupCarouselCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    _scrollView = CarouselScrollView();
    _suggestionsStackView = CarouselStackView();
    _gradientLayer = CarouselGradientLayer();
    self.isAccessibilityElement = NO;
    self.contentView.isAccessibilityElement = NO;
    self.backgroundColor =
        [UIColor colorNamed:kGroupedSecondaryBackgroundColor];
    self.accessibilityIdentifier = kOmniboxCarouselCellAccessibilityIdentifier;
  }
  return self;
}

- (void)didMoveToWindow {
  if (self.window) {
    // Reset scroll to the left edge.
    CGRect scrollViewLeft = CGRectMake(0, 0, 1, 1);
    [self.scrollView scrollRectToVisible:scrollViewLeft animated:NO];
  }
}

- (void)layoutSubviews {
  if (self.shouldApplyLayoutMarginsGuide) {
    [self updateGradient];
  }
  if (base::i18n::IsRTL()) {
    self.scrollView.transform = CGAffineTransformMakeRotation(M_PI);
    self.suggestionsStackView.transform = CGAffineTransformMakeRotation(M_PI);
  }
  [self updateDynamicSpacing];
  [super layoutSubviews];
}

- (void)addContentSubviews {
  [self.contentView addSubview:_scrollView];
  [_scrollView addSubview:_suggestionsStackView];

  // When applying margins guide, add gradient at the right edge to indicate a
  // scrollable view.
  if (self.shouldApplyLayoutMarginsGuide) {
    self.contentView.layer.mask = self.gradientLayer;
  }

  AddSameConstraintsWithInsets(
      _suggestionsStackView, _scrollView,
      NSDirectionalEdgeInsetsMake(kStackMargin, kStackMargin, kStackMargin,
                                  kStackMargin));

  id<LayoutGuideProvider> contentGuide =
      self.shouldApplyLayoutMarginsGuide ? self.contentView.layoutMarginsGuide
                                         : self.contentView;
  AddSameCenterConstraints(_scrollView, contentGuide);
  [NSLayoutConstraint activateConstraints:@[
    [contentGuide.heightAnchor
        constraintEqualToAnchor:_scrollView.heightAnchor],
    [contentGuide.widthAnchor constraintEqualToAnchor:_scrollView.widthAnchor],
    [_scrollView.heightAnchor
        constraintEqualToAnchor:_suggestionsStackView.heightAnchor
                       constant:kStackMargin * 2]
  ]];
}

#pragma mark - properties

- (NSUInteger)tileCount {
  return self.suggestionsStackView.arrangedSubviews.count;
}

- (BOOL)shouldApplyLayoutMarginsGuide {
  // Apply layoutMarginsGuide in Visual Treatment 1 only on Tablet because there
  // is a minimum layoutMargin of 8 that we don't want on phones.
  return ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET;
}

#pragma mark - Accessibility

- (NSArray*)accessibilityElements {
  return self.suggestionsStackView.arrangedSubviews;
}

#pragma mark - Public methods

- (void)setupWithCarouselItems:(NSArray<CarouselItem*>*)carouselItems {
  DCHECK(carouselItems.count <= kCarouselCapacity);

  if (self.contentView.subviews.count == 0) {
    [self addContentSubviews];
  }

  // Remove all previous items from carousel.
  while (self.suggestionsStackView.arrangedSubviews.count != 0) {
    [self.suggestionsStackView.arrangedSubviews
            .firstObject removeFromSuperview];
  }

  for (CarouselItem* item in carouselItems) {
    OmniboxPopupCarouselControl* control = [self newCarouselControl];
    [self.suggestionsStackView addArrangedSubview:control];
    [control setCarouselItem:item];
  }

  if (static_cast<NSInteger>(carouselItems.count) > self.visibleTilesCapacity) {
    self.suggestionsStackView.spacing = self.dynamicSpacing;
  } else {
    self.suggestionsStackView.spacing = kMinStackSpacing;
  }
}

- (void)updateCarouselItem:(CarouselItem*)carouselItem {
  OmniboxPopupCarouselControl* control =
      [self controlForCarouselItem:carouselItem];
  if (!control) {
    return;
  }
  [control setCarouselItem:carouselItem];
}

- (NSInteger)highlightedTileIndex {
  for (OmniboxPopupCarouselControl* control in self.suggestionsStackView
           .arrangedSubviews) {
    if (control.selected) {
      return [self.suggestionsStackView.arrangedSubviews indexOfObject:control];
    }
  }

  return NSNotFound;
}

#pragma mark - CarouselItemConsumer

- (void)deleteCarouselItem:(CarouselItem*)carouselItem {
  OmniboxPopupCarouselControl* control =
      [self controlForCarouselItem:carouselItem];
  if (!control) {
    return;
  }
  [control removeFromSuperview];
  // Remove voice over focus to avoid focusing an invisible tile. Focus instead
  // on the first tile.
  if (self.suggestionsStackView.arrangedSubviews.firstObject) {
    UIAccessibilityPostNotification(
        UIAccessibilityScreenChangedNotification,
        self.suggestionsStackView.arrangedSubviews.firstObject);
  }
  [self.delegate carouselCellDidChangeItemCount:self];
}

#pragma mark - UITableViewCell

- (BOOL)isHighlighted {
  return self.highlightedTileIndex != NSNotFound;
}

- (void)setHighlighted:(BOOL)highlighted animated:(BOOL)animated {
  if (animated) {
    [UIView animateWithDuration:0.2
                     animations:^{
                       [self setHighlighted:highlighted];
                     }];
  } else {
    [self setHighlighted:highlighted];
  }
}

- (void)setHighlighted:(BOOL)highlighted {
  if (self.isHighlighted == highlighted) {
    return;
  }

  if (highlighted) {
    [self highlightFirstTile];
  } else {
    for (OmniboxPopupCarouselControl* control in self.suggestionsStackView
             .arrangedSubviews) {
      control.selected = NO;
    }
  }
}

#pragma mark - OmniboxKeyboardDelegate

- (BOOL)canPerformKeyboardAction:(OmniboxKeyboardAction)keyboardAction {
  switch (keyboardAction) {
    case OmniboxKeyboardActionUpArrow:
      return NO;
    case OmniboxKeyboardActionDownArrow:
      return NO;
    case OmniboxKeyboardActionLeftArrow:
      return self.isHighlighted;
    case OmniboxKeyboardActionRightArrow:
      return self.isHighlighted;
  }
  return NO;
}

- (void)highlightFirstTile {
  NSArray<OmniboxPopupCarouselControl*>* allTiles =
      self.suggestionsStackView.arrangedSubviews;
  allTiles.firstObject.selected = YES;
}

- (void)performKeyboardAction:(OmniboxKeyboardAction)keyboardAction {
  // Find and unhighlight the previously highlighted suggestion.
  NSInteger prevHighlightedIndex = self.highlightedTileIndex;

  if (prevHighlightedIndex == NSNotFound) {
    [self highlightFirstTile];
    return;
  }

  NSInteger nextHighlightedIndex = self.highlightedTileIndex;
  NSArray<OmniboxPopupCarouselControl*>* allTiles =
      self.suggestionsStackView.arrangedSubviews;

  OmniboxKeyboardAction nextTileAction = base::i18n::IsRTL()
                                             ? OmniboxKeyboardActionLeftArrow
                                             : OmniboxKeyboardActionRightArrow;
  OmniboxKeyboardAction previousTileAction =
      base::i18n::IsRTL() ? OmniboxKeyboardActionRightArrow
                          : OmniboxKeyboardActionLeftArrow;

  if (keyboardAction == nextTileAction) {
    nextHighlightedIndex =
        MIN(prevHighlightedIndex + 1, (NSInteger)allTiles.count - 1);
  } else if (keyboardAction == previousTileAction) {
    nextHighlightedIndex = MAX(prevHighlightedIndex - 1, 0);
  } else {
    NOTREACHED();
  }
  allTiles[prevHighlightedIndex].selected = NO;
  allTiles[nextHighlightedIndex].selected = YES;
}

#pragma mark - OmniboxPopupCarouselControlDelegate

- (void)carouselControlDidBecomeFocused:(OmniboxPopupCarouselControl*)control {
  CGRect frameInScrollViewCoordinates = [control convertRect:control.bounds
                                                      toView:self.scrollView];
  CGRect frameWithPadding =
      CGRectInset(frameInScrollViewCoordinates, -kMinStackSpacing * 2, 0);
  [self.scrollView scrollRectToVisible:frameWithPadding animated:NO];
}

#pragma mark - Private methods

- (void)didTapCarouselControl:(OmniboxPopupCarouselControl*)control {
  DCHECK(control.carouselItem);
  [self.delegate carouselCell:self didTapCarouselItem:control.carouselItem];
}

// Returns OmniboxPopupCarouselControl containing `carouselItem`.
- (OmniboxPopupCarouselControl*)controlForCarouselItem:
    (CarouselItem*)carouselItem {
  for (OmniboxPopupCarouselControl* control in self.suggestionsStackView
           .arrangedSubviews) {
    if (control.carouselItem == carouselItem) {
      return control;
    }
  }
  return nil;
}

// Updates `dynamicSpacing` and `visibleTilesCapacity` for carousel dynamic
// spacing.
- (void)updateDynamicSpacing {
  CGFloat availableWidth = self.bounds.size.width - 2 * kStackMargin;
  if (self.shouldApplyLayoutMarginsGuide) {
    availableWidth -= self.contentView.layoutMargins.left +
                      self.contentView.layoutMargins.right + kGradientWidth;
  }
  CGFloat tileWidth = kOmniboxPopupCarouselControlWidth + kMinStackSpacing / 2;

  CGFloat maxVisibleTiles = availableWidth / tileWidth;
  CGFloat nearestHalfTile = maxVisibleTiles - 0.5;
  CGFloat nbFullTiles = floor(nearestHalfTile);
  CGFloat percentageOfTileToFill = nearestHalfTile - nbFullTiles;
  CGFloat extraSpaceToFill = percentageOfTileToFill * tileWidth;
  if (nbFullTiles < FLT_EPSILON) {
    return;
  }
  CGFloat extraSpacingPerTile = extraSpaceToFill / nbFullTiles;

  self.dynamicSpacing = extraSpacingPerTile + kMinStackSpacing;
  self.visibleTilesCapacity = nbFullTiles;

  if (static_cast<NSInteger>(self.tileCount) > self.visibleTilesCapacity) {
    self.suggestionsStackView.spacing = self.dynamicSpacing;
  } else {
    self.suggestionsStackView.spacing = kMinStackSpacing;
  }
}

- (OmniboxPopupCarouselControl*)newCarouselControl {
  OmniboxPopupCarouselControl* control =
      [[OmniboxPopupCarouselControl alloc] init];
  control.delegate = self;
  [control addTarget:self
                action:@selector(didTapCarouselControl:)
      forControlEvents:UIControlEventTouchUpInside];
  control.isAccessibilityElement = YES;
  control.menuProvider = self.menuProvider;

  return control;
}

- (void)updateGradient {
  DCHECK(self.shouldApplyLayoutMarginsGuide);
  CGFloat contentWidth =
      CGRectGetWidth(self.contentView.layoutMarginsGuide.layoutFrame);
  CGRect gradientFrame = CGRectMake(self.contentView.layoutMargins.left, 0,
                                    contentWidth, CGRectGetHeight(self.bounds));
  CGFloat gradientWidth = kGradientWidth / contentWidth;
  self.gradientLayer.frame = gradientFrame;
  NSNumber* endOfFirstGradient = [NSNumber numberWithFloat:gradientWidth];
  NSNumber* startOfSecondGradient =
      [NSNumber numberWithFloat:1.0 - gradientWidth];
  self.gradientLayer.locations =
      @[ @0.0, endOfFirstGradient, startOfSecondGradient, @1.0 ];
}

@end
