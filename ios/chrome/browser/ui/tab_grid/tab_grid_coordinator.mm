// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/tab_grid_coordinator.h"

#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/history/history_coordinator.h"
#import "ios/chrome/browser/ui/history/public/history_presentation_delegate.h"
#import "ios/chrome/browser/ui/main/bvc_container_view_controller.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_mediator.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_presentation_delegate.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_table_view_controller.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_adaptor.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_mediator.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_paging.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_transition_handler.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_url_loader.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TabGridCoordinator ()<TabPresentationDelegate,
                                 HistoryPresentationDelegate,
                                 RecentTabsPresentationDelegate>
// Superclass property specialized for the class that this coordinator uses.
@property(nonatomic, weak) TabGridViewController* baseViewController;
// Pointer to the masking view used to prevent the main view controller from
// being shown at launch.
@property(nonatomic, strong) UIView* launchMaskView;
// Commad dispatcher used while this coordinator's view controller is active.
// (for compatibility with the TabSwitcher protocol).
@property(nonatomic, strong) CommandDispatcher* dispatcher;
// Object that internally backs the public  TabSwitcher
@property(nonatomic, strong) TabGridAdaptor* adaptor;
// Container view controller for the BVC to live in; this class's view
// controller will present this.
@property(nonatomic, strong) BVCContainerViewController* bvcContainer;
// Transitioning delegate for the view controller.
@property(nonatomic, strong) TabGridTransitionHandler* transitionHandler;
// Mediator for regular Tabs.
@property(nonatomic, strong) TabGridMediator* regularTabsMediator;
// Mediator for incognito Tabs.
@property(nonatomic, strong) TabGridMediator* incognitoTabsMediator;
// Mediator for remote Tabs.
@property(nonatomic, strong) RecentTabsMediator* remoteTabsMediator;
// Coordinator for history, which can be started from recent tabs.
@property(nonatomic, strong) HistoryCoordinator* historyCoordinator;
// Specialized URL loader for tab grid, since tab grid has a different use case
// than BVC.
@property(nonatomic, strong) TabGridURLLoader* URLLoader;
@end

@implementation TabGridCoordinator
// Superclass property.
@synthesize baseViewController = _baseViewController;
// Public properties.
@synthesize animationsDisabledForTesting = _animationsDisabledForTesting;
@synthesize regularTabModel = _regularTabModel;
@synthesize incognitoTabModel = _incognitoTabModel;
// Private properties.
@synthesize launchMaskView = _launchMaskView;
@synthesize dispatcher = _dispatcher;
@synthesize adaptor = _adaptor;
@synthesize bvcContainer = _bvcContainer;
@synthesize transitionHandler = _transitionHandler;
@synthesize regularTabsMediator = _regularTabsMediator;
@synthesize incognitoTabsMediator = _incognitoTabsMediator;
@synthesize remoteTabsMediator = _remoteTabsMediator;
@synthesize historyCoordinator = _historyCoordinator;
@synthesize URLLoader = _URLLoader;

- (instancetype)initWithWindow:(nullable UIWindow*)window
    applicationCommandEndpoint:
        (id<ApplicationCommands>)applicationCommandEndpoint {
  if ((self = [super initWithWindow:window])) {
    _dispatcher = [[CommandDispatcher alloc] init];
    [_dispatcher startDispatchingToTarget:applicationCommandEndpoint
                              forProtocol:@protocol(ApplicationCommands)];
    // -startDispatchingToTarget:forProtocol: doesn't pick up protocols the
    // passed protocol conforms to, so ApplicationSettingsCommands and
    // BrowsingDataCommands are explicitly dispatched to the endpoint as well.
    [_dispatcher
        startDispatchingToTarget:applicationCommandEndpoint
                     forProtocol:@protocol(ApplicationSettingsCommands)];
    [_dispatcher startDispatchingToTarget:applicationCommandEndpoint
                              forProtocol:@protocol(BrowsingDataCommands)];
  }
  return self;
}

#pragma mark - Public

- (id<TabSwitcher>)tabSwitcher {
  return self.adaptor;
}

- (TabModel*)regularTabModel {
  // Ensure tab model actually used by the mediator is returned, as it may have
  // been updated.
  return self.regularTabsMediator ? self.regularTabsMediator.tabModel
                                  : _regularTabModel;
}

