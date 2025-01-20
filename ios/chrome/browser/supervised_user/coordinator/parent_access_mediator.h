// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUPERVISED_USER_COORDINATOR_PARENT_ACCESS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SUPERVISED_USER_COORDINATOR_PARENT_ACCESS_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/supervised_user/ui/parent_access_consumer.h"
#import "ios/web/public/web_state.h"

// Mediator for ParentAccessCoordinator.
@interface ParentAccessMediator : NSObject

@property(nonatomic, weak) id<ParentAccessConsumer> consumer;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithWebState:(std::unique_ptr<web::WebState>)webState
    NS_DESIGNATED_INITIALIZER;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_SUPERVISED_USER_COORDINATOR_PARENT_ACCESS_MEDIATOR_H_
