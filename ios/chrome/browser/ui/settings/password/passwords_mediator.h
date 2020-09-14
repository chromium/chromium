// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_MEDIATOR_H_

#import <Foundation/Foundation.h>

#include "base/memory/scoped_refptr.h"
#import "ios/chrome/browser/ui/settings/password/passwords_table_view_controller_delegate.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"

class AuthenticationService;
class IOSChromePasswordCheckManager;
@protocol PasswordsConsumer;
class SyncSetupService;

namespace password_manager {
class PasswordStore;
}

// This mediator fetches and organises the passwords for its consumer.
@interface PasswordsMediator : NSObject <PasswordsTableViewControllerDelegate,
                                         SuccessfulReauthTimeAccessor>

- (instancetype)
    initWithPasswordStore:
        (scoped_refptr<password_manager::PasswordStore>)passwordStore
     passwordCheckManager:
         (scoped_refptr<IOSChromePasswordCheckManager>)passwordCheckManager
              authService:(AuthenticationService*)authService
              syncService:(SyncSetupService*)syncService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, weak) id<PasswordsConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_MEDIATOR_H_
