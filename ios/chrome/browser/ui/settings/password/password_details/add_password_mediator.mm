// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/add_password_mediator.h"

#import "base/check.h"
#import "base/containers/contains.h"
#import "base/containers/flat_set.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/ranges/algorithm.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/cancelable_task_tracker.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "components/password_manager/core/browser/features/password_manager_features_util.h"
#import "components/password_manager/core/browser/form_parsing/form_data_parser.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/password_manager/core/browser/password_sync_util.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "ios/chrome/browser/passwords/model/password_check_observer_bridge.h"
#import "ios/chrome/browser/ui/settings/password/password_details/add_password_details_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_details/add_password_mediator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_details/add_password_view_controller_delegate.h"
#import "net/base/apple/url_conversions.h"

using base::SysNSStringToUTF8;
using base::SysNSStringToUTF16;
using base::SysUTF8ToNSString;

namespace {
// Checks for existing credentials with the same url and username.
bool CheckForDuplicates(
    GURL url,
    NSString* username,
    std::vector<password_manager::CredentialUIEntry> credentials) {
  std::string signon_realm = password_manager::GetSignonRealm(
      password_manager_util::StripAuthAndParams(url));
  std::u16string username_value = SysNSStringToUTF16(username);
  auto have_equal_username_and_realm =
      [&signon_realm,
       &username_value](const password_manager::CredentialUIEntry& credential) {
        return signon_realm == credential.GetFirstSignonRealm() &&
               username_value == credential.username;
      };
  if (base::ranges::any_of(credentials, have_equal_username_and_realm))
    return true;
  return false;
}
}

@interface AddPasswordMediator () <AddPasswordViewControllerDelegate> {
  // Password Check manager.
  raw_ptr<IOSChromePasswordCheckManager> _manager;
  // Pref service.
  raw_ptr<PrefService> _prefService;
  // Sync service.
  raw_ptr<syncer::SyncService> _syncService;
  // Used to create and run validation tasks.
  std::unique_ptr<base::CancelableTaskTracker> _validationTaskTracker;
}

// Delegate for this mediator.
@property(nonatomic, weak) id<AddPasswordMediatorDelegate> delegate;

// Task runner on which validation operations happen.
@property(nonatomic, assign) scoped_refptr<base::SequencedTaskRunner>
    sequencedTaskRunner;

// Stores the url entered in the website field.
@property(nonatomic, assign) GURL URL;

@end

@implementation AddPasswordMediator

- (instancetype)initWithDelegate:(id<AddPasswordMediatorDelegate>)delegate
            passwordCheckManager:(IOSChromePasswordCheckManager*)manager
                     prefService:(PrefService*)prefService
                     syncService:(syncer::SyncService*)syncService {
  DCHECK(delegate);
  DCHECK(manager);
  DCHECK(prefService);
  DCHECK(syncService);
  self = [super init];
  if (self) {
    _delegate = delegate;
    _manager = manager;
    _prefService = prefService;
    _syncService = syncService;
    _sequencedTaskRunner = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_BLOCKING});
    _validationTaskTracker = std::make_unique<base::CancelableTaskTracker>();
  }
  return self;
}

- (void)setConsumer:(id<AddPasswordDetailsConsumer>)consumer {
  if (_consumer == consumer)
    return;
  _consumer = consumer;
  std::optional<std::string> account =
      password_manager::sync_util::GetAccountForSaving(_prefService,
                                                       _syncService);
  if (account) {
    CHECK(!account->empty());
    [_consumer setAccountSavingPasswords:base::SysUTF8ToNSString(*account)];
  } else {
    [_consumer setAccountSavingPasswords:nil];
  }
}

- (void)dealloc {
  _validationTaskTracker->TryCancelAll();
  _validationTaskTracker.reset();
}

#pragma mark - AddPasswordViewControllerDelegate

- (void)addPasswordViewController:(AddPasswordViewController*)viewController
            didAddPasswordDetails:(NSString*)username
                         password:(NSString*)password
                             note:(NSString*)note {
  if (_validationTaskTracker->HasTrackedTasks()) {
    // If the task tracker has pending tasks and the "Save" button is pressed,
    // don't do anything.
    return;
  }

  DCHECK([self isURLValid]);

  password_manager::CredentialUIEntry credential;
  std::string signonRealm = password_manager::GetSignonRealm(self.URL);
  credential.username = SysNSStringToUTF16(username);
  credential.password = SysNSStringToUTF16(password);
  credential.note = SysNSStringToUTF16(note);
  credential.stored_in = {
      password_manager::features_util::GetDefaultPasswordStore(_prefService,
                                                               _syncService)};

  password_manager::CredentialFacet facet;
  facet.url = self.URL;
  facet.signon_realm = signonRealm;
  credential.facets.push_back(std::move(facet));

  _manager->GetSavedPasswordsPresenter()->AddCredential(credential);
  [self.delegate setUpdatedPassword:credential];
  [self.delegate dismissAddPasswordTableViewController];
}

- (void)checkForDuplicates:(NSString*)username {
  _validationTaskTracker->TryCancelAll();
  if (![self isURLValid]) {
    return;
  }

  __weak __typeof(self) weakSelf = self;
  _validationTaskTracker->PostTaskAndReplyWithResult(
      _sequencedTaskRunner.get(), FROM_HERE,
      base::BindOnce(
          &CheckForDuplicates, self.URL, username,
          _manager->GetSavedPasswordsPresenter()->GetSavedCredentials()),
      base::BindOnce(^(bool duplicateFound) {
        [weakSelf.consumer onDuplicateCheckCompletion:duplicateFound];
      }));
}

- (void)showExistingCredential:(NSString*)username {
  if (![self isURLValid]) {
    return;
  }

  std::string signon_realm = password_manager::GetSignonRealm(
      password_manager_util::StripAuthAndParams(self.URL));
  std::u16string username_value = SysNSStringToUTF16(username);
  for (const auto& credential :
       _manager->GetSavedPasswordsPresenter()->GetSavedCredentials()) {
    if (credential.GetFirstSignonRealm() == signon_realm &&
        credential.username == username_value) {
      [self.delegate showPasswordDetailsControllerWithCredential:credential];
      return;
    }
  }
  NOTREACHED_IN_MIGRATION();
}

- (void)didCancelAddPasswordDetails {
  [self.delegate dismissAddPasswordTableViewController];
}

- (void)setWebsiteURL:(NSString*)website {
  self.URL = password_manager_util::ConstructGURLWithScheme(
      SysNSStringToUTF8(website));
}

- (BOOL)isURLValid {
  return self.URL.is_valid() && self.URL.SchemeIsHTTPOrHTTPS();
}

- (BOOL)isTLDMissing {
  std::string hostname = self.URL.host();
  return !base::Contains(hostname, '.');
}

@end
