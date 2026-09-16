// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_view_controller.h"

#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_actor_view_controller.h"
#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_apc_view_controller.h"
#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_calendar_view_controller.h"
#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_consumer.h"
#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_freeform_view_controller.h"
#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_tab_organization_view_controller.h"
#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_ui_catalog_view_controller.h"
#import "ios/chrome/browser/ai_prototyping/utils/ai_prototyping_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface AIPrototypingViewController () <UIPageViewControllerDataSource,
                                           UIPageViewControllerDelegate> {
  // The view controller for each page in the menu.
  NSArray<UIViewController<AIPrototypingViewControllerProtocol>*>* _menuPages;

  // The controller allowing for navigation between the menu sheets.
  UIPageViewController* _pageController;

  // The view controller for the actor tools page.
  AIPrototypingActorViewController* _actorViewController;
}

@end

@implementation AIPrototypingViewController

- (instancetype)initWithTTCViewController:
    (UIViewController<AIPrototypingViewControllerProtocol>*)ttcViewController {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _actorViewController = [[AIPrototypingActorViewController alloc]
        initForFeature:AIPrototypingFeature::kActorTools];
    NSMutableArray* pages = [NSMutableArray
        arrayWithObjects:
            [[AIPrototypingFreeformViewController alloc]
                initForFeature:AIPrototypingFeature::kFreeform],
            [[AIPrototypingUICatalogNavigationController alloc]
                initForFeature:AIPrototypingFeature::kUICatalog],
            [[AIPrototypingTabOrganizationViewController alloc]
                initForFeature:AIPrototypingFeature::kSmartTabGrouping],
            [[AIPrototypingCalendarViewController alloc]
                initForFeature:AIPrototypingFeature::kEnhancedCalendar],
            [[AIPrototypingAPCViewController alloc]
                initForFeature:AIPrototypingFeature::kAPC],
            _actorViewController, nil];
    if (ttcViewController) {
      [pages addObject:ttcViewController];
    }
    _menuPages = [pages copy];
  }
  return self;
}

#pragma mark - UIViewController

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

- (void)updateRawBytes:(NSString*)rawBytes
            forFeature:(AIPrototypingFeature)feature {
  for (UIViewController<AIPrototypingViewControllerProtocol>* viewController in
           _menuPages) {
    if (viewController.feature == feature) {
      if ([viewController respondsToSelector:@selector(updateRawBytes:)]) {
        [viewController updateRawBytes:rawBytes];
      }
      break;
    }
  }
}

- (void)updateWindowId:(NSString*)windowId {
  [_actorViewController updateWindowId:windowId];
}

- (void)updateTabList:(NSArray<NSDictionary*>*)tabs {
  [_actorViewController updateTabList:tabs];
}

- (void)updateFrameList:(NSArray<NSDictionary*>*)frames {
  [_actorViewController updateFrameList:frames];
}

- (void)updateFramesAndContentNodesDebugString:(NSString*)debugString {
  [_actorViewController updateFramesAndContentNodesDebugString:debugString];
}

#pragma mark - UIPageViewControllerDataSource

- (UIViewController*)
                    pageViewController:(UIPageViewController*)pageViewController
    viewControllerBeforeViewController:
        (UIViewController<AIPrototypingViewControllerProtocol>*)viewController {
  if ([_menuPages count] <= 1) {
    return nil;
  }
  NSUInteger currentIndex = [_menuPages indexOfObject:viewController];
  if (currentIndex == 0) {
    return [_menuPages lastObject];
  }
  if (currentIndex != NSNotFound) {
    return [_menuPages objectAtIndex:(currentIndex - 1)];
  }
  return nil;
}

- (UIViewController*)
                   pageViewController:(UIPageViewController*)pageViewController
    viewControllerAfterViewController:
        (UIViewController<AIPrototypingViewControllerProtocol>*)viewController {
  if ([_menuPages count] <= 1) {
    return nil;
  }
  NSUInteger currentIndex = [_menuPages indexOfObject:viewController];
  if (currentIndex == ([_menuPages count] - 1)) {
    return [_menuPages firstObject];
  }
  if (currentIndex != NSNotFound) {
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
