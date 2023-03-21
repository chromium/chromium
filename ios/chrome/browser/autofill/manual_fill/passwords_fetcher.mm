// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/manual_fill/passwords_fetcher.h"

#import "base/containers/cxx20_erase.h"
#import "components/password_manager/core/browser/affiliation/affiliation_utils.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_list_sorter.h"
#import "components/password_manager/core/browser/password_store_consumer.h"
#import "components/password_manager/core/browser/password_store_interface.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/passwords/password_store_observer_bridge.h"
#import "ios/chrome/browser/passwords/save_passwords_consumer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PasswordFetcher () <SavePasswordsConsumerDelegate,
                               PasswordStoreObserver> {
  // The interfaces for getting and manipulating a user's saved passwords.
  scoped_refptr<password_manager::PasswordStoreInterface> _profilePasswordStore;
  scoped_refptr<password_manager::PasswordStoreInterface> _accountPasswordStore;
  // A helper object for passing data about saved passwords from a finished
  // password store request to the SavePasswordsCollectionViewController.
  std::unique_ptr<ios::SavePasswordsConsumer> _savedPasswordsConsumer;
  // The objects to observe changes in the Password Stores.
  std::unique_ptr<PasswordStoreObserverBridge> _profileStoreObserver;
  std::unique_ptr<PasswordStoreObserverBridge> _accountStoreObserver;
  // URL to fetch logins for. May be empty if no filtering is needed.
  GURL _URL;
  // Number of fetchLoginsFromStore: calls that haven't called back yet.
  NSInteger _numPendingStoreFetches;
  // Last results of fetchLoginsFromStore: for `_profilePasswordStore`.
  std::vector<std::unique_ptr<password_manager::PasswordForm>>
      _lastFetchedProfileStoreLogins;
  // Last results of fetchLoginsFromStore: for `_accountPasswordStore`.
  std::vector<std::unique_ptr<password_manager::PasswordForm>>
      _lastFetchedAccountStoreLogins;
}

// Delegate to send the fetchted passwords.
@property(nonatomic, weak) id<PasswordFetcherDelegate> delegate;

@end

@implementation PasswordFetcher

@synthesize delegate = _delegate;

#pragma mark - Initialization

- (instancetype)
    initWithProfilePasswordStore:
        (scoped_refptr<password_manager::PasswordStoreInterface>)
            profilePasswordStore
            accountPasswordStore:
                (scoped_refptr<password_manager::PasswordStoreInterface>)
                    accountPasswordStore
                        delegate:(id<PasswordFetcherDelegate>)delegate
                             URL:(const GURL&)URL {
  DCHECK(profilePasswordStore);
  DCHECK(delegate);
  self = [super init];
  if (self) {
    _delegate = delegate;
    _profilePasswordStore = profilePasswordStore;
    _accountPasswordStore = accountPasswordStore;
    _savedPasswordsConsumer =
        std::make_unique<ios::SavePasswordsConsumer>(self);
    _URL = URL;

    _profileStoreObserver = std::make_unique<PasswordStoreObserverBridge>(self);
    _profilePasswordStore->AddObserver(_profileStoreObserver.get());
    [self fetchLoginsFromStore:_profilePasswordStore.get()];
    if (_accountPasswordStore) {
      _accountStoreObserver =
          std::make_unique<PasswordStoreObserverBridge>(self);
      _accountPasswordStore->AddObserver(_accountStoreObserver.get());
      [self fetchLoginsFromStore:_accountPasswordStore.get()];
    }
  }
  return self;
}

- (void)dealloc {
  _profilePasswordStore->RemoveObserver(_profileStoreObserver.get());
  if (_accountPasswordStore) {
    _accountPasswordStore->RemoveObserver(_accountStoreObserver.get());
  }
}

#pragma mark - Private methods

- (void)fetchLoginsFromStore:(password_manager::PasswordStoreInterface*)store {
  CHECK_GE(_numPendingStoreFetches, 0);
  _numPendingStoreFetches++;

  if (_URL.is_empty()) {
    store->GetAutofillableLogins(_savedPasswordsConsumer->GetWeakPtr());
  } else {
    store->GetLogins(
        {password_manager::PasswordForm::Scheme::kHtml, _URL.spec(), _URL},
        _savedPasswordsConsumer->GetWeakPtr());
  }
}

#pragma mark - SavePasswordsConsumerDelegate

- (void)
    onGetPasswordStoreResults:
        (std::vector<std::unique_ptr<password_manager::PasswordForm>>)results
                    fromStore:(password_manager::PasswordStoreInterface*)store {
  _numPendingStoreFetches--;
  CHECK_GE(_numPendingStoreFetches, 0);

  // Filter out Android facet IDs and any blocked passwords.
  base::EraseIf(results, [](const auto& form) {
    return form->blocked_by_user ||
           password_manager::IsValidAndroidFacetURI(form->signon_realm);
  });

  if (store == _profilePasswordStore) {
    _lastFetchedProfileStoreLogins = std::move(results);
  } else {
    _lastFetchedAccountStoreLogins = std::move(results);
  }

  if (_numPendingStoreFetches > 0) {
    // Wait for other fetches to complete before sending results.
    return;
  }

  std::vector<std::unique_ptr<password_manager::PasswordForm>> combinedResults;
  combinedResults.reserve(_lastFetchedProfileStoreLogins.size() +
                          _lastFetchedAccountStoreLogins.size());
  for (const std::unique_ptr<password_manager::PasswordForm>& form :
       _lastFetchedProfileStoreLogins) {
    combinedResults.push_back(
        std::make_unique<password_manager::PasswordForm>(*form));
  }
  for (const std::unique_ptr<password_manager::PasswordForm>& form :
       _lastFetchedAccountStoreLogins) {
    combinedResults.push_back(
        std::make_unique<password_manager::PasswordForm>(*form));
  }
  password_manager::DuplicatesMap savedPasswordDuplicates;
  password_manager::SortEntriesAndHideDuplicates(&combinedResults,
                                                 &savedPasswordDuplicates);
  [self.delegate passwordFetcher:self
               didFetchPasswords:std::move(combinedResults)];
}

#pragma mark - PasswordStoreObserver

- (void)loginsDidChangeInStore:
    (password_manager::PasswordStoreInterface*)store {
  [self fetchLoginsFromStore:store];
}

@end
