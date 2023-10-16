// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_SHARING_STATUS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_SHARING_STATUS_MEDIATOR_H_

#import <Foundation/Foundation.h>

@class RecipientInfoForIOSDisplay;
@protocol SharingStatusConsumer;

class AuthenticationService;
class ChromeAccountManagerService;

// This mediator passes display information about sender and recipients of the
// user to its consumer.
@interface SharingStatusMediator : NSObject

- (instancetype)
      initWithAuthService:(AuthenticationService*)authService
    accountManagerService:(ChromeAccountManagerService*)accountManagerService
               recipients:(NSArray<RecipientInfoForIOSDisplay*>*)recipients
                  website:(NSString*)website NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Consumer of this mediator.
@property(nonatomic, weak) id<SharingStatusConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_SHARING_STATUS_MEDIATOR_H_
