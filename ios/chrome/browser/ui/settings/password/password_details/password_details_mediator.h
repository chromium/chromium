// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_controller_delegate.h"

namespace password_manager {
struct CredentialUIEntry;
}  // namespace password_manager

class IOSChromePasswordCheckManager;
@protocol PasswordDetailsConsumer;

// This mediator fetches and organises the credentials for its consumer.
@interface PasswordDetailsMediator
    : NSObject <PasswordDetailsTableViewControllerDelegate>

// CredentialUIEntry is converted to the PasswordDetails and passed to a
// consumer.
- (instancetype)initWithPassword:
                    (const password_manager::CredentialUIEntry&)credential
            passwordCheckManager:(IOSChromePasswordCheckManager*)manager
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Consumer of this mediator.
@property(nonatomic, weak) id<PasswordDetailsConsumer> consumer;

// Password passed to the mediator.
@property(nonatomic, readonly) password_manager::CredentialUIEntry credential;

// Disconnects the mediator from all observers.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_MEDIATOR_H_