- (void)setRegularTabModel:(TabModel*)regularTabModel {
  if (self.regularTabsMediator) {
    self.regularTabsMediator.tabModel = regularTabModel;
  } else {
    _regularTabModel = regularTabModel;
  }
}

- (TabModel*)incognitoTabModel {
  // Ensure tab model actually used by the mediator is returned, as it may have
  // been updated.
  return self.incognitoTabsMediator ? self.incognitoTabsMediator.tabModel
                                    : _incognitoTabModel;
}

- (void)setIncognitoTabModel:(TabModel*)incognitoTabModel {
  if (self.incognitoTabsMediator) {
    self.incognitoTabsMediator.tabModel = incognitoTabModel;
  } else {
    _incognitoTabModel = incognitoTabModel;
  }
}

- (void)stopChildCoordinatorsWithCompletion:(ProceduralBlock)completion {
  // Recent tabs context menu may be presented on top of the tab grid.
  [self.baseViewController.remoteTabsViewController dismissModals];
  // History may be presented on top of the tab grid.
  if (self.historyCoordinator) {
    [self.historyCoordinator stopWithCompletion:completion];
  } else if (completion) {
    completion();
  }
}

#pragma mark - ChromeCoordinator

- (void)start {
  TabGridViewController* baseViewController =
      [[TabGridViewController alloc] init];
  baseViewController.dispatcher =
      static_cast<id<ApplicationCommands>>(self.dispatcher);
  self.transitionHandler = [[TabGridTransitionHandler alloc] init];
  self.transitionHandler.provider = baseViewController;
  baseViewController.modalPresentationStyle = UIModalPresentationCustom;
  baseViewController.transitioningDelegate = self.transitionHandler;
  baseViewController.tabPresentationDelegate = self;
  _baseViewController = baseViewController;

  self.adaptor = [[TabGridAdaptor alloc] init];
  self.adaptor.tabGridViewController = self.baseViewController;
  self.adaptor.adaptedDispatcher =
      static_cast<id<ApplicationCommands, OmniboxFocuser, ToolbarCommands>>(
          self.dispatcher);
  self.adaptor.tabGridPager = baseViewController;

  self.regularTabsMediator = [[TabGridMediator alloc]
      initWithConsumer:baseViewController.regularTabsConsumer];
  self.regularTabsMediator.tabModel = _regularTabModel;
  if (_regularTabModel.browserState) {
    self.regularTabsMediator.tabRestoreService =
        IOSChromeTabRestoreServiceFactory::GetForBrowserState(
            _regularTabModel.browserState);
  }
  self.incognitoTabsMediator = [[TabGridMediator alloc]
      initWithConsumer:baseViewController.incognitoTabsConsumer];
  self.incognitoTabsMediator.tabModel = _incognitoTabModel;
  self.adaptor.incognitoMediator = self.incognitoTabsMediator;
  baseViewController.regularTabsDelegate = self.regularTabsMediator;
  baseViewController.incognitoTabsDelegate = self.incognitoTabsMediator;
  baseViewController.regularTabsImageDataSource = self.regularTabsMediator;
  baseViewController.incognitoTabsImageDataSource = self.incognitoTabsMediator;

  // TODO(crbug.com/845192) : Remove RecentTabsTableViewController dependency on
  // ChromeBrowserState so that we don't need to expose the view controller.
  baseViewController.remoteTabsViewController.browserState =
      _regularTabModel.browserState;
  self.remoteTabsMediator = [[RecentTabsMediator alloc] init];
  self.remoteTabsMediator.browserState = _regularTabModel.browserState;
  self.remoteTabsMediator.consumer = baseViewController.remoteTabsConsumer;
  // TODO(crbug.com/845636) : Currently, the image data source must be set
  // before the mediator starts updating its consumer. Fix this so that order of
  // calls does not matter.
  baseViewController.remoteTabsViewController.imageDataSource =
      self.remoteTabsMediator;
  baseViewController.remoteTabsViewController.delegate =
      self.remoteTabsMediator;
  baseViewController.remoteTabsViewController.dispatcher =
      static_cast<id<ApplicationCommands>>(self.dispatcher);
  self.URLLoader = [[TabGridURLLoader alloc]
      initWithRegularWebStateList:self.regularTabModel.webStateList
            incognitoWebStateList:self.incognitoTabModel.webStateList
              regularBrowserState:self.regularTabModel.browserState
            incognitoBrowserState:self.incognitoTabModel.browserState];
  self.adaptor.loader = self.URLLoader;
  baseViewController.remoteTabsViewController.loader = self.URLLoader;
  baseViewController.remoteTabsViewController.presentationDelegate = self;

  // Insert the launch screen view in front of this view to hide it until after
  // launch. This should happen before |baseViewController| is made the window's
  // root view controller.
  NSBundle* mainBundle = base::mac::FrameworkBundle();
  NSArray* topObjects =
      [mainBundle loadNibNamed:@"LaunchScreen" owner:self options:nil];
  UIViewController* launchScreenController =
      base::mac::ObjCCastStrict<UIViewController>([topObjects lastObject]);
  self.launchMaskView = launchScreenController.view;
  [baseViewController.view addSubview:self.launchMaskView];

  // TODO(crbug.com/850387) : Currently, consumer calls from the mediator
  // prematurely loads the view in |RecentTabsTableViewController|. Fix this so
  // that the view is loaded only by an explicit placement in the view
  // hierarchy. As a workaround, the view controller hierarchy is loaded here
  // before |RecentTabsMediator| updates are started.
  self.window.rootViewController = self.baseViewController;
  if (self.remoteTabsMediator.browserState) {
    [self.remoteTabsMediator initObservers];
    [self.remoteTabsMediator refreshSessionsView];
  }

  // Once the mediators are set up, stop keeping pointers to the tab models used
  // to initialize them.
  _regularTabModel = nil;
  _incognitoTabModel = nil;
}

