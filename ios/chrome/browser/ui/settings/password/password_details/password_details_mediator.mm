// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/password_details_mediator.h"

#import "base/containers/contains.h"
#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "ios/chrome/browser/passwords/password_check_observer_bridge.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_controller_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::SysNSStringToUTF16;

@interface PasswordDetailsMediator () <
    PasswordCheckObserver,
    PasswordDetailsTableViewControllerDelegate> {
  // Password Check manager.
  IOSChromePasswordCheckManager* _manager;

  // Listens to compromised passwords changes.
  std::unique_ptr<PasswordCheckObserverBridge> _passwordCheckObserver;
}

// Dictionary of usernames of a same domain. Key: domain and value: NSSet of
// usernames.
@property(nonatomic, strong)
    NSMutableDictionary<NSString*, NSMutableSet<NSString*>*>*
        usernamesWithSameDomainDict;

// Display name to use for the Password Details view.
@property(nonatomic, strong) NSString* displayName;

@end

@implementation PasswordDetailsMediator

- (instancetype)initWithPasswords:
                    (const std::vector<password_manager::CredentialUIEntry>&)
                        credentials
                      displayName:(NSString*)displayName
             passwordCheckManager:(IOSChromePasswordCheckManager*)manager {
  self = [super init];
  if (self) {
    _manager = manager;
    _credentials = credentials;
    _displayName = displayName;
    _passwordCheckObserver.reset(
        new PasswordCheckObserverBridge(self, manager));
    DCHECK(!_credentials.empty());

    // TODO(crbug.com/1400692): Improve saved passwords logic when helper is
    // available in SavedPasswordsPresenter.
    _usernamesWithSameDomainDict = [[NSMutableDictionary alloc] init];
    NSMutableSet<NSString*>* signonRealms = [[NSMutableSet alloc] init];
    auto savedCredentials =
        manager->GetSavedPasswordsPresenter()->GetSavedCredentials();

    // Store all usernames by domain.
    for (const auto& credential : _credentials) {
      [signonRealms
          addObject:[NSString
                        stringWithCString:credential.GetFirstSignonRealm()
                                              .c_str()
                                 encoding:[NSString defaultCStringEncoding]]];
    }
    for (const auto& cred : savedCredentials) {
      NSString* signonRealm =
          [NSString stringWithCString:cred.GetFirstSignonRealm().c_str()
                             encoding:[NSString defaultCStringEncoding]];
      if ([signonRealms containsObject:signonRealm]) {
        NSMutableSet* set =
            [_usernamesWithSameDomainDict objectForKey:signonRealm];
        if (!set) {
          set = [[NSMutableSet alloc] init];
          [set addObject:base::SysUTF16ToNSString(cred.username)];
          [_usernamesWithSameDomainDict setObject:set forKey:signonRealm];

        } else {
          [set addObject:base::SysUTF16ToNSString(cred.username)];
        }
      }
    }
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

// Update the usernames by domain dictionary by removing the old username and
// adding the new one if it has changed.
- (void)updateOldUsernameInDict:(NSString*)oldUsername
                  toNewUsername:(NSString*)newUsername
                withSignonRealm:(NSString*)signonRealm {
  if ([oldUsername isEqualToString:newUsername]) {
    return;
  }

  NSMutableSet* set = [_usernamesWithSameDomainDict objectForKey:signonRealm];
  if (set) {
    [set removeObject:oldUsername];
    [set addObject:newUsername];
  }
}

#pragma mark - PasswordDetailsTableViewControllerDelegate

- (void)passwordDetailsViewController:
            (PasswordDetailsTableViewController*)viewController
               didEditPasswordDetails:(PasswordDetails*)password
                      withOldUsername:(NSString*)oldUsername
                       andOldPassword:(NSString*)oldPassword {
  if ([password.password length] != 0) {
    password_manager::CredentialUIEntry original_credential;

    auto it = std::find_if(
        _credentials.begin(), _credentials.end(),
        [password, oldUsername,
         oldPassword](password_manager::CredentialUIEntry credential) {
          return
              [password.signonRealm
                  isEqualToString:[NSString stringWithUTF8String:
                                                credential.GetFirstSignonRealm()
                                                    .c_str()]] &&
              [oldUsername isEqualToString:base::SysUTF16ToNSString(
                                               credential.username)] &&
              [oldPassword isEqualToString:base::SysUTF16ToNSString(
                                               credential.password)];
        });

    // There should be no reason not to find the credential in the vector of
    // credentials.
    DCHECK(it != _credentials.end());

    original_credential = *it;
    password_manager::CredentialUIEntry updated_credential =
        original_credential;
    updated_credential.username = SysNSStringToUTF16(password.username);
    updated_credential.password = SysNSStringToUTF16(password.password);
    if (_manager->GetSavedPasswordsPresenter()->EditSavedCredentials(
            original_credential, updated_credential) ==
        password_manager::SavedPasswordsPresenter::EditResult::kSuccess) {
      // Update the usernames by domain dictionary.
      NSString* signonRealm = [NSString
          stringWithCString:updated_credential.GetFirstSignonRealm().c_str()
                   encoding:[NSString defaultCStringEncoding]];
      [self updateOldUsernameInDict:oldUsername
                      toNewUsername:password.username
                    withSignonRealm:signonRealm];

      // Update the credential in the credentials vector.
      *it = std::move(updated_credential);
      return;
    }
  }
}

- (void)didFinishEditingPasswordDetails {
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

- (BOOL)isUsernameReused:(NSString*)newUsername forDomain:(NSString*)domain {
  // It is more efficient to check set of the usernames for the same origin
  // instead of delegating this to the `_manager`.
  return [[_usernamesWithSameDomainDict objectForKey:domain]
      containsObject:newUsername];
}

#pragma mark - PasswordCheckObserver

- (void)passwordCheckStateDidChange:(PasswordCheckState)state {
  // No-op. Changing password check state has no effect on compromised
  // passwords.
}

- (void)compromisedCredentialsDidChange {
  [self fetchPasswordWith:_manager->GetUnmutedCompromisedCredentials()];
}

#pragma mark - Private

// Updates password details and sets it to a consumer.
- (void)fetchPasswordWith:
    (const std::vector<password_manager::CredentialUIEntry>&)credentials {
  NSMutableArray<PasswordDetails*>* passwords = [NSMutableArray array];
  for (password_manager::CredentialUIEntry credential : _credentials) {
    PasswordDetails* password =
        [[PasswordDetails alloc] initWithCredential:credential];
    password.compromised = base::Contains(credentials, credential);
    [passwords addObject:password];
  }
  [self.consumer setPasswords:passwords andTitle:_displayName];
}

@end
