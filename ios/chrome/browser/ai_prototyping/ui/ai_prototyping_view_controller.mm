// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_view_controller.h"

#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_calendar_view_controller.h"
#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_consumer.h"
#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_freeform_view_controller.h"
#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_tab_organization_view_controller.h"
#import "ios/chrome/browser/ai_prototyping/utils/ai_prototyping_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface AIPrototypingViewController () <UIPageViewControllerDataSource,
                                           UIPageViewControllerDelegate> {
  // The view controller for each page in the menu.
  NSArray<UIViewController<AIPrototypingViewControllerProtocol>*>* _menuPages;

  // The controller allowing for navigation between the menu sheets.
  UIPageViewController* _pageController;
}

@end

@implementation AIPrototypingViewController

- (instancetype)init {
  self = [super init];
  if (self) {
    _menuPages = [NSArray
        arrayWithObjects:
            [[AIPrototypingFreeformViewController alloc]
                initForFeature:AIPrototypingFeature::kFreeform],
            [[AIPrototypingTabOrganizationViewController alloc]
                initForFeature:AIPrototypingFeature::kSmartTabGrouping],
            [[AIPrototypingCalendarViewController alloc]
                initForFeature:AIPrototypingFeature::kEnhancedCalendar],
            nil];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.translatesAutoresizingMaskIntoConstraints = NO;
  self.view.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];

  _pageController = [[UIPageViewController alloc]
      initWithTransitionStyle:UIPageViewControllerTransitionStyleScroll
        navigationOrientation:
            UIPageViewControllerNavigationOrientationHorizontal
                      options:nil];
  _pageController.dataSource = self;
  _pageController.delegate = self;

  [self addChildViewController:_pageController];
  [self.view addSubview:_pageController.view];
  [_pageController didMoveToParentViewController:self];

  [_pageController
      setViewControllers:[NSArray arrayWithObject:[_menuPages firstObject]]
               direction:UIPageViewControllerNavigationDirectionForward
                animated:NO
              completion:nil];
}

#pragma mark - AIPrototypingConsumer

- (void)updateQueryResult:(NSString*)result
               forFeature:(AIPrototypingFeature)feature {
  for (UIViewController<AIPrototypingViewControllerProtocol>* viewController in
           _menuPages) {
    if (viewController.feature == feature) {
      [viewController enableSubmitButtons];
      [viewController updateResponseField:result];
      break;
    }
  }
}

#pragma mark - UIPageViewControllerDataSource

- (UIViewController*)
                    pageViewController:(UIPageViewController*)pageViewController
    viewControllerBeforeViewController:
        (UIViewController<AIPrototypingViewControllerProtocol>*)viewController {
  NSUInteger currentIndex = [_menuPages indexOfObject:viewController];
  if (currentIndex > 0) {
    return [_menuPages objectAtIndex:(currentIndex - 1)];
  }
  return nil;
}

- (UIViewController*)
                   pageViewController:(UIPageViewController*)pageViewController
    viewControllerAfterViewController:
        (UIViewController<AIPrototypingViewControllerProtocol>*)viewController {
  NSUInteger currentIndex = [_menuPages indexOfObject:viewController];
  if (currentIndex < ([_menuPages count] - 1)) {
    return [_menuPages objectAtIndex:(currentIndex + 1)];
  }
  return nil;
}

- (NSInteger)presentationCountForPageViewController:
    (UIPageViewController*)pageViewController {
  return [_menuPages count];
}

- (NSInteger)presentationIndexForPageViewController:
    (UIPageViewController*)pageViewController {
  return 0;
}

#pragma mark - Setters

- (void)setMutator:(id<AIPrototypingMutator>)mutator {
  _mutator = mutator;
  // Assign the mutator to each menu page.
  for (UIViewController<AIPrototypingViewControllerProtocol>* viewController in
           _menuPages) {
    viewController.mutator = mutator;
  }
}

@end
