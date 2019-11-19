// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/infobar_container_coordinator.h"

#include <memory>

#import "base/mac/foundation_util.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller_factory.h"
#import "ios/chrome/browser/ui/infobars/coordinators/infobar_coordinator.h"
#import "ios/chrome/browser/ui/infobars/infobar_container.h"
#import "ios/chrome/browser/ui/infobars/infobar_container_consumer.h"
#include "ios/chrome/browser/ui/infobars/infobar_container_mediator.h"
#import "ios/chrome/browser/ui/infobars/infobar_feature.h"
#import "ios/chrome/browser/ui/infobars/infobar_positioner.h"
#include "ios/chrome/browser/ui/infobars/legacy_infobar_container_view_controller.h"
#include "ios/chrome/browser/upgrade/upgrade_center.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface InfobarContainerCoordinator () <InfobarContainer,
                                           InfobarContainerConsumer>

@property(nonatomic, assign) WebStateList* webStateList;

// ViewController of the Infobar currently being presented, can be nil.
@property(nonatomic, weak) UIViewController* infobarViewController;
// UIViewController that contains legacy Infobars.
@property(nonatomic, strong)
    LegacyInfobarContainerViewController* legacyContainerViewController;
// The mediator for this Coordinator.
@property(nonatomic, strong) InfobarContainerMediator* mediator;
// The dispatcher for this Coordinator.
@property(nonatomic, weak) id<ApplicationCommands> dispatcher;
// If YES the legacyContainer Fullscreen support will be disabled.
// TODO(crbug.com/927064): Remove this once the legacy container is no longer
// needed.
@property(nonatomic, assign) BOOL legacyContainerFullscrenSupportDisabled;
// infobarCoordinators holds all InfobarCoordinators this ContainerCoordinator
// can display.
@property(nonatomic, strong)
    NSMutableArray<InfobarCoordinator*>* infobarCoordinators;
// Array of Coordinators which banners haven't been presented yet. Once a
// Coordinator banner is presented it should be removed from this Array. If
// empty then it means there are no banners queued to be presented.
@property(nonatomic, strong)
    NSMutableArray<InfobarCoordinator*>* infobarCoordinatorsToPresent;

@end

@implementation InfobarContainerCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:
                                  (ios::ChromeBrowserState*)browserState
                              webStateList:(WebStateList*)webStateList {
  self = [super initWithBaseViewController:viewController
                              browserState:browserState];
  if (self) {
    _webStateList = webStateList;
    _infobarCoordinators = [NSMutableArray array];
    _infobarCoordinatorsToPresent = [NSMutableArray array];
  }
  return self;
}

- (void)start {
  DCHECK(self.positioner);
  DCHECK(self.dispatcher);

  // Creates the LegacyInfobarContainerVC.
  LegacyInfobarContainerViewController* legacyContainer =
      [[LegacyInfobarContainerViewController alloc]
          initWithFullscreenController:
              FullscreenControllerFactory::GetInstance()->GetForBrowserState(
                  self.browserState)];
  [self.baseViewController addChildViewController:legacyContainer];
  // TODO(crbug.com/892376): Shouldn't modify the BaseVC hierarchy, BVC
  // needs to handle this.
  [self.baseViewController.view insertSubview:legacyContainer.view
                                 aboveSubview:self.positioner.parentView];
  [legacyContainer didMoveToParentViewController:self.baseViewController];
  legacyContainer.positioner = self.positioner;
  legacyContainer.disableFullscreenSupport =
      self.legacyContainerFullscrenSupportDisabled;
  self.legacyContainerViewController = legacyContainer;

  // Creates the mediator using both consumers.
  self.mediator = [[InfobarContainerMediator alloc]
      initWithConsumer:self
        legacyConsumer:self.legacyContainerViewController
          webStateList:self.webStateList];

  self.mediator.syncPresenter = self.syncPresenter;

  [[UpgradeCenter sharedInstance] registerClient:self.mediator
                                  withDispatcher:self.dispatcher];
}

