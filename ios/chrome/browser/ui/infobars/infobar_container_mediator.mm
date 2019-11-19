// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/infobar_container_mediator.h"

#include "ios/chrome/browser/infobars/infobar_badge_tab_helper.h"
#include "ios/chrome/browser/infobars/infobar_container_ios.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/ui/infobars/infobar_container_consumer.h"
#import "ios/chrome/browser/ui/infobars/infobar_feature.h"
#include "ios/chrome/browser/upgrade/upgrade_center.h"
#import "ios/chrome/browser/web/tab_id_tab_helper.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface InfobarContainerMediator () <WebStateListObserving> {
  // A single infobar container handles all infobars in all tabs. It keeps
  // track of infobars for current Webstate.
  std::unique_ptr<InfoBarContainerIOS> _infoBarContainer;
  // Bridge class to deliver webStateList notifications.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
}

// The WebStateList that this mediator listens for any changes on its Webstates.
@property(nonatomic, assign) WebStateList* webStateList;

@end

@implementation InfobarContainerMediator

#pragma mark - Public Interface

- (instancetype)initWithConsumer:(id<InfobarContainerConsumer>)consumer
                  legacyConsumer:(id<InfobarContainerConsumer>)legacyConsumer
                    webStateList:(WebStateList*)webStateList {
  self = [super init];
  if (self) {
    _webStateList = webStateList;

    _infoBarContainer.reset(new InfoBarContainerIOS(consumer, legacyConsumer));
    infobars::InfoBarManager* infoBarManager = nullptr;
    if (_webStateList->GetActiveWebState()) {
      infoBarManager =
          InfoBarManagerImpl::FromWebState(_webStateList->GetActiveWebState());
    }
    _infoBarContainer->ChangeInfoBarManager(infoBarManager);

    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _webStateList->AddObserver(_webStateListObserver.get());
  }
  return self;
}

- (void)dealloc {
  [self disconnect];
}

- (void)disconnect {
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
    _webStateListObserver.reset();
    _webStateList = nullptr;
  }
}

#pragma mark - WebStateListObserver

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(int)reason {
  DCHECK_EQ(_webStateList, webStateList);
  if (!newWebState)
    return;
  infobars::InfoBarManager* infoBarManager =
      InfoBarManagerImpl::FromWebState(newWebState);
  _infoBarContainer->ChangeInfoBarManager(infoBarManager);
}

- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
          withWebState:(web::WebState*)newWebState
               atIndex:(int)atIndex {
  DCHECK_EQ(_webStateList, webStateList);
  infobars::InfoBarManager* infoBarManager = nullptr;
  if (newWebState) {
    infoBarManager = InfoBarManagerImpl::FromWebState(newWebState);
  }
  _infoBarContainer->ChangeInfoBarManager(infoBarManager);
}

#pragma mark - InfobarBadgeUIDelegate

- (void)infobarWasAccepted:(InfobarType)infobarType
               forWebState:(web::WebState*)webState {
    DCHECK(webState);
    DCHECK_EQ(webState, self.webStateList->GetActiveWebState());
    InfobarBadgeTabHelper* infobarBadgeTabHelper =
        InfobarBadgeTabHelper::FromWebState(webState);
    DCHECK(infobarBadgeTabHelper);
    infobarBadgeTabHelper->UpdateBadgeForInfobarAccepted(infobarType);
}

- (void)infobarWasReverted:(InfobarType)infobarType
               forWebState:(web::WebState*)webState {
  DCHECK(webState);
  DCHECK_EQ(webState, self.webStateList->GetActiveWebState());
  InfobarBadgeTabHelper* infobarBadgeTabHelper =
      InfobarBadgeTabHelper::FromWebState(webState);
  DCHECK(infobarBadgeTabHelper);
  infobarBadgeTabHelper->UpdateBadgeForInfobarReverted(infobarType);
}

- (void)infobarBannerWasPresented:(InfobarType)infobarType
                      forWebState:(web::WebState*)webState {
  DCHECK(webState);
  DCHECK_EQ(webState, self.webStateList->GetActiveWebState());
  InfobarBadgeTabHelper* infobarBadgeTabHelper =
      InfobarBadgeTabHelper::FromWebState(webState);
  DCHECK(infobarBadgeTabHelper);
  infobarBadgeTabHelper->UpdateBadgeForInfobarBannerPresented(infobarType);
}

- (void)infobarBannerWasDismissed:(InfobarType)infobarType
                      forWebState:(web::WebState*)webState {
  DCHECK(webState);
  // If the banner is dismissed because of a change in WebState, |webState| will
  // not match the AcitveWebStaate, so don't DCHECK.
  InfobarBadgeTabHelper* infobarBadgeTabHelper =
      InfobarBadgeTabHelper::FromWebState(webState);
  DCHECK(infobarBadgeTabHelper);
  infobarBadgeTabHelper->UpdateBadgeForInfobarBannerDismissed(infobarType);
}

#pragma mark - UpgradeCenterClient

- (void)showUpgrade:(UpgradeCenter*)center {
  DCHECK(self.webStateList);
  // Add an infobar on all the open tabs.
  for (int index = 0; index < self.webStateList->count(); ++index) {
    web::WebState* webState = self.webStateList->GetWebStateAt(index);
    NSString* tabID = TabIdTabHelper::FromWebState(webState)->tab_id();
    infobars::InfoBarManager* infoBarManager =
        InfoBarManagerImpl::FromWebState(webState);
    DCHECK(infoBarManager);
    [center addInfoBarToManager:infoBarManager forTabId:tabID];
  }
}

@end