- (void)stop {
  [self.dispatcher stopDispatchingForProtocol:@protocol(ApplicationCommands)];
  [self.dispatcher
      stopDispatchingForProtocol:@protocol(ApplicationSettingsCommands)];
  [self.dispatcher stopDispatchingForProtocol:@protocol(BrowsingDataCommands)];

  // TODO(crbug.com/845192) : RecentTabsTableViewController behaves like a
  // coordinator and that should be factored out.
  [self.baseViewController.remoteTabsViewController dismissModals];
  [self.remoteTabsMediator disconnect];
  self.remoteTabsMediator = nil;
}

#pragma mark - ViewControllerSwapping

- (UIViewController*)activeViewController {
  if (self.bvcContainer) {
    DCHECK_EQ(self.bvcContainer,
              self.baseViewController.presentedViewController);
    DCHECK(self.bvcContainer.currentBVC);
    return self.bvcContainer.currentBVC;
  }
  return self.baseViewController;
}

- (UIViewController*)viewController {
  return self.baseViewController;
}

- (void)prepareToShowTabSwitcher:(id<TabSwitcher>)tabSwitcher {
  DCHECK(tabSwitcher);
  DCHECK_EQ([tabSwitcher viewController], self.baseViewController);
  // No-op if the BVC isn't being presented.
  if (!self.bvcContainer)
    return;
  [base::mac::ObjCCast<TabGridViewController>(self.baseViewController)
      prepareForAppearance];
}

- (void)showTabSwitcher:(id<TabSwitcher>)tabSwitcher
             completion:(ProceduralBlock)completion {
  DCHECK(tabSwitcher);
  DCHECK_EQ([tabSwitcher viewController], self.baseViewController);
  // It's also expected that |tabSwitcher| will be |self.tabSwitcher|, but that
  // may not be worth a DCHECK?

  // If a BVC is currently being presented, dismiss it.  This will trigger any
  // necessary animations.
  if (self.bvcContainer) {
    self.bvcContainer.transitioningDelegate = self.transitionHandler;
    self.bvcContainer = nil;
    BOOL animated = !self.animationsDisabledForTesting;
    [self.baseViewController dismissViewControllerAnimated:animated
                                                completion:completion];
  } else {
    if (completion) {
      completion();
    }
  }
  // Record when the tab switcher is presented.
  // TODO(crbug.com/856965) : Rename metrics.
  base::RecordAction(base::UserMetricsAction("MobileTabSwitcherPresented"));
}

