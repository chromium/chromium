// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_COORDINATOR_SEND_TAB_TO_SELF_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_COORDINATOR_SEND_TAB_TO_SELF_MEDIATOR_H_

#import <Foundation/Foundation.h>

class AuthenticationService;

@protocol SendTabToSelfMediatorDelegate;
namespace signin {
class IdentityManager;
}  // namespace signin

// Mediator for the "send-tab-to-self" feature.
@interface SendTabToSelfMediator : NSObject

// The delegate of this mediator.
@property(nonatomic, weak) id<SendTabToSelfMediatorDelegate> delegate;

- (instancetype)initWithAuthenticationService:(AuthenticationService*)service
                              identityManager:
                                  (signin::IdentityManager*)identityManager
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_COORDINATOR_SEND_TAB_TO_SELF_MEDIATOR_H_
