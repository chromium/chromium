// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_first_run_carousel_view.h"

#import <algorithm>

#import "base/check.h"
#import "base/check_op.h"
#import "base/functional/bind.h"
#import "base/time/time.h"
#import "base/timer/timer.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_first_run_carousel_slide_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Normalized spacing constants.
const CGFloat kSpacingSmall = 6.0;
const CGFloat kSpacingMedium = 10.0;

// Carousel slide & peeking metrics.
const CGFloat kSlidePeekingPadding = 21.0;

// Page control indicator attributes.
const CGFloat kPageControlDotPointSize = 6.0;
const CGFloat kPageIndicatorUnselectedAlpha = 0.3;

// Timing.
const base::TimeDelta kAutoScrollInterval = base::Seconds(3);

}  // namespace

@interface GeminiFirstRunCarouselView () <UIScrollViewDelegate>
@end

@implementation GeminiFirstRunCarouselView {
  NSArray<GeminiFirstRunCarouselSlide*>* _slides;
  NSMutableArray<GeminiFirstRunCarouselSlideView*>* _slideViews;
  UIStackView* _slideStack;
  UIScrollView* _scrollView;
  UIPageControl* _pageControl;
  GeminiFirstRunCarouselSlideView* _dummyFirstSlideView;
  base::RepeatingTimer _autoScrollTimer;
  NSLayoutConstraint* _pageControlTopConstraint;
  BOOL _isProgrammaticScrolling;
  BOOL _hasInitialLayout;
  NSInteger _currentPage;
  NSInteger _previousPage;
}

#pragma mark - Initialization

- (instancetype)initWithSlides:(NSArray<GeminiFirstRunCarouselSlide*>*)slides {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    // There is no point in using a carousel view for 1 slide or fewer.
    CHECK(slides.count > 1);
    _slides = [slides copy];
    _slideViews = [NSMutableArray array];
    self.shouldGroupAccessibilityChildren = YES;

    [self setupSubviews];
    [self setupConstraints];

    [self registerForTraitChanges:@[ UITraitVerticalSizeClass.class ]
                       withAction:@selector(
                                      updateLayoutForCurrentTraitCollection)];
    [self updateLayoutForCurrentTraitCollection];

    CHECK_EQ(_slideViews.count, _slides.count);
  }
  return self;
}

#pragma mark - Public

- (void)startAutoScrolling {
  [self playCurrentSlideAnimation];
  [self stopAutoScrolling];
  __weak __typeof(self) weakSelf = self;
  _autoScrollTimer.Start(FROM_HERE, kAutoScrollInterval, base::BindRepeating(^{
                           [weakSelf advanceToNextSlide];
                         }));
}

- (void)stopAutoScrolling {
  _autoScrollTimer.Stop();
  _isProgrammaticScrolling = NO;
  [self cleanupDummySlideIfPresent];
}

- (void)recenterActiveSlide {
  CGFloat pageWidth = _scrollView.bounds.size.width;
  if (pageWidth > 0) {
    CGPoint targetOffset =
        CGPointMake([self contentOffsetXForPage:_currentPage], 0);
    if (!CGPointEqualToPoint(_scrollView.contentOffset, targetOffset)) {
      _scrollView.contentOffset = targetOffset;
    }
  }
}

- (void)prepareForSizeTransition {
  _isProgrammaticScrolling = YES;
}

- (void)completeSizeTransition {
  [self recenterActiveSlide];
  _isProgrammaticScrolling = NO;
}

#pragma mark - UIView

