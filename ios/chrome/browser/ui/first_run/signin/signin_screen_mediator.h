// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_SIGNIN_SIGNIN_SCREEN_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_SIGNIN_SIGNIN_SCREEN_MEDIATOR_H_

#import <Foundation/Foundation.h>

class AuthenticationService;
class ChromeAccountManagerService;
class PrefService;
@protocol SigninScreenConsumer;
namespace syncer {
class SyncService;
}  // syncer

// Mediator that handles the sign-in operation.
@interface SigninScreenMediator : NSObject

// Consumer for this mediator.
@property(nonatomic, weak) id<SigninScreenConsumer> consumer;

// The designated initializer.
// |accountManagerService| account manager service.
// |authenticationService| authentication service.
// |localPrefService| application local pref.
// |prefService| user pref.
// |syncService| sync service.
// |showFREConsent| YES if the screen needs to display the term of service.
- (instancetype)initWithAccountManagerService:
                    (ChromeAccountManagerService*)accountManagerService
                        authenticationService:
                            (AuthenticationService*)authenticationService
                             localPrefService:(PrefService*)localPrefService
                                  prefService:(PrefService*)prefService
                                  syncService:(syncer::SyncService*)syncService
                               showFREConsent:(BOOL)showFREConsent
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnect the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_SIGNIN_SIGNIN_SCREEN_MEDIATOR_H_