- (void)showTabViewController:(UIViewController*)viewController
                   completion:(ProceduralBlock)completion {
  DCHECK(viewController);

  // Record when the tab switcher is dismissed.
  // TODO(crbug.com/856965) : Rename metrics.
  base::RecordAction(base::UserMetricsAction("MobileTabSwitcherDismissed"));

  // If another BVC is already being presented, swap this one into the
  // container.
  if (self.bvcContainer) {
    self.bvcContainer.currentBVC = viewController;
    if (completion) {
      completion();
    }
    return;
  }

  self.bvcContainer = [[BVCContainerViewController alloc] init];
  self.bvcContainer.currentBVC = viewController;
  self.bvcContainer.transitioningDelegate = self.transitionHandler;
  BOOL animated = !self.animationsDisabledForTesting;
  // Never animate if the launch mask is in place.
  if (self.launchMaskView)
    animated = NO;

  // Extened |completion| to signal the tab switcher delegate
  // that the animated "tab switcher dismissal" (that is, presenting something
  // on top of the tab switcher) transition has completed.
  // Finally, the launch mask view should be removed.
  ProceduralBlock extendedCompletion = ^{
    [self.tabSwitcher.delegate
        tabSwitcherDismissTransitionDidEnd:self.tabSwitcher];
    if (completion) {
      completion();
    }
    [self.launchMaskView removeFromSuperview];
    self.launchMaskView = nil;
  };

  [self.baseViewController presentViewController:self.bvcContainer
                                        animated:animated
                                      completion:extendedCompletion];
}

#pragma mark - TabPresentationDelegate

- (void)showActiveTabInPage:(TabGridPage)page focusOmnibox:(BOOL)focusOmnibox {
  DCHECK(self.regularTabModel && self.incognitoTabModel);
  TabModel* activeTabModel;
  switch (page) {
    case TabGridPageIncognitoTabs:
      DCHECK_GT(self.incognitoTabModel.count, 0U);
      activeTabModel = self.incognitoTabModel;
      break;
    case TabGridPageRegularTabs:
      DCHECK_GT(self.regularTabModel.count, 0U);
      activeTabModel = self.regularTabModel;
      break;
    case TabGridPageRemoteTabs:
      NOTREACHED() << "It is invalid to have an active tab in remote tabs.";
      break;
  }
  // Trigger the transition through the TabSwitcher delegate. This will in turn
  // call back into this coordinator via the ViewControllerSwapping protocol.
  [self.tabSwitcher.delegate tabSwitcher:self.tabSwitcher
             shouldFinishWithActiveModel:activeTabModel
                            focusOmnibox:focusOmnibox];
}

#pragma mark - RecentTabsPresentationDelegate

- (void)dismissRecentTabs {
  // It is valid for tab grid to ignore this since recent tabs is embedded and
  // will not be dismissed.
}

- (void)showHistoryFromRecentTabs {
  // A history coordinator from main_controller won't work properly from the
  // tab grid. Using a local coordinator works better when hooked up with a
  // specialized URL loader and tab presentation delegate.
  self.historyCoordinator = [[HistoryCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                    browserState:self.regularTabModel.browserState];
  self.historyCoordinator.loader = self.URLLoader;
  self.historyCoordinator.presentationDelegate = self;
  self.historyCoordinator.dispatcher =
      static_cast<id<ApplicationCommands>>(self.dispatcher);
  [self.historyCoordinator start];
}

- (void)showActiveRegularTabFromRecentTabs {
  [self.tabSwitcher.delegate tabSwitcher:self.tabSwitcher
             shouldFinishWithActiveModel:self.regularTabModel
                            focusOmnibox:NO];
}

#pragma mark - HistoryPresentationDelegate

- (void)showActiveRegularTabFromHistory {
  [self.tabSwitcher.delegate tabSwitcher:self.tabSwitcher
             shouldFinishWithActiveModel:self.regularTabModel
                            focusOmnibox:NO];
}

- (void)showActiveIncognitoTabFromHistory {
  [self.tabSwitcher.delegate tabSwitcher:self.tabSwitcher
             shouldFinishWithActiveModel:self.incognitoTabModel
                            focusOmnibox:NO];
}

@end
