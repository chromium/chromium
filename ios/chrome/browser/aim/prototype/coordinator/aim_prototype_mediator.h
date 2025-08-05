// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_PROTOTYPE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_PROTOTYPE_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_consumer.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_mutator.h"

@class AIMPrototypeMediator;
class TemplateURLService;
class UrlLoadingBrowserAgent;

// Delegate for the AIM prototype mediator.
@protocol AIMPrototypeMediatorDelegate
- (void)dismissAimPrototype;
@end

// Mediator for the AIM prototype.
@interface AIMPrototypeMediator : NSObject <AIMPrototypeMutator>

@property(nonatomic, weak) id<AIMPrototypeConsumer> consumer;
@property(nonatomic, weak) id<AIMPrototypeMediatorDelegate> delegate;

- (instancetype)initWithUrlLoadingBrowserAgent:
                    (UrlLoadingBrowserAgent*)urlLoadingBrowserAgent
                            templateURLService:
                                (TemplateURLService*)templateURLService;
- (void)processImage:(UIImage*)image;
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_PROTOTYPE_MEDIATOR_H_
