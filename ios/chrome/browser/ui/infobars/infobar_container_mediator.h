// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_CONTAINER_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_CONTAINER_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/infobars/infobar_badge_ui_delegate.h"
#include "ios/chrome/browser/upgrade/upgrade_center.h"

@protocol InfobarContainerConsumer;
@protocol SigninPresenter;
@protocol SyncPresenter;
class WebStateList;

@interface InfobarContainerMediator
    : NSObject <InfobarBadgeUIDelegate, UpgradeCenterClient>

// Designated initializer. None of the parameters are retained.
// TODO(crbug.com/927064): The legacy consumer won't be needed once
// legacyInfobars are no longer supported.
- (instancetype)initWithConsumer:(id<InfobarContainerConsumer>)consumer
                  legacyConsumer:(id<InfobarContainerConsumer>)legacyConsumer
                    webStateList:(WebStateList*)webStateList
    NS_DESIGNATED_INITIALIZER;
;

- (instancetype)init NS_UNAVAILABLE;

// The SyncPresenter delegate for this Mediator.
@property(nonatomic, weak) id<SyncPresenter> syncPresenter;

// The SigninPresenter delegate for this Mediator.
@property(nonatomic, weak) id<SigninPresenter> signinPresenter;

// Stops observing all objects.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_CONTAINER_MEDIATOR_H_
