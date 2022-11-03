// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_carousel_cell.h"

#import "base/check.h"
#import "base/notreached.h"
#import "ios/chrome/browser/ui/omnibox/popup/carousel_item.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_carousel_control.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Maximum number of item in the Carousel.
const NSUInteger kCarouselCapacity = 10;
// Margin of the StackView.
const CGFloat kStackMargin = 8.0f;
// Space between items in the StackView.
const CGFloat kStackSpacing = 6.0f;

// Horizontal UIScrollView used in OmniboxPopupCarouselCell.
UIScrollView* CarouselScrollView() {
  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  scrollView.showsVerticalScrollIndicator = NO;
  scrollView.showsHorizontalScrollIndicator = YES;
  scrollView.backgroundColor =
      [UIColor colorNamed:kGroupedSecondaryBackgroundColor];
  return scrollView;
}

// Horizontal UIStackView used in OmniboxPopupCarouselCell.
UIStackView* CarouselStackView() {
  UIStackView* stackView = [[UIStackView alloc] init];
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.axis = UILayoutConstraintAxisHorizontal;
  stackView.alignment = UIStackViewAlignmentTop;
  stackView.distribution = UIStackViewDistributionEqualSpacing;
  stackView.spacing = kStackSpacing;
  stackView.backgroundColor =
      [UIColor colorNamed:kGroupedSecondaryBackgroundColor];
  return stackView;
}

}  // namespace

@interface OmniboxPopupCarouselCell ()

// Horizontal UIScrollView for the Carousel.
@property(nonatomic, strong) UIScrollView* scrollView;
// Horizontal UIStackView containing CarouselItems.
@property(nonatomic, strong) UIStackView* suggestionsStackView;
// The subset of controls that correspond to items that aren't hidden.
@property(nonatomic, strong, readonly)
    NSArray<OmniboxPopupCarouselControl*>* visibleControls;

@end

@implementation OmniboxPopupCarouselCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    _scrollView = CarouselScrollView();
    _suggestionsStackView = CarouselStackView();
    self.isAccessibilityElement = NO;
    self.contentView.isAccessibilityElement = NO;
    for (NSUInteger i = 0; i < kCarouselCapacity; ++i) {
      OmniboxPopupCarouselControl* control =
          [[OmniboxPopupCarouselControl alloc] init];
      [_suggestionsStackView addArrangedSubview:control];
      control.delegate = self;
      [control addTarget:self
                    action:@selector(didTapCarouselControl:)
          forControlEvents:UIControlEventTouchUpInside];
      control.hidden = YES;
      control.isAccessibilityElement = YES;
    }
  }
  return self;
}

- (void)didMoveToWindow {
  if (self.window) {
    [self.scrollView flashScrollIndicators];
  }
}

- (void)addContentSubviews {
  [self.contentView addSubview:_scrollView];
  [_scrollView addSubview:_suggestionsStackView];

  AddSameCenterConstraints(_scrollView, self.contentView);

  AddSameConstraintsWithInsets(
      _suggestionsStackView, _scrollView,
      ChromeDirectionalEdgeInsetsMake(kStackMargin, kStackMargin, kStackMargin,
                                      kStackMargin));

  [NSLayoutConstraint activateConstraints:@[
    [self.contentView.heightAnchor
        constraintEqualToAnchor:_scrollView.heightAnchor],
    [self.contentView.widthAnchor
        constraintEqualToAnchor:_scrollView.widthAnchor],
    [_scrollView.heightAnchor
        constraintEqualToAnchor:_suggestionsStackView.heightAnchor
                       constant:kStackMargin * 2]
  ]];
}

#pragma mark - Properties

- (NSArray<OmniboxPopupCarouselControl*>*)visibleControls {
  NSMutableArray* visibleControls = [[NSMutableArray alloc] init];
  for (OmniboxPopupCarouselControl* control in self.suggestionsStackView
           .arrangedSubviews) {
    if (!control.hidden) {
      [visibleControls addObject:control];
    }
  }
  return visibleControls;
}

