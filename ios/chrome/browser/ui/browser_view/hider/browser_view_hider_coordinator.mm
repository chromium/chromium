// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_view/hider/browser_view_hider_coordinator.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/overlays/public/overlay_presenter.h"
#import "ios/chrome/browser/ui/browser_view/hider/browser_view_hider_view_controller.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_steady_view_mediator.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_coordinator_delegate.h"
#import "ios/chrome/browser/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BrowserViewHiderCoordinator ()

@property(nonatomic, strong) BrowserViewHiderViewController* viewController;

@property(nonatomic, strong) LocationBarSteadyViewMediator* mediator;

@end

@implementation BrowserViewHiderCoordinator

- (void)start {
  self.viewController = [[BrowserViewHiderViewController alloc] init];
  self.viewController.incognito =
      self.browser->GetBrowserState()->IsOffTheRecord();

  [self.baseViewController addChildViewController:self.viewController];
  [self.baseViewController.view addSubview:self.viewController.view];
  self.viewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraintsToSides(
      self.baseViewController.view, self.viewController.view,
      LayoutSides::kBottom | LayoutSides::kLeading | LayoutSides::kTrailing);

  NamedGuide* primaryToolbarGuide =
      [NamedGuide guideWithName:kPrimaryToolbarGuide
                           view:self.baseViewController.view];
  [self.viewController.view.topAnchor
      constraintEqualToAnchor:primaryToolbarGuide.topAnchor]
      .active = YES;
  [self.viewController didMoveToParentViewController:self.baseViewController];

  self.mediator = [[LocationBarSteadyViewMediator alloc]
      initWithLocationBarModel:self.locationBarModel];
  self.mediator.webStateList = self.browser->GetWebStateList();
  self.mediator.webContentAreaOverlayPresenter = OverlayPresenter::FromBrowser(
      self.browser, OverlayModality::kWebContentArea);
  self.mediator.consumer = self.viewController;
}

- (void)stop {
  self.mediator.consumer = nil;
  [self.mediator disconnect];

  self.mediator = nil;

  [self.viewController removeFromParentViewController];
  [self.viewController.view removeFromSuperview];

  self.viewController = nil;
}

- (id<ViewRevealingAnimatee>)animatee {
  return self.viewController;
}

- (ViewRevealingVerticalPanHandler*)panGestureHandler {
  return self.viewController.panGestureHandler;
}

- (void)setPanGestureHandler:
    (ViewRevealingVerticalPanHandler*)panGestureHandler {
  self.viewController.panGestureHandler = panGestureHandler;
}

@end
