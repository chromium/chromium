// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WELCOME_BACK_COORDINATOR_WELCOME_BACK_MEDIATOR_H_
#define IOS_CHROME_BROWSER_WELCOME_BACK_COORDINATOR_WELCOME_BACK_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

class AuthenticationService;
class ChromeAccountManagerService;
@protocol WelcomeBackScreenConsumer;

// Mediator for the Welcome Back screen.
@interface WelcomeBackMediator : NSObject

// The consumer for this object. Setting it will update the consumer with
// the current data.
@property(nonatomic, weak) id<WelcomeBackScreenConsumer> consumer;

// The designated initializer.
- (instancetype)initWithAuthenticationService:
                    (AuthenticationService*)authenticationService
                        accountManagerService:
                            (ChromeAccountManagerService*)accountManagerService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_WELCOME_BACK_COORDINATOR_WELCOME_BACK_MEDIATOR_H_