#pragma mark - Accessibility

- (NSArray*)accessibilityElements {
  return self.visibleControls;
}

#pragma mark - Public methods

- (void)setupWithCarouselItems:(NSArray<CarouselItem*>*)carouselItems {
  if (self.contentView.subviews.count == 0) {
    [self addContentSubviews];
  }

  DCHECK(carouselItems.count <= kCarouselCapacity);
  for (NSUInteger i = 0; i < kCarouselCapacity; ++i) {
    OmniboxPopupCarouselControl* control =
        self.suggestionsStackView.arrangedSubviews[i];
    CarouselItem* item = i < carouselItems.count ? carouselItems[i] : nil;
    [control setCarouselItem:item];
    control.hidden = !item;
    control.menuProvider = self.menuProvider;
  }
}

- (void)updateCarouselItem:(CarouselItem*)carouselItem {
  OmniboxPopupCarouselControl* control =
      [self controlForCarouselItem:carouselItem];
  if (!control || control.hidden) {
    return;
  }
  [control setCarouselItem:carouselItem];
}

- (void)carouselItem:(CarouselItem*)carouselItem setHidden:(BOOL)hidden {
  OmniboxPopupCarouselControl* control =
      [self controlForCarouselItem:carouselItem];
  if (!control || control.hidden == hidden) {
    return;
  }
  control.hidden = hidden;
  control.selected = false;
  [self.delegate carouselCellDidChangeVisibleCount:self];
}

- (NSInteger)highlightedTileIndex {
  for (OmniboxPopupCarouselControl* control in self.suggestionsStackView
           .arrangedSubviews) {
    if (control.hidden) {
      continue;
    }

    if (control.selected) {
      return [self.suggestionsStackView.arrangedSubviews indexOfObject:control];
    }
  }

  return NSNotFound;
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

  for (OmniboxPopupCarouselControl* control in allTiles) {
    if (control.hidden) {
      continue;
    }
    control.selected = YES;
    return;
  }
}

- (void)performKeyboardAction:(OmniboxKeyboardAction)keyboardAction {
  // Find and unhighlight the previously highlighted suggestion.
  NSInteger previouslyHighlightedIndex = self.highlightedTileIndex;

  NSArray<OmniboxPopupCarouselControl*>* allTiles =
      self.suggestionsStackView.arrangedSubviews;
  if (previouslyHighlightedIndex != NSNotFound) {
    OmniboxPopupCarouselControl* previouslyHighlightedControl =
        allTiles[previouslyHighlightedIndex];
    previouslyHighlightedControl.selected = NO;

    OmniboxPopupCarouselControl* nextHighlightedControl =
        previouslyHighlightedControl;
    if (keyboardAction == OmniboxKeyboardActionRightArrow) {
      for (NSInteger i = previouslyHighlightedIndex + 1;
           i < static_cast<NSInteger>(allTiles.count); i++) {
        OmniboxPopupCarouselControl* control = allTiles[i];
        if (control.hidden) {
          continue;
        } else {
          nextHighlightedControl = control;
          break;
        }
      }
    } else if (keyboardAction == OmniboxKeyboardActionLeftArrow) {
      for (NSInteger i = previouslyHighlightedIndex - 1; i >= 0; i--) {
        OmniboxPopupCarouselControl* control = allTiles[i];
        if (control.hidden) {
          continue;
        } else {
          nextHighlightedControl = control;
          break;
        }
      }
    } else {
      NOTREACHED();
    }

    nextHighlightedControl.selected = YES;
  } else {
    [self highlightFirstTile];
  }
}

#pragma mark - OmniboxPopupCarouselControlDelegate

- (void)carouselControlDidBecomeFocused:(OmniboxPopupCarouselControl*)control {
  CGRect frameInScrollViewCoordinates = [control convertRect:control.bounds
                                                      toView:self.scrollView];
  CGRect frameWithPadding =
      CGRectInset(frameInScrollViewCoordinates, -kStackSpacing * 2, 0);
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

@end
