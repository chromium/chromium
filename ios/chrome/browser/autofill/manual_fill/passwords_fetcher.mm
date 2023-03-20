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
  // The object to observe changes in the profile Password Store.
  std::unique_ptr<PasswordStoreObserverBridge> _profileStoreObserver;
  // URL to fetch logins for. May be empty if no filtering is needed.
  GURL _URL;
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
    _savedPasswordsConsumer.reset(new ios::SavePasswordsConsumer(self));
    _profileStoreObserver.reset(new PasswordStoreObserverBridge(self));
    _profilePasswordStore->AddObserver(_profileStoreObserver.get());
    _URL = URL;
    [self fetchLogins];
  }
  return self;
}

- (void)dealloc {
  _profilePasswordStore->RemoveObserver(_profileStoreObserver.get());
}

#pragma mark - Private methods

- (void)fetchLogins {
  if (_URL.is_empty()) {
    _profilePasswordStore->GetAutofillableLogins(
        _savedPasswordsConsumer->GetWeakPtr());
  } else {
    password_manager::PasswordFormDigest digest = {
        password_manager::PasswordForm::Scheme::kHtml, std::string(), _URL};
    digest.signon_realm = _URL.spec();
    _profilePasswordStore->GetLogins(digest,
                                     _savedPasswordsConsumer->GetWeakPtr());
  }
}

#pragma mark - SavePasswordsConsumerDelegate

- (void)onGetPasswordStoreResults:
    (std::vector<std::unique_ptr<password_manager::PasswordForm>>)results {
  // Filter out Android facet IDs and any blocked passwords.
  base::EraseIf(results, [](const auto& form) {
    return form->blocked_by_user ||
           password_manager::IsValidAndroidFacetURI(form->signon_realm);
  });

  password_manager::DuplicatesMap savedPasswordDuplicates;
  password_manager::SortEntriesAndHideDuplicates(&results,
                                                 &savedPasswordDuplicates);
  [self.delegate passwordFetcher:self didFetchPasswords:std::move(results)];
}

#pragma mark - PasswordStoreObserver

- (void)loginsDidChange {
  [self fetchLogins];
}

@end
