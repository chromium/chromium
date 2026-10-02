// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_COORDINATOR_TTC_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_COORDINATOR_TTC_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_mutator.h"

class TTCKeyedService;
@protocol TTCConsumer;

// Mediator driving the TTC UI by bridging TTCKeyedService and
// active TTCSessionController events to the consumer.
@interface TTCMediator : NSObject <TTCMutator>

// Consumer receiving UI updates from the mediator.
@property(nonatomic, weak) id<TTCConsumer> consumer;

// Designated initializer injecting the profile's TTC keyed service.
- (instancetype)initWithTTCService:(TTCKeyedService*)ttcService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects active observations and cleans up references. Does not terminate
// the underlying profile session.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_COORDINATOR_TTC_MEDIATOR_H_
