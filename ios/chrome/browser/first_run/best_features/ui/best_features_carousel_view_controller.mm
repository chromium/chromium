// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/best_features/ui/best_features_carousel_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/first_run/best_features/ui/feature_highlight_animated_view_controller.h"
#import "ios/chrome/browser/first_run/public/best_features_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller_delegate.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@interface BestFeaturesCarouselViewController () <
    UIPageViewControllerDataSource,
    UIPageViewControllerDelegate,
    PromoStyleViewControllerDelegate>
@end

@implementation BestFeaturesCarouselViewController {
  NSArray<BestFeaturesItem*>* _items;
  int _startIndex;
  UIPageViewController* _pageViewController;
  NSMutableArray<FeatureHighlightAnimatedViewController*>*
      _featureHighlightPages;
  UIPageControl* _pageControl;
}

- (instancetype)initWithBestFeaturesItems:(NSArray<BestFeaturesItem*>*)items
                               startIndex:(int)startIndex {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _items = items;
    _startIndex = startIndex;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];

  _featureHighlightPages = [[NSMutableArray alloc] init];
  for (NSUInteger i = 0; i < [_items count]; i++) {
    FeatureHighlightAnimatedViewController* viewController =
        [[FeatureHighlightAnimatedViewController alloc]
            initWithFeatureHighlightItem:_items[i]];
    viewController.delegate = self;
    [_featureHighlightPages addObject:viewController];
  }

  _pageViewController = [[UIPageViewController alloc]
      initWithTransitionStyle:UIPageViewControllerTransitionStyleScroll
        navigationOrientation:
            UIPageViewControllerNavigationOrientationHorizontal
                      options:nil];
  _pageViewController.dataSource = self;
  _pageViewController.delegate = self;

  [self addChildViewController:_pageViewController];
  [self.view addSubview:_pageViewController.view];
  [_pageViewController didMoveToParentViewController:self];

  _pageViewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(self.view, _pageViewController.view);

  _pageControl = [[UIPageControl alloc] init];
  _pageControl.numberOfPages = [_featureHighlightPages count];
  _pageControl.currentPage = _startIndex;
  _pageControl.pageIndicatorTintColor = [UIColor colorNamed:kGrey400Color];
  _pageControl.currentPageIndicatorTintColor =
      [UIColor colorNamed:kGrey700Color];
  _pageControl.translatesAutoresizingMaskIntoConstraints = NO;

  [self.view addSubview:_pageControl];

  [NSLayoutConstraint activateConstraints:@[
    [_pageControl.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [_pageControl.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor
                       constant:40],
  ]];

  if (_featureHighlightPages.count > 0) {
    [_pageViewController
        setViewControllers:@[
          _featureHighlightPages[static_cast<NSUInteger>(_startIndex)]
        ]
                 direction:UIPageViewControllerNavigationDirectionForward
                  animated:YES
                completion:nil];
  }
}

#pragma mark - Public

- (int)currentIndex {
  FeatureHighlightAnimatedViewController* currentViewController =
      base::apple::ObjCCastStrict<FeatureHighlightAnimatedViewController>(
          _pageViewController.viewControllers.firstObject);
  return [_featureHighlightPages indexOfObject:currentViewController];
}

#pragma mark - UIPageViewControllerDataSource

- (UIViewController*)pageViewController:
                         (UIPageViewController*)pageViewController
     viewControllerBeforeViewController:(UIViewController*)viewController {
  NSUInteger currentIndex = [_featureHighlightPages
      indexOfObject:base::apple::ObjCCastStrict<
                        FeatureHighlightAnimatedViewController>(
                        viewController)];
  if (currentIndex > 0) {
    return _featureHighlightPages[currentIndex - 1];
  }
  return nil;
}

- (UIViewController*)pageViewController:
                         (UIPageViewController*)pageViewController
      viewControllerAfterViewController:(UIViewController*)viewController {
  NSUInteger currentIndex = [_featureHighlightPages
      indexOfObject:base::apple::ObjCCastStrict<
                        FeatureHighlightAnimatedViewController>(
                        viewController)];
  if (currentIndex < [_items count] - 1) {
    return _featureHighlightPages[currentIndex + 1];
  }
  return nil;
}

#pragma mark - UIPageViewControllerDelegate

- (void)pageViewController:(UIPageViewController*)pageViewController
         didFinishAnimating:(BOOL)finished
    previousViewControllers:(NSArray<UIViewController*>*)previousViewControllers
        transitionCompleted:(BOOL)completed {
  if (completed) {
    UIViewController* currentViewController =
        pageViewController.viewControllers.firstObject;
    NSUInteger index = [_featureHighlightPages
        indexOfObject:base::apple::ObjCCastStrict<
                          FeatureHighlightAnimatedViewController>(
                          currentViewController)];
    if (index != NSNotFound) {
      _pageControl.currentPage = index;
    }
  }
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  [self.delegate didTapPrimaryActionButton];
}

@end
