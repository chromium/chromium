// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_CHECKUP_PASSWORD_CHECKUP_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_CHECKUP_PASSWORD_CHECKUP_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_view_controller_delegate.h"

@protocol PasswordCheckupConsumer;

// This mediator fetches and organises the insecure credentials for its
// consumer.
@interface PasswordCheckupMediator
    : NSObject <PasswordCheckupViewControllerDelegate>

- (instancetype)initWithPasswordCheckManager:
    (scoped_refptr<IOSChromePasswordCheckManager>)passwordCheckManager
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Consumer of this mediator.
@property(nonatomic, weak) id<PasswordCheckupConsumer> consumer;

// Disconnect the observers.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_CHECKUP_PASSWORD_CHECKUP_MEDIATOR_H_
