// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_coordinator.h"

#import "base/check_op.h"
#import "components/search_engines/search_engine_choice_utils.h"
#import "components/search_engines/search_engines_switches.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/first_run/first_run_screen_delegate.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_mediator.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_table/search_engine_choice_table_mediator.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_table/search_engine_choice_table_view_controller.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_view_controller.h"
#import "ios/chrome/browser/ui/search_engine_choice/why_am_i_seeing_this/why_am_i_seeing_this_coordinator.h"
#import "ios/chrome/browser/ui/search_engine_choice/why_am_i_seeing_this/why_am_i_seeing_this_view_controller.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface SearchEngineChoiceCoordinator () <
    SearchEngineChoiceTableActionDelegate,
    SearchEngineChoiceActionDelegate,
    LearnMoreCoordinatorDelegate>
@end

@implementation SearchEngineChoiceCoordinator {
  // The mediator that fetches the list of search engines.
  SearchEngineChoiceTableMediator* _searchEnginesTableMediator;
  // The view controller for the search engines table.
  SearchEngineChoiceTableViewController* _searchEnginesTableViewController;
  // The mediator for the search engine choice screen.
  SearchEngineChoiceMediator* _mediator;
  // The navigation controller displaying SearchEngineChoiceViewController.
  SearchEngineChoiceViewController* _viewController;
  // Coordinator for the informational popup that may be displayed to the user.
  WhyAmISeeingThisCoordinator* _whyAmISeeingThisCoordinator;
  // Whether the screen is being shown in the FRE.
  BOOL _firstRun;
  // First run screen delegate.
  __weak id<FirstRunScreenDelegate> _first_run_delegate;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _firstRun = NO;
  }
  return self;
}

- (instancetype)initForFirstRunWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                                    browser:(Browser*)browser
                                           firstRunDelegate:
                                               (id<FirstRunScreenDelegate>)
                                                   delegate {
  self = [self initWithBaseViewController:navigationController browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _firstRun = YES;
    _first_run_delegate = delegate;
  }
  return self;
}

- (void)start {
  [super start];

  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  _searchEnginesTableViewController =
      [[SearchEngineChoiceTableViewController alloc]
          initWithStyle:ChromeTableViewStyle()];
  FaviconLoader* faviconLoader =
      IOSChromeFaviconLoaderFactory::GetForBrowserState(browserState);
  _searchEnginesTableMediator = [[SearchEngineChoiceTableMediator alloc]
      initWithTemplateURLService:ios::TemplateURLServiceFactory::
                                     GetForBrowserState(browserState)
                     prefService:browserState->GetPrefs()
                   faviconLoader:faviconLoader];
  _searchEnginesTableMediator.consumer = _searchEnginesTableViewController;
  _searchEnginesTableViewController.delegate = self;

  _viewController = [[SearchEngineChoiceViewController alloc]
      initWithSearchEngineTableViewController:
          _searchEnginesTableViewController];
  _viewController.actionDelegate = self;
  _searchEnginesTableMediator.faviconUpdateConsumer = _viewController;

  _mediator =
      [[SearchEngineChoiceMediator alloc] initWithFaviconLoader:faviconLoader];
  _mediator.consumer = _viewController;

  _viewController.modalInPresentation = YES;
  if (_firstRun) {
    BOOL animated = self.baseNavigationController.topViewController != nil;
    [self.baseNavigationController setViewControllers:@[ _viewController ]
                                             animated:animated];
    search_engines::RecordChoiceScreenEvent(
        search_engines::SearchEngineChoiceScreenEvents::
            kFreChoiceScreenWasDisplayed);
  } else {
    [self.baseViewController presentViewController:_viewController
                                          animated:YES
                                        completion:nil];
    search_engines::RecordChoiceScreenEvent(
        search_engines::SearchEngineChoiceScreenEvents::
            kChoiceScreenWasDisplayed);
  }
}

- (void)stop {
  if (!_firstRun) {
    [_viewController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:nil];
  }

  [_whyAmISeeingThisCoordinator stop];
  _whyAmISeeingThisCoordinator = nil;
  _searchEnginesTableViewController.delegate = nil;
  _searchEnginesTableViewController = nil;
  [_searchEnginesTableMediator disconnect];
  _searchEnginesTableMediator.consumer = nil;
  _searchEnginesTableMediator = nil;
  [_mediator disconnect];
  _mediator = nil;
  _viewController = nil;
  _baseNavigationController = nil;
  _first_run_delegate = nil;
  [super stop];
}

#pragma mark - SearchEngineChoiceTableActionDelegate

- (void)selectSearchEngineAtRow:(NSInteger)row {
  _searchEnginesTableMediator.selectedRow = row;
  [_mediator
      setSelectedItem:_searchEnginesTableViewController.searchEngines[row]];
  _viewController.didUserSelectARow = YES;
  [_viewController updatePrimaryActionButton];
}

- (void)didReachBottom {
  _searchEnginesTableViewController.didReachBottom = YES;
  [_viewController updatePrimaryActionButton];
}

#pragma mark - SearchEngineChoiceViewControllerDelegate

- (void)showLearnMore {
  _whyAmISeeingThisCoordinator = [[WhyAmISeeingThisCoordinator alloc]
      initWithBaseViewController:_viewController
                         browser:self.browser];
  _whyAmISeeingThisCoordinator.delegate = self;
  [_whyAmISeeingThisCoordinator start];
  if (_firstRun) {
    search_engines::RecordChoiceScreenEvent(
        search_engines::SearchEngineChoiceScreenEvents::
            kFreLearnMoreWasDisplayed);
  } else {
    search_engines::RecordChoiceScreenEvent(
        search_engines::SearchEngineChoiceScreenEvents::kLearnMoreWasDisplayed);
  }
}

- (void)didTapPrimaryButton {
  if (_firstRun) {
    search_engines::RecordChoiceScreenEvent(
        search_engines::SearchEngineChoiceScreenEvents::kFreDefaultWasSet);
  } else {
    search_engines::RecordChoiceScreenEvent(
        search_engines::SearchEngineChoiceScreenEvents::kDefaultWasSet);
  }
  [_searchEnginesTableMediator saveDefaultSearchEngine];
  [self dismissChoiceScreen];
}

#pragma mark - LearnMoreCoordinatorDelegate

- (void)learnMoreDidDismiss {
  [_whyAmISeeingThisCoordinator stop];
  _whyAmISeeingThisCoordinator.delegate = nil;
  _whyAmISeeingThisCoordinator = nil;
}

#pragma mark - Private

- (void)dismissChoiceScreen {
  if (_firstRun) {
    [_first_run_delegate screenWillFinishPresenting];
  } else {
    [self.delegate choiceScreenWillBeDismissed:self];
  }
}

@end