- (void)layoutSubviews {
  [super layoutSubviews];
  // On initial layout, ensure the active slide is properly positioned within
  // the scroll view once bounds are established (particularly important for
  // RTL where slide 0 starts at the maximum content offset).
  if (!_hasInitialLayout && _scrollView.bounds.size.width > 0) {
    _hasInitialLayout = YES;
    [self recenterActiveSlide];
  }
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  // Ignore programmatic animated page transitions to prevent the page control
  // indicator dots from flickering.
  if (_isProgrammaticScrolling) {
    return;
  }
  // Update the current page index and page control indicator dots in real-time
  // as the user drags past the midpoint of each slide.
  CGFloat pageWidth = scrollView.bounds.size.width;
  if (pageWidth > 0) {
    NSInteger page =
        [self pageIndexForContentOffset:scrollView.contentOffset.x];
    if (page >= 0 && page < static_cast<NSInteger>(_slides.count)) {
      _currentPage = page;
      _pageControl.currentPage = page;
    }
  }
}

- (void)scrollViewWillBeginDragging:(UIScrollView*)scrollView {
  _isProgrammaticScrolling = NO;
  _previousPage = _currentPage;
  // Pause auto-scrolling when the user manually interacts with the carousel.
  [self stopAutoScrolling];
}

- (void)scrollViewDidEndDecelerating:(UIScrollView*)scrollView {
  [self handleUserScrollDidEnd];
}

- (void)scrollViewDidEndDragging:(UIScrollView*)scrollView
                  willDecelerate:(BOOL)decelerate {
  if (!decelerate) {
    [self handleUserScrollDidEnd];
  }
}

- (void)scrollViewDidEndScrollingAnimation:(UIScrollView*)scrollView {
  _previousPage = _currentPage;
  [self cleanupOffscreenSlideAnimations];
  if (_dummyFirstSlideView.superview != nil) {
    // We just finished auto-scrolling into the duplicate of the first slide.
    // Instantly reset the scroll offset back to the real first slide (index 0)
    // without animation and detach the duplicate slide.
    _scrollView.contentOffset = CGPointMake([self contentOffsetXForPage:0], 0);
    [self cleanupDummySlideIfPresent];
  }
  [self playCurrentSlideAnimation];
  _isProgrammaticScrolling = NO;
}

#pragma mark - Private

- (void)handleUserScrollDidEnd {
  // Play the Lottie animation only if the user transitioned to a new slide.
  if (_currentPage != _previousPage) {
    _previousPage = _currentPage;
    [self cleanupOffscreenSlideAnimations];
    [self playCurrentSlideAnimation];
  }
}

- (BOOL)isRTL {
  return self.effectiveUserInterfaceLayoutDirection ==
         UIUserInterfaceLayoutDirectionRightToLeft;
}

- (CGFloat)contentOffsetXForPage:(NSInteger)page {
  CGFloat pageWidth = _scrollView.bounds.size.width;
  if (pageWidth <= 0 || _slides.count == 0) {
    return 0;
  }
  NSInteger clampedPage = std::clamp(page, static_cast<NSInteger>(0),
                                     static_cast<NSInteger>(_slides.count - 1));
  NSInteger visualIndex =
      [self isRTL] ? (static_cast<NSInteger>(_slides.count) - 1 - clampedPage)
                   : clampedPage;
  return visualIndex * pageWidth;
}

- (NSInteger)pageIndexForContentOffset:(CGFloat)offsetX {
  CGFloat pageWidth = _scrollView.bounds.size.width;
  if (pageWidth <= 0) {
    return 0;
  }
  NSInteger visualIndex = round(offsetX / pageWidth);
  if (visualIndex < 0 || visualIndex >= static_cast<NSInteger>(_slides.count)) {
    return NSNotFound;
  }
  return [self isRTL]
             ? (static_cast<NSInteger>(_slides.count) - 1 - visualIndex)
             : visualIndex;
}

- (void)cleanupOffscreenSlideAnimations {
  for (NSInteger i = 0; i < static_cast<NSInteger>(_slideViews.count); ++i) {
    if (i != _currentPage) {
      [self resetSlideToFirstFrame:i];
    }
  }
}

- (void)cleanupDummySlideIfPresent {
  if (_dummyFirstSlideView.superview != nil) {
    [_dummyFirstSlideView removeFromSuperview];
  }
}

