// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

#import "ios/chrome/browser/infobars/infobar_container_state_delegate.h"

namespace infobars {
class InfoBarManager;
}
namespace web {
class WebState;
}

@class TabModel;
@protocol ApplicationCommands;
@protocol InfobarPositioner;
@protocol SyncPresenter;

// Coordinator that owns and manages an InfoBarContainer.
@interface InfobarCoordinator : ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:
                                  (ios::ChromeBrowserState*)browserState
                                  tabModel:(TabModel*)tabModel
    NS_DESIGNATED_INITIALIZER;
;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
    NS_UNAVAILABLE;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:
                                  (ios::ChromeBrowserState*)browserState
    NS_UNAVAILABLE;

// The InfoBarContainer View.
- (UIView*)view;

// Updates the InfobarContainer according to the positioner information.
- (void)updateInfobarContainer;

// YES if an infobar is being presented for |webState|.
- (BOOL)isInfobarPresentingForWebState:(web::WebState*)webState;

// The dispatcher for this Coordinator.
@property(nonatomic, weak) id<ApplicationCommands> dispatcher;

// The delegate used to position the InfoBarContainer in the view.
@property(nonatomic, weak) id<InfobarPositioner> positioner;

// The SyncPresenter delegate for this Coordinator.
@property(nonatomic, weak) id<SyncPresenter> syncPresenter;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_COORDINATOR_H_
