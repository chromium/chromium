// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_controller_delegate.h"

namespace password_manager {
struct CredentialUIEntry;
}  // namespace password_manager

class IOSChromePasswordCheckManager;
@protocol PasswordDetailsConsumer;

// This mediator fetches and organises the credentials for its consumer.
@interface PasswordDetailsMediator
    : NSObject <PasswordDetailsTableViewControllerDelegate>

// Vector of CredentialUIEntry is converted to an array of PasswordDetails and
// passed to a consumer with the display name (title) for the Password Details
// view.
- (instancetype)initWithPasswords:
                    (const std::vector<password_manager::CredentialUIEntry>&)
                        credentials
                      displayName:(NSString*)displayName
             passwordCheckManager:(IOSChromePasswordCheckManager*)manager
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Consumer of this mediator.
@property(nonatomic, weak) id<PasswordDetailsConsumer> consumer;

// Array of credentials passed to the mediator.
@property(nonatomic, readonly) std::vector<password_manager::CredentialUIEntry>
    credentials;

// Disconnects the mediator from all observers.
- (void)disconnect;

// Remove credential from credentials cache.
- (void)removeCredential:(const password_manager::CredentialUIEntry&)credential;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_MEDIATOR_H_
