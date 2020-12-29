// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/infobar_container_coordinator.h"

#include <memory>

#import "base/mac/foundation_util.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_features.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_container.h"
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
                                           InfobarContainerConsumer,
                                           InfobarBannerContainer>

// ViewController of the Infobar currently being presented, can be nil.
@property(nonatomic, weak) UIViewController* infobarViewController;
// UIViewController that contains legacy Infobars.
@property(nonatomic, strong)
    LegacyInfobarContainerViewController* legacyContainerViewController;
// The mediator for this Coordinator.
@property(nonatomic, strong) InfobarContainerMediator* mediator;
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
// If YES, the banner is not shown, but the badge and subsequent modals will be.
@property(nonatomic, assign) BOOL skipBanner;
// YES if this container baseViewController is currently visible and part of
// the view hierarchy.
@property(nonatomic, assign, getter=isBaseViewControllerVisible)
    BOOL baseViewControllerVisible;

@end

@implementation InfobarContainerCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _infobarCoordinators = [NSMutableArray array];
    _infobarCoordinatorsToPresent = [NSMutableArray array];
  }
  return self;
}

- (void)start {
  DCHECK(self.positioner);

  // Creates the LegacyInfobarContainerVC.
  FullscreenController* controller =
      FullscreenController::FromBrowser(self.browser);
  LegacyInfobarContainerViewController* legacyContainer =
      [[LegacyInfobarContainerViewController alloc]
          initWithFullscreenController:controller];
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
          webStateList:self.browser->GetWebStateList()];

  self.mediator.syncPresenter = self.syncPresenter;

  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forSelector:@selector(displayModalInfobar:)];

  // TODO(crbug.com/1045047): Use HandlerForProtocol after commands protocol
  // clean up.
  [[UpgradeCenter sharedInstance]
      registerClient:self.mediator
         withHandler:static_cast<id<ApplicationCommands>>(
                         self.browser->GetCommandDispatcher())];
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

  [self.browser->GetCommandDispatcher() stopDispatchingToTarget:self];
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
  self.baseViewControllerVisible = YES;
  InfobarCoordinator* coordinator =
      [self.infobarCoordinatorsToPresent firstObject];
  if (coordinator)
    [self presentBannerForInfobarCoordinator:coordinator];
}

- (void)baseViewWillDisappear {
  self.baseViewControllerVisible = NO;
}

#pragma mark - ChromeCoordinator

- (MutableCoordinatorArray*)childCoordinators {
  return static_cast<MutableCoordinatorArray*>(self.infobarCoordinators);
}

#pragma mark - Accessors

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

- (void)addInfoBarWithDelegate:(id<InfobarUIDelegate>)infoBarDelegate
                    skipBanner:(BOOL)skipBanner {
  DCHECK(IsInfobarUIRebootEnabled());
  InfobarCoordinator* infobarCoordinator =
      static_cast<InfobarCoordinator*>(infoBarDelegate);

  [self.infobarCoordinators addObject:infobarCoordinator];

  self.skipBanner = skipBanner;

  // Configure the Coordinator and try to present the Banner afterwards.
  [infobarCoordinator start];
  // Only set the infobarCoordinator's badgeDelegate if it supports a badge. Not
  // doing so might cause undefined behavior since no badge was added.
  if (infobarCoordinator.hasBadge)
    infobarCoordinator.badgeDelegate = self.mediator;
  infobarCoordinator.browser = self.browser;
  infobarCoordinator.webState =
      self.browser->GetWebStateList()->GetActiveWebState();
  infobarCoordinator.baseViewController = self.baseViewController;
  infobarCoordinator.bannerViewController.infobarBannerContainer = self;
  // TODO(crbug.com/1045047): Use HandlerForProtocol after commands protocol
  // clean up.
  infobarCoordinator.handler = static_cast<id<ApplicationCommands>>(
      self.browser->GetCommandDispatcher());
  infobarCoordinator.infobarContainer = self;
  [self presentBannerForInfobarCoordinator:infobarCoordinator];
}

- (void)infobarManagerWillChange {
  if (IsInfobarUIRebootEnabled()) {
    [self dismissInfobarBannerAnimated:NO completion:nil];
  }
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
  [self presentNextBannerInQueue];
}

- (void)childCoordinatorStopped:(InfobarCoordinator*)infobarCoordinator {
  [self.infobarCoordinators removeObject:infobarCoordinator];
  // Also remove it from |infobarCoordinatorsToPresent| in case it was queued
  // for a presentation.
  [self.infobarCoordinatorsToPresent removeObject:infobarCoordinator];
}

#pragma mark InfobarBannerContainerDelegate

- (void)infobarBannerFinishedPresenting {
  [self presentNextBannerInQueue];
}

- (BOOL)shouldDismissBanner {
  return !self.baseViewControllerVisible;
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

// Presents the Banner for the next InfobarCoordinator in queue, if any.
- (void)presentNextBannerInQueue {
  InfobarCoordinator* coordinator =
      [self.infobarCoordinatorsToPresent firstObject];
  if (coordinator)
    [self presentBannerForInfobarCoordinator:coordinator];
}

// Presents the infobarBanner for |infobarCoordinator| if possible, if not it
// queues the banner in self.infobarCoordinatorsToPresent for future
// presentation.
- (void)presentBannerForInfobarCoordinator:
    (InfobarCoordinator*)infobarCoordinator {
  // Each banner can only be presented once.
  if (infobarCoordinator.bannerWasPresented || self.skipBanner)
    return;

  // If a banner is being presented or base VC is not in window, queue it then
  // return.
  if (!(self.infobarBannerState ==
        InfobarBannerPresentationState::NotPresented) ||
      (!self.baseViewController.view.window) ||
      (!self.baseViewControllerVisible)) {
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
