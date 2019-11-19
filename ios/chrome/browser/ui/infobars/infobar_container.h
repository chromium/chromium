// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_CONTAINER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_CONTAINER_H_

#import <Foundation/Foundation.h>

@class InfobarCoordinator;

// Protocol for the InfobarCoordinators to communicate with the InfobarContainer
// Coordinator.
@protocol InfobarContainer

// Informs the InfobarContainerCoordinator that |infobarCoordinator| has
// finished presenting its banner(s).
- (void)childCoordinatorBannerFinishedPresented:
    (InfobarCoordinator*)infobarCoordinator;

// Informs the InfobarContainerCoordinator that |infobarCoordinator| has
// stopped.
- (void)childCoordinatorStopped:(InfobarCoordinator*)infobarCoordinator;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_CONTAINER_H_
