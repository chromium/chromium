// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/password_details_mediator.h"

#import <memory>
#import <utility>
#import <vector>

#import "base/containers/contains.h"
#import "base/containers/cxx20_erase.h"
#import "base/containers/flat_set.h"
#import "base/memory/raw_ptr.h"
#import "base/ranges/algorithm.h"
#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_manager_features_util.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/sync/base/features.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/browser/passwords/password_check_observer_bridge.h"
#import "ios/chrome/browser/passwords/password_checkup_metrics.h"
#import "ios/chrome/browser/passwords/password_checkup_utils.h"
#import "ios/chrome/browser/ui/settings/password/account_storage_utils.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_mediator+private.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_mediator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_metrics_utils.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_controller_delegate.h"

using base::SysNSStringToUTF16;
using password_manager::CredentialUIEntry;

namespace {

bool IsPasswordNotesWithBackupEnabled() {
  return base::FeatureList::IsEnabled(syncer::kPasswordNotesWithBackup);
}

bool MatchesRealmUsernameAndPassword(PasswordDetails* password,
                                     const CredentialUIEntry& credential) {
  return base::SysNSStringToUTF8(password.signonRealm) ==
             credential.GetFirstSignonRealm() &&
         base::SysNSStringToUTF16(password.username) == credential.username &&
         base::SysNSStringToUTF16(password.password) == credential.password;
}

// Helper that determines if a credential should be displayed as compromised in
// password details. Even if a credential is compromised, it is only displayed
// as such when password details was opened from the password manager or the
// compromised password issues page.
bool ShouldDisplayCredentialAsCompromised(
    DetailsContext details_context,
    const CredentialUIEntry& credential,
    std::vector<password_manager::CredentialUIEntry> insecure_credentials) {
  switch (details_context) {
    case DetailsContext::kPasswordSettings:
    case DetailsContext::kOutsideSettings:
    case DetailsContext::kCompromisedIssues:
    case DetailsContext::kDismissedWarnings:
      for (const auto& insecure_credential : insecure_credentials) {
        if (credential == insecure_credential) {
          return password_manager::features::IsPasswordCheckupEnabled()
                     ? IsCredentialUnmutedCompromised(insecure_credential)
                     : IsCompromised(insecure_credential);
        }
      }
      return false;
    case DetailsContext::kReusedIssues:
    case DetailsContext::kWeakIssues:
      return false;
  }
}

// Helper that determines if a credential should be displayed as muted in
// password details. Even if a credential is muted, it is only displayed
// as such when password details was opened from the dismissed warning issues
// page.
bool ShouldDisplayCredentialAsMuted(
    DetailsContext details_context,
    const CredentialUIEntry& credential,
    std::vector<password_manager::CredentialUIEntry> insecure_credentials) {
  // Muted credentials are only available when kIOSPasswordCheckup feature is
  // enabled.
  if (!password_manager::features::IsPasswordCheckupEnabled()) {
    return false;
  }

  switch (details_context) {
    case DetailsContext::kPasswordSettings:
    case DetailsContext::kOutsideSettings:
    case DetailsContext::kCompromisedIssues:
    case DetailsContext::kReusedIssues:
    case DetailsContext::kWeakIssues:
      return false;
    case DetailsContext::kDismissedWarnings:
      for (const auto& insecure_credential : insecure_credentials) {
        if (credential == insecure_credential) {
          return insecure_credential.IsMuted();
        }
      }
      return false;
  }
}

}  // namespace

