// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_PROTOTYPE_NAVIGATION_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_PROTOTYPE_NAVIGATION_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/aim/prototype/coordinator/aim_prototype_url_loader.h"
#import "ios/web/public/web_state.h"

@protocol AIMPrototypeNavigationConsumer;
@class AIMPrototypeNavigationMediator;
class UrlLoadingBrowserAgent;

// Delegate for the AIM prototype navigation mediator.
@protocol AIMPrototypeNavigationMediatorDelegate

// Called when the navigation mediator requires the AIM prototype to be
// dismissed.
- (void)navigationMediatorDidFinish:
    (AIMPrototypeNavigationMediator*)navigationMediator;

@end

// A mediator for the AIM prototype's navigation.
@interface AIMPrototypeNavigationMediator : NSObject <AIMPrototypeURLLoader>

// The consumer for this mediator.
@property(nonatomic, weak) id<AIMPrototypeNavigationConsumer> consumer;

// The delegate for this mediator.
@property(nonatomic, weak) id<AIMPrototypeNavigationMediatorDelegate> delegate;

// Initializes the mediator.
- (instancetype)initWithUrlLoadingBrowserAgent:
                    (UrlLoadingBrowserAgent*)urlLoadingBrowserAgent
                                webStateParams:
                                    (const web::WebState::CreateParams&)params;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_PROTOTYPE_NAVIGATION_MEDIATOR_H_
