// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_COORDINATOR_TTC_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_COORDINATOR_TTC_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol AIPrototypingViewControllerProtocol;

// Coordinator managing the TalkToChrome feature.
@interface TTCCoordinator : ChromeCoordinator

// The view controller managed by this coordinator.
@property(nonatomic, readonly, strong)
    UIViewController<AIPrototypingViewControllerProtocol>* viewController;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_COORDINATOR_TTC_COORDINATOR_H_
