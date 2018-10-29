// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/password_mediator.h"

#include <vector>

#include "base/strings/sys_string_conversions.h"
#include "components/password_manager/core/browser/password_store.h"
#import "ios/chrome/browser/autofill/manual_fill/passwords_fetcher.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/action_cell.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/credential.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/credential_password_form.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_content_delegate.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_password_cell.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/password_consumer.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/password_list_delegate.h"
#import "ios/chrome/browser/ui/list_model/list_model.h"
#import "ios/chrome/browser/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state/web_state.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace manual_fill {

NSString* const ManagePasswordsAccessibilityIdentifier =
    @"kManualFillManagePasswordsAccessibilityIdentifier";
NSString* const OtherPasswordsAccessibilityIdentifier =
    @"kManualFillOtherPasswordsAccessibilityIdentifier";

}  // namespace manual_fill

@interface ManualFillPasswordMediator ()<ManualFillContentDelegate,
                                         PasswordFetcherDelegate>
// The |WebStateList| containing the active web state. Used to filter the list
// of credentials based on the active web state.
@property(nonatomic, assign) WebStateList* webStateList;
// The password fetcher to query the user profile.
@property(nonatomic, strong) PasswordFetcher* passwordFetcher;
// A cache of the credentials fetched from the store, not synced. Useful to
// reuse the mediator.
@property(nonatomic, strong) NSArray<ManualFillCredential*>* credentials;
// If the filter is disabled, the "Show All Passwords" button is not included
// in the model.
@property(nonatomic, assign, readonly) BOOL isAllPasswordButtonEnabled;

@end

@implementation ManualFillPasswordMediator

@synthesize consumer = _consumer;
@synthesize contentDelegate = _contentDelegate;
@synthesize credentials = _credentials;
@synthesize disableFilter = _disableFilter;
@synthesize navigationDelegate = _navigationDelegate;
@synthesize passwordFetcher = _passwordFetcher;
@synthesize webStateList = _webStateList;

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                       passwordStore:
                           (scoped_refptr<password_manager::PasswordStore>)
                               passwordStore {
  self = [super init];
  if (self) {
    _credentials = @[];
    _webStateList = webStateList;
    _passwordFetcher =
        [[PasswordFetcher alloc] initWithPasswordStore:passwordStore
                                              delegate:self];
  }
  return self;
}

- (void)setConsumer:(id<ManualFillPasswordConsumer>)consumer {
  if (consumer == _consumer) {
    return;
  }
  _consumer = consumer;
  [self postCredentialsToConsumer];
  [self postActionsToConsumer];
}

#pragma mark - PasswordFetcherDelegate

- (void)passwordFetcher:(PasswordFetcher*)passwordFetcher
      didFetchPasswords:
          (std::vector<std::unique_ptr<autofill::PasswordForm>>&)passwords {
  NSMutableArray<ManualFillCredential*>* credentials =
      [[NSMutableArray alloc] initWithCapacity:passwords.size()];
  for (auto it = passwords.begin(); it != passwords.end(); ++it) {
    autofill::PasswordForm& form = **it;
    ManualFillCredential* credential =
        [[ManualFillCredential alloc] initWithPasswordForm:form];
    [credentials addObject:credential];
  }
  self.credentials = credentials;
  [self postCredentialsToConsumer];
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

// Posts the credentials to the consumer. If filtered is |YES| it only post the
// ones associated with the active web state.
- (void)postCredentialsToConsumer {
  if (!self.consumer) {
    return;
  }
  if (self.disableFilter) {
    auto credentials = [self createItemsForCredentials:self.credentials];
    [self.consumer presentCredentials:credentials];
    return;
  }
  web::WebState* currentWebState = self.webStateList->GetActiveWebState();
  if (!currentWebState) {
    return;
  }
  GURL visibleURL = currentWebState->GetVisibleURL();
  std::string site_name =
      net::registry_controlled_domains::GetDomainAndRegistry(
          visibleURL.host(),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  NSString* siteName = base::SysUTF8ToNSString(site_name);

  NSPredicate* predicate =
      [NSPredicate predicateWithFormat:@"siteName = %@", siteName];
  NSArray* filteredCredentials =
      [self.credentials filteredArrayUsingPredicate:predicate];
  auto credentials = [self createItemsForCredentials:filteredCredentials];
  [self.consumer presentCredentials:credentials];
}

// Creates a table view model with the passed credentials.
- (NSArray<ManualFillCredentialItem*>*)createItemsForCredentials:
    (NSArray<ManualFillCredential*>*)credentials {
  NSMutableArray* items =
      [[NSMutableArray alloc] initWithCapacity:credentials.count];
  for (ManualFillCredential* credential in credentials) {
    auto item = [[ManualFillCredentialItem alloc] initWithCredential:credential
                                                            delegate:self];
    [items addObject:item];
  }
  return items;
}

- (void)postActionsToConsumer {
  if (!self.consumer) {
    return;
  }
  if (self.isAllPasswordButtonEnabled) {
    NSString* otherPasswordsTitleString = l10n_util::GetNSString(
        IDS_IOS_MANUAL_FALLBACK_USE_OTHER_PASSWORD_WITH_DOTS);
    __weak __typeof(self) weakSelf = self;

    auto otherPasswordsItem = [[ManualFillActionItem alloc]
        initWithTitle:otherPasswordsTitleString
               action:^{
                 [weakSelf.navigationDelegate openAllPasswordsList];
               }];
    otherPasswordsItem.accessibilityIdentifier =
        manual_fill::OtherPasswordsAccessibilityIdentifier;

    NSString* managePasswordsTitle =
        l10n_util::GetNSString(IDS_IOS_MANUAL_FALLBACK_MANAGE_PASSWORDS);
    auto managePasswordsItem = [[ManualFillActionItem alloc]
        initWithTitle:managePasswordsTitle
               action:^{
                 [weakSelf.navigationDelegate openPasswordSettings];
               }];
    managePasswordsItem.accessibilityIdentifier =
        manual_fill::ManagePasswordsAccessibilityIdentifier;
    [self.consumer presentActions:@[ otherPasswordsItem, managePasswordsItem ]];
  } else {
    [self.consumer presentActions:@[]];
  }
}

#pragma mark - Getters

- (BOOL)isAllPasswordButtonEnabled {
  return !self.disableFilter;
}

#pragma mark - ManualFillContentDelegate

- (void)userDidPickContent:(NSString*)content isSecure:(BOOL)isSecure {
  [self.navigationDelegate dismissPresentedViewController];
  [self.contentDelegate userDidPickContent:content isSecure:isSecure];
}

@end