- (void)stop {
  [[UpgradeCenter sharedInstance] unregisterClient:self.mediator];
  [self.mediator disconnect];
  if (!self.legacyContainerViewController)
    return;

  [self.legacyContainerViewController willMoveToParentViewController:nil];
  [self.legacyContainerViewController.view removeFromSuperview];
  [self.legacyContainerViewController removeFromParentViewController];
  self.legacyContainerViewController = nil;
}

#pragma mark - Public Interface

- (void)hideContainer:(BOOL)hidden {
  [self.legacyContainerViewController.view setHidden:hidden];
  [self.infobarViewController.view setHidden:hidden];
}

- (UIView*)legacyContainerView {
  return self.legacyContainerViewController.view;
}

- (void)updateInfobarContainer {
  // TODO(crbug.com/927064): No need to update the non legacy container since
  // updateLayoutAnimated is NO-OP.
  [self.legacyContainerViewController updateLayoutAnimated:NO];
}

- (BOOL)isInfobarPresentingForWebState:(web::WebState*)webState {
  infobars::InfoBarManager* infoBarManager =
      InfoBarManagerImpl::FromWebState(webState);
  if (infoBarManager->infobar_count() > 0) {
    return YES;
  }
  return NO;
}

- (void)dismissInfobarBannerAnimated:(BOOL)animated
                          completion:(void (^)())completion {
  DCHECK(IsInfobarUIRebootEnabled());

  for (InfobarCoordinator* infobarCoordinator in self.infobarCoordinators) {
    if (infobarCoordinator.infobarBannerState !=
        InfobarBannerPresentationState::NotPresented) {
      // Since only one Banner can be presented at any time, dismiss it.
      [infobarCoordinator dismissInfobarBannerAnimated:animated
                                            completion:completion];
      return;
    }
  }
  // If no banner was presented make sure the completion block still runs.
  if (completion)
    completion();
}

- (void)baseViewDidAppear {
  InfobarCoordinator* coordinator =
      [self.infobarCoordinatorsToPresent firstObject];
  if (coordinator)
    [self presentBannerForInfobarCoordinator:coordinator];
}

#pragma mark - ChromeCoordinator

- (MutableCoordinatorArray*)childCoordinators {
  return static_cast<MutableCoordinatorArray*>(self.infobarCoordinators);
}

#pragma mark - Accessors

- (void)setCommandDispatcher:(CommandDispatcher*)commandDispatcher {
  if (commandDispatcher == self.commandDispatcher) {
    return;
  }

  if (self.commandDispatcher) {
    [self.commandDispatcher stopDispatchingToTarget:self];
  }

  [commandDispatcher startDispatchingToTarget:self
                                  forSelector:@selector(displayModalInfobar:)];
  _commandDispatcher = commandDispatcher;
  self.dispatcher = static_cast<id<ApplicationCommands>>(_commandDispatcher);
}

- (InfobarBannerPresentationState)infobarBannerState {
  DCHECK(IsInfobarUIRebootEnabled());
  for (InfobarCoordinator* infobarCoordinator in self.infobarCoordinators) {
    if (infobarCoordinator.infobarBannerState !=
        InfobarBannerPresentationState::NotPresented) {
      // Since only one Banner can be presented at any time, early return.
      return infobarCoordinator.infobarBannerState;
    }
  }
  return InfobarBannerPresentationState::NotPresented;
}

#pragma mark - Protocols

#pragma mark InfobarContainerConsumer

- (void)addInfoBarWithDelegate:(id<InfobarUIDelegate>)infoBarDelegate {
  DCHECK(IsInfobarUIRebootEnabled());
  InfobarCoordinator* infobarCoordinator =
      static_cast<InfobarCoordinator*>(infoBarDelegate);

  [self.infobarCoordinators addObject:infobarCoordinator];

  // Configure the Coordinator and try to present the Banner afterwards.
  [infobarCoordinator start];
  // Only set the infobarCoordinator's badgeDelegate if it supports a badge. Not
  // doing so might cause undefined behavior since no badge was added.
  if (infobarCoordinator.hasBadge)
    infobarCoordinator.badgeDelegate = self.mediator;
  infobarCoordinator.browserState = self.browserState;
  infobarCoordinator.webState = self.webStateList->GetActiveWebState();
  infobarCoordinator.baseViewController = self.baseViewController;
  infobarCoordinator.dispatcher = self.dispatcher;
  infobarCoordinator.infobarContainer = self;
  [self presentBannerForInfobarCoordinator:infobarCoordinator];
}

