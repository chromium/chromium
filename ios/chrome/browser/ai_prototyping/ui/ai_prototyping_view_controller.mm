// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_view_controller.h"

#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_consumer.h"
#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_freeform_view_controller.h"
#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_mutator.h"
#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_view_controller_protocol.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@interface AIPrototypingViewController () <UIPageViewControllerDataSource,
                                           UIPageViewControllerDelegate>

// The controller allowing for navigation between the menu sheets.
@property(nonatomic, strong) UIPageViewController* pageController;

// The currently visible menu page.
@property(nonatomic, weak)
    id<AIPrototypingConsumer, AIPrototypingViewControllerProtocol>
        currentPage;

@end

@implementation AIPrototypingViewController

// Synthesized from `AIPrototypingViewControllerProtocol`.
@synthesize mutator = _mutator;

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.translatesAutoresizingMaskIntoConstraints = NO;
  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];

  self.pageController = [[UIPageViewController alloc]
      initWithTransitionStyle:UIPageViewControllerTransitionStyleScroll
        navigationOrientation:
            UIPageViewControllerNavigationOrientationHorizontal
                      options:nil];
  self.pageController.dataSource = self;
  self.pageController.delegate = self;

  [self addChildViewController:self.pageController];
  [self.view addSubview:self.pageController.view];
  [self.pageController didMoveToParentViewController:self];

  UIViewController<AIPrototypingConsumer, AIPrototypingViewControllerProtocol>*
      freeformViewController =
          [[AIPrototypingFreeformViewController alloc] init];
  freeformViewController.mutator = self.mutator;
  self.currentPage = freeformViewController;

  NSArray* viewControllers = [NSArray arrayWithObject:freeformViewController];
  [self.pageController
      setViewControllers:viewControllers
               direction:UIPageViewControllerNavigationDirectionForward
                animated:NO
              completion:nil];
}

#pragma mark - AIPrototypingConsumer

- (void)updateQueryResult:(NSString*)result {
  [self.currentPage updateQueryResult:result];
}

#pragma mark - UIPageViewControllerDataSource

- (UIViewController*)pageViewController:
                         (UIPageViewController*)pageViewController
     viewControllerBeforeViewController:(UIViewController*)viewController {
  // Implement this when we have multiple pages.
  return nil;
}

- (UIViewController*)pageViewController:
                         (UIPageViewController*)pageViewController
      viewControllerAfterViewController:(UIViewController*)viewController {
  // Implement this when we have multiple pages.
  return nil;
}

@end