- (void)scrollToPage:(NSInteger)page animated:(BOOL)animated {
  CGFloat pageWidth = _scrollView.bounds.size.width;
  if (pageWidth <= 0) {
    return;
  }
  _previousPage = _currentPage;
  _currentPage = page;
  _pageControl.currentPage = page;
  _isProgrammaticScrolling = animated;

  CGPoint targetOffset = CGPointMake([self contentOffsetXForPage:page], 0);
  [_scrollView setContentOffset:targetOffset animated:animated];
  if (!animated) {
    // Setting content offset with animated:NO does not trigger
    // scrollViewDidEndScrollingAnimation:, so we must reset off-screen
    // slides and start target slide playback synchronously here.
    [self cleanupOffscreenSlideAnimations];
    [self playCurrentSlideAnimation];
  }
}

// Handles user tapping or scrubbing on the page control dots to jump directly
// to a specific slide.
- (void)pageControlValueChanged:(UIPageControl*)sender {
  // Stop auto-scrolling when the user directly interacts with the page control.
  [self stopAutoScrolling];
  if (sender.currentPage == _currentPage) {
    return;
  }
  BOOL isDiscrete =
      (sender.interactionState == UIPageControlInteractionStateDiscrete);
  [self scrollToPage:sender.currentPage animated:isDiscrete];
}

// Used to advance to the next slide. When advancing past the last slide,
// seamlessly scrolls forward into a temporary duplicate of the first slide
// before resetting to index 0.
- (void)advanceToNextSlide {
  CGFloat pageWidth = _scrollView.bounds.size.width;
  if (pageWidth <= 0) {
    return;
  }

  if (_currentPage < static_cast<NSInteger>(_slides.count) - 1) {
    [self scrollToPage:_currentPage + 1 animated:YES];
  } else {
    // Wrap around seamlessly into the duplicate first slide.
    [self attachDummyFirstSlide];
    _isProgrammaticScrolling = YES;
    _previousPage = _currentPage;
    _currentPage = 0;
    _pageControl.currentPage = 0;
    if ([self isRTL]) {
      // In RTL, adding the dummy first slide at trailing (leftmost edge) shifts
      // the existing slides right by 1 page width. Set the content offset to 1
      // page width so the user continues seeing the last slide, then animate
      // leftward to x = 0 (the dummy first slide).
      _scrollView.contentOffset = CGPointMake(pageWidth, 0);
      [_scrollView setContentOffset:CGPointZero animated:YES];
    } else {
      CGPoint targetOffset = CGPointMake(_slides.count * pageWidth, 0);
      [_scrollView setContentOffset:targetOffset animated:YES];
    }
  }
}

- (void)attachDummyFirstSlide {
  if (_dummyFirstSlideView.superview == nil) {
    [_slideStack addArrangedSubview:_dummyFirstSlideView];
    [NSLayoutConstraint activateConstraints:@[
      [_dummyFirstSlideView.widthAnchor
          constraintEqualToAnchor:_slideViews.firstObject.widthAnchor],
    ]];
    [_slideStack layoutIfNeeded];
  }
  [_dummyFirstSlideView resetToFirstFrame];
}

