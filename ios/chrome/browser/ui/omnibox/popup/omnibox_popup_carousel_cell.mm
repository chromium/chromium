// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_carousel_cell.h"

#import "base/check.h"
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

#pragma mark - Accessibility

- (NSArray*)accessibilityElements {
  return self.suggestionsStackView.arrangedSubviews;
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
    [control setupWithCarouselItem:item];
    control.hidden = !item;
    control.menuProvider = self.menuProvider;
  }
}

- (void)updateCarouselItem:(CarouselItem*)carouselItem {
  for (OmniboxPopupCarouselControl* control in self.suggestionsStackView
           .arrangedSubviews) {
    // Check only visible controls.
    if (control.hidden) {
      break;
    }
    if (control.carouselItem == carouselItem) {
      [control setupWithCarouselItem:carouselItem];
    }
  }
}

#pragma mark - Private methods

- (void)didTapCarouselControl:(OmniboxPopupCarouselControl*)control {
  DCHECK(control.carouselItem);
  [self.delegate didTapCarouselItem:control.carouselItem];
}

@end
