// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUPERVISED_USER_COORDINATOR_PARENT_ACCESS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SUPERVISED_USER_COORDINATOR_PARENT_ACCESS_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/web/public/web_state.h"

class GURL;

@protocol ParentAccessConsumer;
@protocol ParentAccessMediatorDelegate;

// Mediator for ParentAccessCoordinator.
@interface ParentAccessMediator : NSObject

// Consumer to reflect model changes in the UI.
@property(nonatomic, weak) id<ParentAccessConsumer> consumer;

// Delegate for this mediator.
@property(nonatomic, weak) id<ParentAccessMediatorDelegate> delegate;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithWebState:(std::unique_ptr<web::WebState>)webState
                 parentAccessURL:(const GURL&)parentAccessURL
    NS_DESIGNATED_INITIALIZER;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_SUPERVISED_USER_COORDINATOR_PARENT_ACCESS_MEDIATOR_H_