@interface PasswordDetailsMediator () <
    PasswordCheckObserver,
    PasswordDetailsTableViewControllerDelegate> {
  // Password Check manager.
  scoped_refptr<IOSChromePasswordCheckManager> _manager;

  // Listens to compromised passwords changes.
  std::unique_ptr<PasswordCheckObserverBridge> _passwordCheckObserver;

  // The BrowserState pref service.
  raw_ptr<PrefService> _prefService;

  // The sync service.
  raw_ptr<syncer::SyncService> _syncService;

  // Delegate for this mediator.
  id<PasswordDetailsMediatorDelegate> _delegate;
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

- (instancetype)
       initWithPasswords:(const std::vector<CredentialUIEntry>&)credentials
             displayName:(NSString*)displayName
    passwordCheckManager:(scoped_refptr<IOSChromePasswordCheckManager>)manager
             prefService:(PrefService*)prefService
             syncService:(syncer::SyncService*)syncService
                 context:(DetailsContext)context
                delegate:(id<PasswordDetailsMediatorDelegate>)delegate {
  DCHECK(manager);
  DCHECK(!credentials.empty());

  self = [super init];
  if (!self) {
    return nil;
  }

  _manager = manager;
  _passwordCheckObserver =
      std::make_unique<PasswordCheckObserverBridge>(self, manager.get());
  _credentials = credentials;
  _displayName = displayName;
  _context = context;
  _prefService = prefService;
  _syncService = syncService;
  _delegate = delegate;

  // TODO(crbug.com/1400692): Improve saved passwords logic when helper is
  // available in SavedPasswordsPresenter.
  _usernamesWithSameDomainDict = [[NSMutableDictionary alloc] init];
  NSMutableSet<NSString*>* signonRealms = [[NSMutableSet alloc] init];
  auto savedCredentials =
      manager->GetSavedPasswordsPresenter()->GetSavedCredentials();

  // Store all usernames by domain.
  for (const auto& credential : self.credentials) {
    [signonRealms
        addObject:[NSString
                      stringWithCString:credential.GetFirstSignonRealm().c_str()
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
  return self;
}

- (void)setConsumer:(id<PasswordDetailsConsumer>)consumer {
  if (_consumer == consumer)
    return;
  _consumer = consumer;

  // The email might be empty and the callee handles that.
  [_consumer setUserEmail:base::SysUTF8ToNSString(
                              _syncService->GetAccountInfo().email)];

  [self providePasswordsToConsumer];

  if (self.credentials[0].blocked_by_user) {
    DCHECK_EQ(self.credentials.size(), 1u);
    [_consumer setIsBlockedSite:YES];
  }
}

- (void)disconnect {
  _passwordCheckObserver.reset();
  _manager = nullptr;
}

- (void)removeCredential:(PasswordDetails*)password {
  // When details was opened from the Password Manager, only log password
  // check actions if the password is compromised.
  if (password_manager::ShouldRecordPasswordCheckUserAction(
          self.context, password.compromised)) {
    password_manager::LogDeletePassword(
        password_manager::GetWarningTypeForDetailsContext(self.context));
  }

  // Map from PasswordDetails to CredentialUIEntry. Should support blocklists.
  auto it = base::ranges::find_if(
      _credentials, [password](const CredentialUIEntry& credential) {
        return MatchesRealmUsernameAndPassword(password, credential);
      });
  if (it == _credentials.end()) {
    // TODO(crbug.com/1359392): Convert into DCHECK.
    return;
  }

  // Use the iterator before base::Erase() makes it invalid.
  _manager->GetSavedPasswordsPresenter()->RemoveCredential(*it);
  // TODO(crbug.com/1359392). Once kPasswordsGrouping launches, the mediator
  // should update the passwords model and receive the updates via
  // SavedPasswordsPresenterObserver, instead of replicating the updates to its
  // own copy and calling [self providePasswordsToConsumer:]. Today when the
  // flag is disabled and the password is edited, it's impossible to identify
  // the new object to show (sign-on realm can't be used as an id, there might
  // be multiple credentials; nor username/password since the values changed).
  base::Erase(_credentials, *it);
  [self providePasswordsToConsumer];

  // Update form managers so the list of password suggestions shown to the user
  // is the correct one.
  [_delegate updateFormManagers];
}

- (void)moveCredentialToAccountStore:(PasswordDetails*)password {
  // Map from PasswordDetails to CredentialUIEntry.
  auto it = base::ranges::find_if(
      _credentials, [password](const CredentialUIEntry& credential) {
        return MatchesRealmUsernameAndPassword(password, credential);
      });

  if (it == _credentials.end()) {
    return;
  }

  it->stored_in = {password_manager::PasswordForm::Store::kAccountStore};
  _manager->GetSavedPasswordsPresenter()->MoveCredentialsToAccount(
      {*it}, password_manager::metrics_util::MoveToAccountStoreTrigger::
                 kExplicitlyTriggeredInSettings);
  [self providePasswordsToConsumer];
}

- (void)moveCredentialToAccountStoreWithConflict:(PasswordDetails*)password {
  auto localCredential = base::ranges::find_if(
      _credentials, [password](const CredentialUIEntry& credential) {
        return MatchesRealmUsernameAndPassword(password, credential);
      });
  absl::optional<CredentialUIEntry> accountCredential =
      [self conflictingAccountPassword:password];
  DCHECK(localCredential != _credentials.end());
  DCHECK(accountCredential.has_value());
  if (localCredential->last_used_time < accountCredential->last_used_time) {
    [self removeCredential:password];
    return;
  }
  [self removeCredential:[[PasswordDetails alloc]
                             initWithCredential:*accountCredential]];
  [self moveCredentialToAccountStore:password];
}

- (BOOL)hasPasswordConflictInAccount:(PasswordDetails*)password {
  return [self conflictingAccountPassword:password].has_value();
}

- (void)didConfirmWarningDismissalForPassword:(PasswordDetails*)password {
  // Map from PasswordDetails to CredentialUIEntry.
  auto it = base::ranges::find_if(
      _credentials,
      [password](const password_manager::CredentialUIEntry& credential) {
        return MatchesRealmUsernameAndPassword(password, credential);
      });

  if (it == _credentials.end()) {
    return;
  }

  _manager->MuteCredential(*it);
}

#pragma mark - PasswordDetailsTableViewControllerDelegate

- (void)passwordDetailsViewController:
            (PasswordDetailsTableViewController*)viewController
               didEditPasswordDetails:(PasswordDetails*)password
                      withOldUsername:(NSString*)oldUsername
                          oldPassword:(NSString*)oldPassword
                              oldNote:(NSString*)oldNote {
  if ([password.password length] != 0) {
    CredentialUIEntry original_credential;

    auto it = base::ranges::find_if(
        _credentials, [password, oldUsername, oldPassword,
                       oldNote](const CredentialUIEntry& credential) {
          return
              [password.signonRealm
                  isEqualToString:[NSString stringWithUTF8String:
                                                credential.GetFirstSignonRealm()
                                                    .c_str()]] &&
              [oldUsername isEqualToString:base::SysUTF16ToNSString(
                                               credential.username)] &&
              [oldPassword isEqualToString:base::SysUTF16ToNSString(
                                               credential.password)] &&
              (!IsPasswordNotesWithBackupEnabled() ||
               [oldNote
                   isEqualToString:base::SysUTF16ToNSString(credential.note)]);
        });

    // There should be no reason not to find the credential in the vector of
    // credentials.
    DCHECK(it != _credentials.end());

    original_credential = *it;
    CredentialUIEntry updated_credential = original_credential;
    updated_credential.username = SysNSStringToUTF16(password.username);
    updated_credential.password = SysNSStringToUTF16(password.password);
    if (IsPasswordNotesWithBackupEnabled()) {
      updated_credential.note = SysNSStringToUTF16(password.note);
    }
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

      // Update form managers so the list of password suggestions shown to the
      // user is the correct one.
      [_delegate updateFormManagers];
      return;
    }
  }
}

- (void)didFinishEditingPasswordDetails {
  [self providePasswordsToConsumer];
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

- (void)dismissWarningForPassword:(PasswordDetails*)password {
  // Show confirmation dialog.
  [_delegate showDismissWarningDialogWithPasswordDetails:password];
}

- (void)restoreWarningForCurrentPassword {
  // Restoring a warning is only available in the
  // DetailsContext::kDismissedWarnings context, which is always showing only 1
  // credential.
  CHECK(self.credentials.size() == 1);
  password_manager::CredentialUIEntry credential = self.credentials[0];
  _manager->UnmuteCredential(credential);
  base::Erase(_credentials, credential);
  [self providePasswordsToConsumer];
}

#pragma mark - PasswordCheckObserver

- (void)passwordCheckStateDidChange:(PasswordCheckState)state {
  // No-op. Changing password check state has no effect on compromised
  // passwords.
}

- (void)insecureCredentialsDidChange {
  [self providePasswordsToConsumer];
}

#pragma mark - Private

// Pushes password details to the consumer.
- (void)providePasswordsToConsumer {
  NSMutableArray<PasswordDetails*>* passwords = [NSMutableArray array];
  // Fetch the insecure credentials to get their updated version.
  std::vector<password_manager::CredentialUIEntry> insecureCredentials =
      _manager->GetInsecureCredentials();
  for (const CredentialUIEntry& credential : self.credentials) {
    PasswordDetails* password =
        [[PasswordDetails alloc] initWithCredential:credential];
    password.context = self.context;
    password.compromised = ShouldDisplayCredentialAsCompromised(
        self.context, credential, insecureCredentials);

    // `password.isCompromised` is always false for muted credentials, so
    // short-circuit to avoid unnecessary computation in
    // ShouldDisplayCredentialAsMuted.
    password.muted = !password.isCompromised &&
                     ShouldDisplayCredentialAsMuted(self.context, credential,
                                                    insecureCredentials);

    // Only offer moving to the account if all of these hold.
    // - The embedder of this page wants to support it.
    // - The entry was flagged as local only in the top-level view.
    // - The user is interested in saving passwords to the account, i.e. they
    // are opted in to account storage.
    password.shouldOfferToMoveToAccount =
        self.context == DetailsContext::kPasswordSettings &&
        password_manager::features_util::IsOptedInForAccountStorage(
            _prefService, _syncService) &&
        ShouldShowLocalOnlyIcon(credential, _syncService);
    [passwords addObject:password];
  }
  [self.consumer setPasswords:passwords andTitle:_displayName];
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

// Returns a credential that a) is saved in the user account, and b) has the
// same website/username as `password`, but a different password value.
- (absl::optional<CredentialUIEntry>)conflictingAccountPassword:
    (PasswordDetails*)password {
  // All credentials for the same website are in `_credentials` due to password
  // grouping. So it's enough to search that reduced list and not all saved
  // passwords.
  auto it = base::ranges::find_if(
      _credentials, [password](const CredentialUIEntry& credential) {
        return credential.stored_in.contains(
                   password_manager::PasswordForm::Store::kAccountStore) &&
               base::SysNSStringToUTF8(password.signonRealm) ==
                   credential.GetFirstSignonRealm() &&
               base::SysNSStringToUTF16(password.username) ==
                   credential.username &&
               base::SysNSStringToUTF16(password.password) !=
                   credential.password;
      });
  if (it == _credentials.end()) {
    return absl::nullopt;
  }
  return *it;
}

@end
