// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_password_mediator.h"

#include <vector>

#include "base/metrics/user_metrics.h"
#include "base/strings/sys_string_conversions.h"
#include "components/password_manager/core/browser/password_store.h"
#import "ios/chrome/browser/autofill/manual_fill/passwords_fetcher.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#include "ios/chrome/browser/passwords/password_manager_features.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/action_cell.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/credential.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/credential_password_form.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_content_injector.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_password_cell.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/password_consumer.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/password_list_navigator.h"
#import "ios/chrome/browser/ui/list_model/list_model.h"
#import "ios/chrome/browser/ui/table_view/table_view_model.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/gfx/favicon_size.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Minimum favicon size to retrieve.
const CGFloat kMinFaviconSizePt = 8.0;
}  // namespace

namespace manual_fill {

NSString* const ManagePasswordsAccessibilityIdentifier =
    @"kManualFillManagePasswordsAccessibilityIdentifier";
NSString* const OtherPasswordsAccessibilityIdentifier =
    @"kManualFillOtherPasswordsAccessibilityIdentifier";
NSString* const SuggestPasswordAccessibilityIdentifier =
    @"kManualFillSuggestPasswordAccessibilityIdentifier";

}  // namespace manual_fill

// Checks if two credential are connected. They are considered connected if they
// have same host.
BOOL AreCredentialsAtIndexesConnected(
    NSArray<ManualFillCredential*>* credentials,
    int firstIndex,
    int secondIndex) {
  if (firstIndex < 0 || firstIndex >= (int)credentials.count ||
      secondIndex < 0 || secondIndex >= (int)credentials.count)
    return NO;
  return [credentials[firstIndex].host
      isEqualToString:credentials[secondIndex].host];
}

@interface ManualFillPasswordMediator () <ManualFillContentInjector,
                                          PasswordFetcherDelegate> {
  // The interface for getting and manipulating a user's saved passwords.
  scoped_refptr<password_manager::PasswordStore> _passwordStore;
}

// The password fetcher to query the user profile.
@property(nonatomic, strong) PasswordFetcher* passwordFetcher;

// The favicon loader used in TableViewFaviconDataSource.
@property(nonatomic, assign) FaviconLoader* faviconLoader;

// A cache of the credentials fetched from the store, not synced. Useful to
// reuse the mediator.
@property(nonatomic, strong) NSArray<ManualFillCredential*>* credentials;

// YES if the password fetcher has completed at least one fetch.
@property(nonatomic, assign) BOOL passwordFetcherDidFetch;

@end

@implementation ManualFillPasswordMediator

- (instancetype)initWithPasswordStore:
                    (scoped_refptr<password_manager::PasswordStore>)
                        passwordStore
                        faviconLoader:(FaviconLoader*)faviconLoader {
  self = [super init];
  if (self) {
    _credentials = @[];
    _passwordStore = passwordStore;
    _faviconLoader = faviconLoader;
  }
  return self;
}

- (void)fetchPasswordsForURL:(const GURL&)URL {
  self.credentials = @[];
  self.passwordFetcher =
      [[PasswordFetcher alloc] initWithPasswordStore:_passwordStore
                                            delegate:self
                                                 URL:URL];
}

#pragma mark - PasswordFetcherDelegate

- (void)passwordFetcher:(PasswordFetcher*)passwordFetcher
      didFetchPasswords:
          (std::vector<std::unique_ptr<autofill::PasswordForm>>)passwords {
  NSMutableArray<ManualFillCredential*>* credentials =
      [[NSMutableArray alloc] initWithCapacity:passwords.size()];
  for (const auto& form : passwords) {
    ManualFillCredential* credential =
        [[ManualFillCredential alloc] initWithPasswordForm:*form];
    [credentials addObject:credential];
  }
  self.credentials = credentials;
  self.passwordFetcherDidFetch = YES;
  [self postDataToConsumer];
}

#pragma mark - UISearchResultsUpdating

- (void)updateSearchResultsForSearchController:
    (UISearchController*)searchController {
  NSString* searchText = searchController.searchBar.text;
  if (!searchText.length) {
    auto credentials = [self createItemsForCredentials:self.credentials];
    [self.consumer presentCredentials:credentials];
    return;
  }

  NSPredicate* predicate = [NSPredicate
      predicateWithFormat:@"host CONTAINS[cd] %@ OR username CONTAINS[cd] %@",
                          searchText, searchText];
  NSArray* filteredCredentials =
      [self.credentials filteredArrayUsingPredicate:predicate];
  auto credentials = [self createItemsForCredentials:filteredCredentials];
  [self.consumer presentCredentials:credentials];
}