- (void)infobarManagerWillChange {
  self.infobarCoordinators = [NSMutableArray array];
  self.infobarCoordinatorsToPresent = [NSMutableArray array];
}

- (void)setUserInteractionEnabled:(BOOL)enabled {
  [self.infobarViewController.view setUserInteractionEnabled:enabled];
}

- (void)updateLayoutAnimated:(BOOL)animated {
  DCHECK(IsInfobarUIRebootEnabled());
  // TODO(crbug.com/927064): NO-OP - This shouldn't be needed in the new UI
  // since we use autolayout for the contained Infobars.
}

#pragma mark InfobarContainer

- (void)childCoordinatorBannerFinishedPresented:
    (InfobarCoordinator*)infobarCoordinator {
  InfobarCoordinator* coordinator =
      [self.infobarCoordinatorsToPresent firstObject];
  if (coordinator)
    [self presentBannerForInfobarCoordinator:coordinator];
}

- (void)childCoordinatorStopped:(InfobarCoordinator*)infobarCoordinator {
  [self.infobarCoordinators removeObject:infobarCoordinator];
  // Also remove it from |infobarCoordinatorsToPresent| in case it was queued
  // for a presentation.
  [self.infobarCoordinatorsToPresent removeObject:infobarCoordinator];
}

#pragma mark InfobarCommands

- (void)displayModalInfobar:(InfobarType)infobarType {
  InfobarCoordinator* infobarCoordinator =
      [self infobarCoordinatorForInfobarTye:infobarType];
  DCHECK(infobarCoordinator);
  DCHECK(infobarCoordinator.infobarType != InfobarType::kInfobarTypeConfirm);
  [infobarCoordinator presentInfobarModal];
}

#pragma mark - Private

// Presents the infobarBanner for |infobarCoordinator| if possible, if not it
// queues the banner in self.infobarCoordinatorsToPresent for future
// presentation.
- (void)presentBannerForInfobarCoordinator:
    (InfobarCoordinator*)infobarCoordinator {
  // Each banner can only be presented once.
  if (infobarCoordinator.bannerWasPresented)
    return;

  // If a banner is being presented or base VC is not in window, queue it then
  // return.
  if (!(self.infobarBannerState ==
        InfobarBannerPresentationState::NotPresented) ||
      (!self.baseViewController.view.window)) {
    [self queueInfobarCoordinatorForPresentation:infobarCoordinator];
    return;
  }

  // Present Banner.
  [infobarCoordinator presentInfobarBannerAnimated:YES completion:nil];
  self.infobarViewController = infobarCoordinator.bannerViewController;
  [self.infobarCoordinatorsToPresent removeObject:infobarCoordinator];
}

// Returns the InfobarCoordinator for |infobarType|. If there's more than one
// (e.g. kInfobarTypeConfirm) it will return the first one that was added. If no
// InfobarCoordinator returns nil.
- (InfobarCoordinator*)infobarCoordinatorForInfobarTye:
    (InfobarType)infobarType {
  for (InfobarCoordinator* coordinator in self.infobarCoordinators) {
    if (coordinator.infobarType == infobarType) {
      return coordinator;
    }
  }
  return nil;
}

// Queues an InfobarBanner for presentation. If it has already been queued it
// won't be added again.
- (void)queueInfobarCoordinatorForPresentation:
    (InfobarCoordinator*)coordinator {
  if (![self.infobarCoordinatorsToPresent containsObject:coordinator]) {
    if (coordinator.highPriorityPresentation) {
      [self.infobarCoordinatorsToPresent insertObject:coordinator atIndex:0];
    } else {
      [self.infobarCoordinatorsToPresent addObject:coordinator];
    }
  }
}

@end
