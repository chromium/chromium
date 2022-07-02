// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/password_details_mediator.h"

#include "base/strings/sys_string_conversions.h"
#include "components/password_manager/core/browser/password_form.h"
#include "ios/chrome/browser/passwords/password_check_observer_bridge.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_controller_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::SysNSStringToUTF16;
using InsecureCredentialsView =
    password_manager::InsecureCredentialsManager::CredentialsView;

@interface PasswordDetailsMediator () <
    PasswordCheckObserver,
    PasswordDetailsTableViewControllerDelegate> {
  // Password Check manager.
  IOSChromePasswordCheckManager* _manager;

  // Listens to compromised passwords changes.
  std::unique_ptr<PasswordCheckObserverBridge> _passwordCheckObserver;
}

// List of the usernames for the same domain.
@property(nonatomic, strong) NSSet<NSString*>* usernamesWithSameDomain;

@end

@implementation PasswordDetailsMediator

- (instancetype)initWithPassword:
                    (const password_manager::PasswordForm&)passwordForm
            passwordCheckManager:(IOSChromePasswordCheckManager*)manager {
  self = [super init];
  if (self) {
    _manager = manager;
    _password = passwordForm;
    _passwordCheckObserver.reset(
        new PasswordCheckObserverBridge(self, manager));
    NSMutableSet<NSString*>* usernames = [[NSMutableSet alloc] init];
    auto forms = manager->GetAllCredentials();
    for (const auto& form : forms) {
      if (form.signon_realm == passwordForm.signon_realm) {
        [usernames addObject:base::SysUTF16ToNSString(form.username_value)];
      }
    }
    [usernames
        removeObject:base::SysUTF16ToNSString(passwordForm.username_value)];
    _usernamesWithSameDomain = usernames;
  }
  return self;
}

- (void)setConsumer:(id<PasswordDetailsConsumer>)consumer {
  if (_consumer == consumer)
    return;
  _consumer = consumer;

  [self fetchPasswordWith:_manager->GetUnmutedCompromisedCredentials()];
}

- (void)disconnect {
  _manager->RemoveObserver(_passwordCheckObserver.get());
}

#pragma mark - PasswordDetailsTableViewControllerDelegate

- (void)passwordDetailsViewController:
            (PasswordDetailsTableViewController*)viewController
               didEditPasswordDetails:(PasswordDetails*)password {
  if ([password.password length] != 0) {
    if (_manager->EditPasswordForm(_password,
                                   SysNSStringToUTF16(password.username),
                                   SysNSStringToUTF16(password.password))) {
      _password.username_value = SysNSStringToUTF16(password.username);
      _password.password_value = SysNSStringToUTF16(password.password);
      return;
    }
  }
  [self fetchPasswordWith:_manager->GetUnmutedCompromisedCredentials()];
}

- (void)passwordDetailsViewController:
            (PasswordDetailsTableViewController*)viewController
                didAddPasswordDetails:(NSString*)username
                             password:(NSString*)password {
  NOTREACHED();
}

- (void)checkForDuplicates:(NSString*)username {
  NOTREACHED();
}

- (void)showExistingCredential:(NSString*)username {
  NOTREACHED();
}

- (void)didCancelAddPasswordDetails {
  NOTREACHED();
}

- (void)setWebsiteURL:(NSString*)website {
  NOTREACHED();
}

- (BOOL)isURLValid {
  return YES;
}

- (BOOL)isTLDMissing {
  return NO;
}

- (BOOL)isUsernameReused:(NSString*)newUsername {
  // It is more efficient to check set of the usernames for the same origin
  // instead of delegating this to the `_manager`.
  return [self.usernamesWithSameDomain containsObject:newUsername];
}

#pragma mark - PasswordCheckObserver

- (void)passwordCheckStateDidChange:(PasswordCheckState)state {
  // No-op. Changing password check state has no effect on compromised
  // passwords.
}

- (void)compromisedCredentialsDidChange:(InsecureCredentialsView)credentials {
  [self fetchPasswordWith:credentials];
}

#pragma mark - Private

// Updates password details and sets it to a consumer.
- (void)fetchPasswordWith:(InsecureCredentialsView)credentials {
  PasswordDetails* password =
      [[PasswordDetails alloc] initWithPasswordForm:_password];
  password.compromised = NO;

  for (const auto& credential : credentials) {
    if (std::tie(credential.signon_realm, credential.username,
                 credential.password) == std::tie(_password.signon_realm,
                                                  _password.username_value,
                                                  _password.password_value))
      password.compromised = YES;
  }

  [self.consumer setPassword:password];
}

@end
