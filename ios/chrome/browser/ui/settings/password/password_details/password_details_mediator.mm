// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/password_details_mediator.h"

#import <algorithm>
#import <memory>
#import <utility>
#import <vector>

#import "base/containers/contains.h"
#import "base/containers/flat_set.h"
#import "base/memory/raw_ptr.h"
#import "base/ranges/algorithm.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "build/branding_buildflags.h"
#import "components/password_manager/core/browser/features/password_manager_features_util.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_manager_metrics_util.h"
#import "components/password_manager/core/browser/password_sync_util.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/model/password_check_observer_bridge.h"
#import "ios/chrome/browser/passwords/model/password_checkup_metrics.h"
#import "ios/chrome/browser/passwords/model/password_checkup_utils.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/settings/password/account_storage_utils.h"
#import "ios/chrome/browser/ui/settings/password/password_details/credential_details.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_mediator+Testing.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_mediator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_metrics_utils.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_controller_delegate.h"

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
#import "base/command_line.h"
#import "components/password_manager/core/browser/password_manager_switches.h"
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)

using base::SysNSStringToUTF16;
using password_manager::CredentialUIEntry;

namespace {

base::Time GetLastUsedModifiedOrCreatedTime(
    password_manager::SavedPasswordsPresenter* saved_passwords_presenter,
    const CredentialUIEntry& entry) {
  base::Time time = entry.last_used_time;
  for (const password_manager::PasswordForm& form :
       saved_passwords_presenter->GetCorrespondingPasswordForms(entry)) {
    time = std::max(time, form.date_last_used);
    time = std::max(time, form.date_password_modified);
    time = std::max(time, form.date_created);
  }
  return time;
}

bool MatchesRealmUsernamePasswordAndCreationTime(
    CredentialDetails* credentialDetails,
    const CredentialUIEntry& credential) {
  return base::SysNSStringToUTF8(credentialDetails.signonRealm) ==
             credential.GetFirstSignonRealm() &&
         base::SysNSStringToUTF16(credentialDetails.username) ==
             credential.username &&
         base::SysNSStringToUTF16(credentialDetails.password) ==
             credential.password &&
         base::SysNSStringToUTF16(credentialDetails.userDisplayName) ==
             credential.user_display_name &&
         credentialDetails.creationTime == credential.creation_time;
}

// Whether displaying a credential as compromised is supported in the current
// context.
bool CanDisplayCredentialAsCompromised(DetailsContext details_context) {
  switch (details_context) {
    case DetailsContext::kPasswordSettings:
    case DetailsContext::kOutsideSettings:
    case DetailsContext::kCompromisedIssues:
    case DetailsContext::kDismissedWarnings:
      return true;
    case DetailsContext::kReusedIssues:
    case DetailsContext::kWeakIssues:
      return false;
  }
}

// Helper that determines if a credential should be displayed as compromised in
// password details. Even if a credential is compromised, it is only displayed
// as such when password details was opened from the password manager or the
// compromised password issues page.
bool ShouldDisplayCredentialAsCompromised(
    DetailsContext details_context,
    const CredentialUIEntry& credential,
    std::vector<password_manager::CredentialUIEntry> insecure_credentials) {
  if (!CanDisplayCredentialAsCompromised(details_context)) {
    return false;
  }

  for (const auto& insecure_credential : insecure_credentials) {
    if (credential == insecure_credential) {
      return IsCredentialUnmutedCompromised(insecure_credential);
    }
  }
  return false;
}

// Whether displaying a credential as muted is supported in the current context.
bool CanDisplayCredentialAsMuted(DetailsContext details_context) {
  switch (details_context) {
    case DetailsContext::kPasswordSettings:
    case DetailsContext::kOutsideSettings:
    case DetailsContext::kCompromisedIssues:
    case DetailsContext::kReusedIssues:
    case DetailsContext::kWeakIssues:
      return false;
    case DetailsContext::kDismissedWarnings:
      return true;
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
  if (!CanDisplayCredentialAsMuted(details_context)) {
    return false;
  }

  for (const auto& insecure_credential : insecure_credentials) {
    if (credential == insecure_credential) {
      return insecure_credential.IsMuted();
    }
  }
  return false;
}

// Returns true if the credential matches the other arguments.
bool AreMatchingCredentials(const CredentialUIEntry& credential,
                            CredentialDetails* credential_details,
                            NSString* old_username,
                            NSString* old_user_display_name) {
  return
      [credential_details.signonRealm
          isEqualToString:base::SysUTF8ToNSString(
                              credential.GetFirstSignonRealm())] &&
      [old_username
          isEqualToString:base::SysUTF16ToNSString(credential.username)] &&
      [old_user_display_name isEqualToString:base::SysUTF16ToNSString(
                                                 credential.user_display_name)];
}

// Returns true if the credential matches the other arguments.
bool AreMatchingCredentials(const CredentialUIEntry& credential,
                            CredentialDetails* credential_details,
                            NSString* old_username,
                            NSString* old_password,
                            NSString* old_note) {
  return [credential_details.signonRealm
             isEqualToString:base::SysUTF8ToNSString(
                                 credential.GetFirstSignonRealm())] &&
         [old_username
             isEqualToString:base::SysUTF16ToNSString(credential.username)] &&
         [old_password
             isEqualToString:base::SysUTF16ToNSString(credential.password)] &&
         [old_note isEqualToString:base::SysUTF16ToNSString(credential.note)];
}

}  // namespace

@interface PasswordDetailsMediator () <
    PasswordCheckObserver,
    PasswordDetailsTableViewControllerDelegate> {
  // Password Check manager.
  scoped_refptr<IOSChromePasswordCheckManager> _manager;

  // Listens to compromised passwords changes.
  std::unique_ptr<PasswordCheckObserverBridge> _passwordCheckObserver;

  // The profile pref service.
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

// The context in which the password details are accessed.
@property(nonatomic, assign) DetailsContext context;

@end

@implementation PasswordDetailsMediator

- (instancetype)
    initWithPasswords:(const std::vector<CredentialUIEntry>&)credentials
          displayName:(NSString*)displayName
              profile:(ProfileIOS*)profile
              context:(DetailsContext)context
             delegate:(id<PasswordDetailsMediatorDelegate>)delegate {
  DCHECK(profile);
  DCHECK(!credentials.empty());

  self = [super init];
  if (!self) {
    return nil;
  }

  _manager = IOSChromePasswordCheckManagerFactory::GetForProfile(profile).get();
  _passwordCheckObserver =
      std::make_unique<PasswordCheckObserverBridge>(self, _manager.get());
  _credentials = credentials;
  _displayName = displayName;
  _context = context;
  _prefService = profile->GetPrefs();
  _syncService = SyncServiceFactory::GetForProfile(profile);
  _delegate = delegate;

  return self;
}

- (void)setConsumer:(id<PasswordDetailsConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;

  // The email might be empty and the callee handles that.
  [_consumer setUserEmail:base::SysUTF8ToNSString(
                              _syncService->GetAccountInfo().email)];

  [self providePasswordsToConsumer];

  if (self.credentials[0].blocked_by_user) {
    DCHECK_EQ(self.credentials.size(), 1u);
    [_consumer setIsBlockedSite:YES];
  }

  if ([self shouldDisplayShareButton]) {
    [_consumer setupRightShareButton:
                   _prefService->GetBoolean(
                       password_manager::prefs::kPasswordSharingEnabled)];
  }
}

- (void)disconnect {
  _passwordCheckObserver.reset();
  _manager = nullptr;
}

- (void)removeCredential:(CredentialDetails*)credentialDetails {
  // When details was opened from the Password Manager, only log password
  // check actions if the password is compromised.
  if (password_manager::ShouldRecordPasswordCheckUserAction(
          self.context, credentialDetails.compromised)) {
    password_manager::LogDeletePassword(
        password_manager::GetWarningTypeForDetailsContext(self.context));
  }

  // Map from CredentialDetails to CredentialUIEntry. Should support blocklists.
  auto it = base::ranges::find_if(
      _credentials, [credentialDetails](const CredentialUIEntry& credential) {
        return MatchesRealmUsernamePasswordAndCreationTime(credentialDetails,
                                                           credential);
      });
  if (it == _credentials.end()) {
    // TODO(crbug.com/40862365): Convert into DCHECK.
    return;
  }

  // Use the iterator before std::erase() makes it invalid.
  self.savedPasswordsPresenter->RemoveCredential(*it);
  // TODO(crbug.com/40862365). Once kPasswordsGrouping launches, the mediator
  // should update the passwords model and receive the updates via
  // SavedPasswordsPresenterObserver, instead of replicating the updates to its
  // own copy and calling [self providePasswordsToConsumer:]. Today when the
  // flag is disabled and the password is edited, it's impossible to identify
  // the new object to show (sign-on realm can't be used as an id, there might
  // be multiple credentials; nor username/password since the values changed).
  std::erase(_credentials, *it);
  [self providePasswordsToConsumer];

  // Update form managers so the list of password suggestions shown to the user
  // is the correct one.
  [_delegate updateFormManagers];
}

- (void)moveCredentialToAccountStore:(CredentialDetails*)credentialDetails {
  // Map from CredentialDetails to CredentialUIEntry.
  auto it = base::ranges::find_if(
      _credentials, [credentialDetails](const CredentialUIEntry& credential) {
        return MatchesRealmUsernamePasswordAndCreationTime(credentialDetails,
                                                           credential);
      });

  if (it == _credentials.end()) {
    return;
  }

  it->stored_in = {password_manager::PasswordForm::Store::kAccountStore};
  self.savedPasswordsPresenter->MoveCredentialsToAccount(
      {*it}, password_manager::metrics_util::MoveToAccountStoreTrigger::
                 kExplicitlyTriggeredInSettings);
  [self providePasswordsToConsumer];
}

- (void)moveCredentialToAccountStoreWithConflict:
    (CredentialDetails*)credentialDetails {
  auto localCredential = base::ranges::find_if(
      _credentials, [credentialDetails](const CredentialUIEntry& credential) {
        return MatchesRealmUsernamePasswordAndCreationTime(credentialDetails,
                                                           credential);
      });
  std::optional<CredentialUIEntry> accountCredential =
      [self conflictingAccountPassword:credentialDetails];
  DCHECK(localCredential != _credentials.end());
  DCHECK(accountCredential.has_value());
  if (GetLastUsedModifiedOrCreatedTime(self.savedPasswordsPresenter,
                                       *localCredential) <
      GetLastUsedModifiedOrCreatedTime(self.savedPasswordsPresenter,
                                       *accountCredential)) {
    [self removeCredential:credentialDetails];
    return;
  }
  [self removeCredential:[[CredentialDetails alloc]
                             initWithCredential:*accountCredential]];
  [self moveCredentialToAccountStore:credentialDetails];
}

- (BOOL)hasPasswordConflictInAccount:(CredentialDetails*)credential {
  return [self conflictingAccountPassword:credential].has_value();
}

- (void)didConfirmWarningDismissalForPassword:
    (CredentialDetails*)credentialDetails {
  // Map from CredentialDetails to CredentialUIEntry.
  auto it = base::ranges::find_if(
      _credentials, [credentialDetails](
                        const password_manager::CredentialUIEntry& credential) {
        return MatchesRealmUsernamePasswordAndCreationTime(credentialDetails,
                                                           credential);
      });

  if (it == _credentials.end()) {
    return;
  }

  _manager->MuteCredential(*it);
}

- (password_manager::SavedPasswordsPresenter*)savedPasswordsPresenter {
  return _manager->GetSavedPasswordsPresenter();
}

#pragma mark - PasswordDetailsTableViewControllerDelegate

- (void)passwordDetailsViewController:
            (PasswordDetailsTableViewController*)viewController
             didEditCredentialDetails:(CredentialDetails*)credentialDetails
                      withOldUsername:(NSString*)oldUsername
                   oldUserDisplayName:(NSString*)oldUserDisplayName
                          oldPassword:(NSString*)oldPassword
                              oldNote:(NSString*)oldNote {
  CredentialUIEntry originalCredential;
  CredentialUIEntry updatedCredential;
  std::vector<CredentialUIEntry>::iterator it;
  if (credentialDetails.credentialType == CredentialTypePasskey) {
    it = base::ranges::find_if(
        _credentials, [credentialDetails, oldUsername, oldUserDisplayName](
                          const CredentialUIEntry& credential) {
          return AreMatchingCredentials(credential, credentialDetails,
                                        oldUsername, oldUserDisplayName);
        });

    // There should be no reason not to find the credential in the vector of
    // credentials.
    DCHECK(it != _credentials.end());

    originalCredential = *it;
    updatedCredential = originalCredential;
    updatedCredential.username = SysNSStringToUTF16(credentialDetails.username);
    updatedCredential.user_display_name =
        SysNSStringToUTF16(credentialDetails.userDisplayName);
  } else if ([credentialDetails.password length] != 0) {
    it = base::ranges::find_if(
        _credentials, [credentialDetails, oldUsername, oldPassword,
                       oldNote](const CredentialUIEntry& credential) {
          return AreMatchingCredentials(credential, credentialDetails,
                                        oldUsername, oldPassword, oldNote);
        });

    // There should be no reason not to find the credential in the vector of
    // credentials.
    CHECK(it != _credentials.end());

    originalCredential = *it;
    updatedCredential = originalCredential;
    updatedCredential.username = SysNSStringToUTF16(credentialDetails.username);
    updatedCredential.password = SysNSStringToUTF16(credentialDetails.password);
    updatedCredential.note = SysNSStringToUTF16(credentialDetails.note);
  } else {
    return;
  }

  if (self.savedPasswordsPresenter->EditSavedCredentials(originalCredential,
                                                         updatedCredential) ==
      password_manager::SavedPasswordsPresenter::EditResult::kSuccess) {
    // Update the usernames by domain dictionary.
    NSString* signonRealm =
        base::SysUTF8ToNSString(updatedCredential.GetFirstSignonRealm());
    [self updateOldUsernameInDict:oldUsername
                    toNewUsername:credentialDetails.username
                  withSignonRealm:signonRealm];

    // Update the credential in the credentials vector.
    *it = std::move(updatedCredential);

    // Update form managers so the list of password suggestions shown to the
    // user is the correct one.
    [_delegate updateFormManagers];
  }
}

- (void)didFinishEditingPasswordDetails {
  [self providePasswordsToConsumer];
}

- (void)passwordDetailsViewController:
            (PasswordDetailsTableViewController*)viewController
                didAddPasswordDetails:(NSString*)username
                             password:(NSString*)password {
  NOTREACHED_IN_MIGRATION();
}

- (void)checkForDuplicates:(NSString*)username {
  NOTREACHED_IN_MIGRATION();
}

- (void)showExistingCredential:(NSString*)username {
  NOTREACHED_IN_MIGRATION();
}

- (void)didCancelAddPasswordDetails {
  NOTREACHED_IN_MIGRATION();
}

- (void)setWebsiteURL:(NSString*)website {
  NOTREACHED_IN_MIGRATION();
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
  return [[self.usernamesWithSameDomainDict objectForKey:domain]
      containsObject:newUsername];
}

- (void)dismissWarningForPassword:(CredentialDetails*)credential {
  // Show confirmation dialog.
  [_delegate showDismissWarningDialogWithCredentialDetails:credential];
}

- (void)restoreWarningForCurrentPassword {
  // Restoring a warning is only available in the
  // DetailsContext::kDismissedWarnings context, which is always showing only 1
  // credential.
  CHECK(self.credentials.size() == 1);
  password_manager::CredentialUIEntry credential = self.credentials[0];
  _manager->UnmuteCredential(credential);
  std::erase(_credentials, credential);
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

- (void)passwordCheckManagerWillShutdown {
  _passwordCheckObserver.reset();
}

#pragma mark - Private

- (NSMutableDictionary<NSString*, NSMutableSet<NSString*>*>*)
    usernamesWithSameDomainDict {
  if (!_usernamesWithSameDomainDict) {
    // TODO(crbug.com/40883869): Improve saved passwords logic when helper is
    // available in SavedPasswordsPresenter.
    _usernamesWithSameDomainDict = [[NSMutableDictionary alloc] init];
    NSMutableSet<NSString*>* signonRealms = [[NSMutableSet alloc] init];
    auto savedCredentials = self.savedPasswordsPresenter->GetSavedCredentials();

    // Store all usernames by domain.
    for (const auto& credential : self.credentials) {
      [signonRealms
          addObject:base::SysUTF8ToNSString(credential.GetFirstSignonRealm())];
    }
    for (const auto& cred : savedCredentials) {
      NSString* signonRealm =
          base::SysUTF8ToNSString(cred.GetFirstSignonRealm());
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
  return _usernamesWithSameDomainDict;
}

// Pushes password details to the consumer.
- (void)providePasswordsToConsumer {
  NSMutableArray<CredentialDetails*>* passwords = [NSMutableArray array];
  // Fetch the insecure credentials to get their updated version.
  std::vector<password_manager::CredentialUIEntry> insecureCredentials;
  // Only fetch insecure credentials if they are going to be used.
  if (CanDisplayCredentialAsCompromised(self.context) ||
      CanDisplayCredentialAsMuted(self.context)) {
    insecureCredentials = _manager->GetInsecureCredentials();
  }
  for (const CredentialUIEntry& credential : self.credentials) {
    CredentialDetails* credentialDetails =
        [[CredentialDetails alloc] initWithCredential:credential];
    credentialDetails.context = self.context;
    credentialDetails.compromised = ShouldDisplayCredentialAsCompromised(
        self.context, credential, insecureCredentials);

    // `credentialDetails.isCompromised` is always false for muted credentials,
    // so short-circuit to avoid unnecessary computation in
    // ShouldDisplayCredentialAsMuted.
    credentialDetails.muted =
        !credentialDetails.isCompromised &&
        ShouldDisplayCredentialAsMuted(self.context, credential,
                                       insecureCredentials);

    // Only offer moving to the account if all of these hold.
    // - The embedder of this page wants to support it.
    // - The entry was flagged as local only in the top-level view.
    // - The user is interested in saving passwords to the account, i.e. they
    // are opted in to account storage.
    credentialDetails.shouldOfferToMoveToAccount =
        self.context == DetailsContext::kPasswordSettings &&
        password_manager::features_util::IsOptedInForAccountStorage(
            _prefService, _syncService) &&
        ShouldShowLocalOnlyIcon(credential, _syncService);
    [passwords addObject:credentialDetails];
  }
  [self.consumer setCredentials:passwords andTitle:_displayName];
}

// Update the usernames by domain dictionary by removing the old username and
// adding the new one if it has changed.
- (void)updateOldUsernameInDict:(NSString*)oldUsername
                  toNewUsername:(NSString*)newUsername
                withSignonRealm:(NSString*)signonRealm {
  if ([oldUsername isEqualToString:newUsername]) {
    return;
  }

  NSMutableSet* set =
      [self.usernamesWithSameDomainDict objectForKey:signonRealm];
  if (set) {
    [set removeObject:oldUsername];
    [set addObject:newUsername];
  }
}

// Returns a credential that a) is saved in the user account, and b) has the
// same website/username as `credentialDetails`, but a different password value.
- (std::optional<CredentialUIEntry>)conflictingAccountPassword:
    (CredentialDetails*)credentialDetails {
  // All credentials for the same website are in `_credentials` due to password
  // grouping. So it's enough to search that reduced list and not all saved
  // passwords.
  auto it = base::ranges::find_if(
      _credentials, [credentialDetails](const CredentialUIEntry& credential) {
        return credential.stored_in.contains(
                   password_manager::PasswordForm::Store::kAccountStore) &&
               base::SysNSStringToUTF8(credentialDetails.signonRealm) ==
                   credential.GetFirstSignonRealm() &&
               base::SysNSStringToUTF16(credentialDetails.username) ==
                   credential.username &&
               base::SysNSStringToUTF16(credentialDetails.password) !=
                   credential.password;
      });
  if (it == _credentials.end()) {
    return std::nullopt;
  }
  return *it;
}

// Returns YES if all of the following conditions are met:
// * User is syncing or signed in and opted in to account storage.
// * Password sending feature is enabled.
// * Build is branded (bypassed with a command line switch in EG tests).
- (BOOL)shouldDisplayShareButton {
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          password_manager::kEnableShareButtonUnbranded)) {
    return false;
  }
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)

  return password_manager::sync_util::GetAccountForSaving(_prefService,
                                                          _syncService)
      .has_value();
}

@end