#pragma mark - Private

- (void)postDataToConsumer {
  // To avoid duplicating the metric tracking how many passwords are sent to the
  // consumer, only post credentials if at least the first fetch is done. Or
  // else there will be spam metrics with 0 passwords everytime the screen is
  // open.
  if (self.passwordFetcherDidFetch) {
    [self postCredentialsToConsumer];
    [self postActionsToConsumer];
  }
}

// Posts the credentials to the consumer. If filtered is |YES| it only post the
// ones associated with the active web state.
- (void)postCredentialsToConsumer {
  if (!self.consumer) {
    return;
  }
  auto credentials = [self createItemsForCredentials:self.credentials];
  [self.consumer presentCredentials:credentials];
}

// Creates a table view model with the passed credentials.
- (NSArray<ManualFillCredentialItem*>*)createItemsForCredentials:
    (NSArray<ManualFillCredential*>*)credentials {
  NSMutableArray* items =
      [[NSMutableArray alloc] initWithCapacity:credentials.count];
  for (int i = 0; i < (int)credentials.count; i++) {
    BOOL isConnectedToPreviousItem =
        AreCredentialsAtIndexesConnected(credentials, i, i - 1);
    BOOL isConnectedToNextItem =
        AreCredentialsAtIndexesConnected(credentials, i, i + 1);
    ManualFillCredential* credential = credentials[i];
    auto item = [[ManualFillCredentialItem alloc]
               initWithCredential:credential
        isConnectedToPreviousItem:isConnectedToPreviousItem
            isConnectedToNextItem:isConnectedToNextItem
                  contentInjector:self];
    [items addObject:item];
  }
  return items;
}

- (void)postActionsToConsumer {
  if (!self.consumer) {
    return;
  }
  if (self.isActionSectionEnabled) {
    NSMutableArray<ManualFillActionItem*>* actions =
        [[NSMutableArray alloc] init];
    __weak __typeof(self) weakSelf = self;

    NSString* otherPasswordsTitleString = l10n_util::GetNSString(
        IDS_IOS_MANUAL_FALLBACK_USE_OTHER_PASSWORD_WITH_DOTS);
    auto otherPasswordsItem = [[ManualFillActionItem alloc]
        initWithTitle:otherPasswordsTitleString
               action:^{
                 base::RecordAction(base::UserMetricsAction(
                     "ManualFallback_Password_OpenOtherPassword"));
                 [weakSelf.navigator openAllPasswordsList];
               }];
    otherPasswordsItem.accessibilityIdentifier =
        manual_fill::OtherPasswordsAccessibilityIdentifier;
    [actions addObject:otherPasswordsItem];

    NSString* managePasswordsTitle =
        l10n_util::GetNSString(IDS_IOS_MANUAL_FALLBACK_MANAGE_PASSWORDS);
    auto managePasswordsItem = [[ManualFillActionItem alloc]
        initWithTitle:managePasswordsTitle
               action:^{
                 base::RecordAction(base::UserMetricsAction(
                     "ManualFallback_Password_OpenManagePassword"));
                 [weakSelf.navigator openPasswordSettings];
               }];
    managePasswordsItem.accessibilityIdentifier =
        manual_fill::ManagePasswordsAccessibilityIdentifier;
    [actions addObject:managePasswordsItem];

    [self.consumer presentActions:actions];
  }
}

#pragma mark - Setters

- (void)setConsumer:(id<ManualFillPasswordConsumer>)consumer {
  if (consumer == _consumer) {
    return;
  }
  _consumer = consumer;
  [self postDataToConsumer];
}

#pragma mark - ManualFillContentInjector

- (BOOL)canUserInjectInPasswordField:(BOOL)passwordField
                       requiresHTTPS:(BOOL)requiresHTTPS {
  return [self.contentInjector canUserInjectInPasswordField:passwordField
                                              requiresHTTPS:requiresHTTPS];
}

- (void)userDidPickContent:(NSString*)content
             passwordField:(BOOL)passwordField
             requiresHTTPS:(BOOL)requiresHTTPS {
  [self.delegate manualFillPasswordMediatorWillInjectContent:self];
  [self.contentInjector userDidPickContent:content
                             passwordField:passwordField
                             requiresHTTPS:requiresHTTPS];
}

#pragma mark - TableViewFaviconDataSource

- (void)faviconForURL:(const GURL&)URL
           completion:(void (^)(FaviconAttributes*))completion {
  DCHECK(completion);
  self.faviconLoader->FaviconForPageUrl(
      URL, gfx::kFaviconSize, kMinFaviconSizePt,
      /*fallback_to_google_server=*/false, completion);
}

@end
