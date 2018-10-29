// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/infobar_coordinator.h"

#include <memory>

#include "ios/chrome/browser/infobars/infobar_container_delegate_ios.h"
#include "ios/chrome/browser/infobars/infobar_container_ios.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_model_observer.h"
#import "ios/chrome/browser/ui/authentication/re_signin_infobar_delegate.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#include "ios/chrome/browser/ui/infobars/infobar_container_view_controller.h"
#import "ios/chrome/browser/ui/infobars/infobar_positioner.h"
#import "ios/chrome/browser/ui/settings/sync_utils/sync_util.h"
#import "ios/chrome/browser/ui/signin_interaction/public/signin_presenter.h"
#include "ios/chrome/browser/upgrade/upgrade_center.h"
#import "ios/chrome/browser/web/tab_id_tab_helper.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface InfobarCoordinator ()<TabModelObserver,
                                 UpgradeCenterClient,
                                 SigninPresenter> {
  // Bridge class to deliver container change notifications.
  std::unique_ptr<InfoBarContainerDelegateIOS> _infoBarContainerDelegate;

  // A single infobar container handles all infobars in all tabs. It keeps
  // track of infobars for current tab (accessed via infobar helper of
  // the current tab).
  std::unique_ptr<InfoBarContainerIOS> _infoBarContainer;
}
@property(nonatomic, assign) TabModel* tabModel;

// UIViewController that contains Infobars.
@property(nonatomic, strong)
    InfobarContainerViewController* containerViewController;

@end

@implementation InfobarCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:
                                  (ios::ChromeBrowserState*)browserState
                                  tabModel:(TabModel*)tabModel {
  self = [super initWithBaseViewController:viewController
                              browserState:browserState];
  if (self) {
    _tabModel = tabModel;
    [_tabModel addObserver:self];
    _containerViewController = [[InfobarContainerViewController alloc] init];
    _infoBarContainerDelegate.reset(
        new InfoBarContainerDelegateIOS(_containerViewController));
    _infoBarContainer.reset(new InfoBarContainerIOS(
        _infoBarContainerDelegate.get(), _containerViewController));
  }
  return self;
}

- (void)start {
  DCHECK(self.positioner);
  DCHECK(self.dispatcher);

  [self.baseViewController addChildViewController:self.containerViewController];
  // TODO(crbug.com/892376): We shouldn't modify the BaseVC hierarchy, BVC needs
  // to handle this.
  [self.baseViewController.view insertSubview:self.containerViewController.view
                                 aboveSubview:self.positioner.parentView];
  [self.containerViewController
      didMoveToParentViewController:self.baseViewController];
  self.containerViewController.positioner = self.positioner;

  infobars::InfoBarManager* infoBarManager = nullptr;
  if (self.tabModel.currentTab) {
    DCHECK(self.tabModel.currentTab.webState);
    infoBarManager =
        InfoBarManagerImpl::FromWebState(self.tabModel.currentTab.webState);
  }
  _infoBarContainer->ChangeInfoBarManager(infoBarManager);

  [[UpgradeCenter sharedInstance] registerClient:self
                                  withDispatcher:self.dispatcher];
}

- (void)stop {
  [self.tabModel removeObserver:self];
  [[UpgradeCenter sharedInstance] unregisterClient:self];
}

#pragma mark - Public Interface

- (UIView*)view {
  return self.containerViewController.view;
}

- (void)updateInfobarContainer {
  [self.containerViewController infoBarContainerStateDidChangeAnimated:NO];
}

- (BOOL)isInfobarPresentingForWebState:(web::WebState*)webState {
  infobars::InfoBarManager* infoBarManager =
      InfoBarManagerImpl::FromWebState(webState);
  if (infoBarManager->infobar_count() > 0) {
    return YES;
  }
  return NO;
}

#pragma mark - TabModelObserver methods

- (void)tabModel:(TabModel*)model
    didChangeActiveTab:(Tab*)newTab
           previousTab:(Tab*)previousTab
               atIndex:(NSUInteger)index {
  DCHECK(newTab.webState);
  infobars::InfoBarManager* infoBarManager =
      InfoBarManagerImpl::FromWebState(newTab.webState);
  _infoBarContainer->ChangeInfoBarManager(infoBarManager);
}

- (void)tabModel:(TabModel*)model
    newTabWillOpen:(Tab*)tab
      inBackground:(BOOL)background {
  // When adding new tabs, check what kind of reminder infobar should
  // be added to the new tab. Try to add only one of them.
  // This check is done when a new tab is added either through the Tools Menu
  // "New Tab", through a long press on the Tab Switcher button "New Tab", and
  // through creating a New Tab from the Tab Switcher. This method is called
  // after a new tab has added and finished initial navigation. If this is added
  // earlier, the initial navigation may end up clearing the infobar(s) that are
  // just added.
  web::WebState* webState = tab.webState;
  DCHECK(webState);

  infobars::InfoBarManager* infoBarManager =
      InfoBarManagerImpl::FromWebState(webState);
  [[UpgradeCenter sharedInstance] addInfoBarToManager:infoBarManager
                                             forTabId:[tab tabId]];

  if (!ReSignInInfoBarDelegate::Create(self.browserState, tab,
                                       self /* id<SigninPresenter> */)) {
    DisplaySyncErrors(self.browserState, tab,
                      self.syncPresenter /* id<SyncPresenter> */);
  }
}

- (void)tabModel:(TabModel*)model
    didReplaceTab:(Tab*)oldTab
          withTab:(Tab*)newTab
          atIndex:(NSUInteger)index {
  infobars::InfoBarManager* infoBarManager = nullptr;
  if (newTab) {
    DCHECK(newTab.webState);
    infoBarManager = InfoBarManagerImpl::FromWebState(newTab.webState);
  }
  _infoBarContainer->ChangeInfoBarManager(infoBarManager);
}

#pragma mark - SigninPresenter

- (void)showSignin:(ShowSigninCommand*)command {
  [self.dispatcher showSignin:command
           baseViewController:self.baseViewController];
}

#pragma mark - UpgradeCenterClient

- (void)showUpgrade:(UpgradeCenter*)center {
  if (!self.tabModel)
    return;

  // Add an infobar on all the open tabs.
  DCHECK(self.tabModel.webStateList);
  WebStateList* webStateList = self.tabModel.webStateList;
  for (int index = 0; index < webStateList->count(); ++index) {
    web::WebState* webState = webStateList->GetWebStateAt(index);
    NSString* tabId = TabIdTabHelper::FromWebState(webState)->tab_id();
    infobars::InfoBarManager* infoBarManager =
        InfoBarManagerImpl::FromWebState(webState);
    DCHECK(infoBarManager);
    [center addInfoBarToManager:infoBarManager forTabId:tabId];
  }
}

@end
