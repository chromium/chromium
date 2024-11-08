// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_COORDINATOR_AI_PROTOTYPING_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_COORDINATOR_AI_PROTOTYPING_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_mutator.h"

@protocol AIPrototypingConsumer;

namespace web {
class WebState;
}

// The mediator for the AI prototyping menu.
@interface AIPrototypingMediator : NSObject <AIPrototypingMutator>

// The consumer used to interact with the view controller.
@property(nonatomic, weak) id<AIPrototypingConsumer> consumer;

- (instancetype)initWithWebState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_COORDINATOR_AI_PROTOTYPING_MEDIATOR_H_