- (void)setupSubviews {
  // Initialize the horizontal paging scroll view.
  _scrollView = [[UIScrollView alloc] init];
  _scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  _scrollView.pagingEnabled = YES;
  _scrollView.showsHorizontalScrollIndicator = NO;
  _scrollView.showsVerticalScrollIndicator = NO;
  _scrollView.clipsToBounds = NO;
  _scrollView.delegate = self;
  [self addSubview:_scrollView];

  // Horizontal stack view inside the scroll view that holds all slide views
  // side-by-side with some spacing between pages.
  _slideStack = [[UIStackView alloc] init];
  _slideStack.translatesAutoresizingMaskIntoConstraints = NO;
  _slideStack.spacing = kSlidePeekingPadding;
  // Add a trailing margin equal to kSlidePeekingPadding so the scroll view's
  // contentSize width is an exact multiple of its bounds width, allowing the
  // final slide to scroll fully into position without being clipped.
  _slideStack.directionalLayoutMargins =
      NSDirectionalEdgeInsetsMake(0, 0, 0, kSlidePeekingPadding);
  _slideStack.layoutMarginsRelativeArrangement = YES;
  [_scrollView addSubview:_slideStack];

  // Create each slide view.
  for (GeminiFirstRunCarouselSlide* slide in _slides) {
    GeminiFirstRunCarouselSlideView* slideView =
        [[GeminiFirstRunCarouselSlideView alloc] initWithSlide:slide];
    [_slideStack addArrangedSubview:slideView];
    [_slideViews addObject:slideView];
  }

  // Create a duplicate of the first slide for seamless forward auto-scrolling.
  // It is attached only during the wrap-around scroll animation.
  _dummyFirstSlideView =
      [[GeminiFirstRunCarouselSlideView alloc] initWithSlide:_slides[0]];

  // Configure the page indicator dots. User interaction is enabled to support
  // tapping dots, scrubbing across pages, and VoiceOver accessibility.
  _pageControl = [[UIPageControl alloc] init];
  _pageControl.translatesAutoresizingMaskIntoConstraints = NO;
  _pageControl.numberOfPages = _slides.count;
  _pageControl.pageIndicatorTintColor = [[UIColor colorNamed:kTextPrimaryColor]
      colorWithAlphaComponent:kPageIndicatorUnselectedAlpha];
  _pageControl.currentPageIndicatorTintColor =
      [UIColor colorNamed:kTextPrimaryColor];
  UIImageSymbolConfiguration* dotConfig = [UIImageSymbolConfiguration
      configurationWithPointSize:kPageControlDotPointSize
                          weight:UIImageSymbolWeightRegular];
  _pageControl.preferredIndicatorImage =
      SymbolWithConfiguration(SymbolCircleFill, dotConfig);
  [_pageControl addTarget:self
                   action:@selector(pageControlValueChanged:)
         forControlEvents:UIControlEventValueChanged];
  [self addSubview:_pageControl];
}

- (void)setupConstraints {
  _pageControlTopConstraint =
      [_pageControl.topAnchor constraintEqualToAnchor:_scrollView.bottomAnchor
                                             constant:kSpacingMedium];

  NSMutableArray<NSLayoutConstraint*>* constraints =
      [NSMutableArray arrayWithArray:@[
        [_slideStack.heightAnchor
            constraintEqualToAnchor:_scrollView.frameLayoutGuide.heightAnchor],
        [_scrollView.topAnchor constraintEqualToAnchor:self.topAnchor],
        [_scrollView.leadingAnchor
            constraintEqualToAnchor:self.leadingAnchor
                           constant:kSlidePeekingPadding],
        [_scrollView.widthAnchor constraintEqualToAnchor:self.widthAnchor
                                                constant:-kSlidePeekingPadding],
        _pageControlTopConstraint,
        [_pageControl.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
        [_pageControl.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
      ]];

  for (GeminiFirstRunCarouselSlideView* slideView in _slideViews) {
    [constraints addObject:[slideView.widthAnchor
                               constraintEqualToAnchor:_scrollView.widthAnchor
                                              constant:-kSlidePeekingPadding]];
  }

  // Pin the horizontal slide stack to the scroll view's content layout guide.
  AddSameConstraints(_slideStack, _scrollView.contentLayoutGuide);

  [NSLayoutConstraint activateConstraints:constraints];
}

- (void)updateLayoutForCurrentTraitCollection {
  BOOL isCompactHeight = (self.traitCollection.verticalSizeClass ==
                          UIUserInterfaceSizeClassCompact);
  // Adjust page control spacing based on size class.
  _pageControlTopConstraint.constant =
      isCompactHeight ? kSpacingSmall : kSpacingMedium;
}

- (void)resetSlideToFirstFrame:(NSInteger)slideIndex {
  [_slideViews[slideIndex] resetToFirstFrame];
}

- (void)playCurrentSlideAnimation {
  [_slideViews[_currentPage] playAnimation];
}

@end
