// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_password_mediator.h"

#import <vector>

#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/form_util/form_activity_observer_bridge.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/password_manager/core/browser/password_manager_client.h"
#import "components/password_manager/core/browser/password_store/password_store_interface.h"
#import "components/sync/base/model_type.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/browser/autofill/model/manual_fill/passwords_fetcher.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/passwords/model/password_tab_helper.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_action_cell.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_content_injector.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_credential+PasswordForm.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_credential.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_password_cell.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/password_consumer.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/password_list_navigator.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/gfx/favicon_size.h"
#import "url/gurl.h"

namespace manual_fill {

NSString* const ManagePasswordsAccessibilityIdentifier =
    @"kManualFillManagePasswordsAccessibilityIdentifier";
NSString* const ManageSettingsAccessibilityIdentifier =
    @"kManualFillManageSettingsAccessibilityIdentifier";
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

@interface ManualFillPasswordMediator () <CRWWebStateObserver,
                                          FormActivityObserver,
                                          ManualFillContentInjector,
                                          PasswordFetcherDelegate> {
  // The interfaces for getting and manipulating a user's saved passwords.
  scoped_refptr<password_manager::PasswordStoreInterface> _profilePasswordStore;
  scoped_refptr<password_manager::PasswordStoreInterface> _accountPasswordStore;
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

// YES if the active field is of type 'password'.
@property(nonatomic, assign) BOOL activeFieldIsPassword;

// The relevant active web state.
@property(nonatomic, assign) web::WebState* webState;

// Sync service.
@property(nonatomic, assign) syncer::SyncService* syncService;

@end

@implementation ManualFillPasswordMediator {
  // Bridge to observe the web state from Objective-C.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;

  // Bridge to observe form activity in `_webState`.
  std::unique_ptr<autofill::FormActivityObserverBridge>
      _formActivityObserverBridge;

  // Origin to fetch passwords for.
  GURL _URL;
}

- (instancetype)
    initWithProfilePasswordStore:
        (scoped_refptr<password_manager::PasswordStoreInterface>)
            profilePasswordStore
            accountPasswordStore:
                (scoped_refptr<password_manager::PasswordStoreInterface>)
                    accountPasswordStore
                   faviconLoader:(FaviconLoader*)faviconLoader
                        webState:(web::WebState*)webState
                     syncService:(syncer::SyncService*)syncService
                             URL:(const GURL&)URL
          invokedOnPasswordField:(BOOL)invokedOnPasswordField {
  self = [super init];
  if (self) {
    _credentials = @[];
    _profilePasswordStore = profilePasswordStore;
    _accountPasswordStore = accountPasswordStore;
    _faviconLoader = faviconLoader;
    _webState = webState;
    _syncService = syncService;
    _URL = URL;
    _activeFieldIsPassword = invokedOnPasswordField;
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _webState->AddObserver(_webStateObserverBridge.get());
    _formActivityObserverBridge =
        std::make_unique<autofill::FormActivityObserverBridge>(_webState, self);
  }
  return self;
}

- (void)dealloc {
  if (_webState) {
    [self webStateDestroyed:_webState];
  }
}

- (void)fetchPasswords {
  self.credentials = @[];
  self.passwordFetcher = [[PasswordFetcher alloc]
      initWithProfilePasswordStore:_profilePasswordStore
              accountPasswordStore:_accountPasswordStore
                          delegate:self
                               URL:_URL];
}

#pragma mark - PasswordFetcherDelegate

- (void)passwordFetcher:(PasswordFetcher*)passwordFetcher
      didFetchPasswords:
          (std::vector<std::unique_ptr<password_manager::PasswordForm>>)
              passwords {
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
    NSArray<ManualFillCredentialItem*>* credentials =
        [self createItemsForCredentials:self.credentials];
    [self.consumer presentCredentials:credentials];
    return;
  }

  NSPredicate* predicate = [NSPredicate
      predicateWithFormat:@"host CONTAINS[cd] %@ OR username CONTAINS[cd] %@",
                          searchText, searchText];
  NSArray* filteredCredentials =
      [self.credentials filteredArrayUsingPredicate:predicate];
  NSArray<ManualFillCredentialItem*>* credentials =
      [self createItemsForCredentials:filteredCredentials];
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

// Posts the credentials to the consumer. If filtered is `YES` it only post the
// ones associated with the active web state.
- (void)postCredentialsToConsumer {
  if (!self.consumer) {
    return;
  }
  NSArray<ManualFillCredentialItem*>* credentials =
      [self createItemsForCredentials:self.credentials];
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
    ManualFillCredentialItem* item = [[ManualFillCredentialItem alloc]
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

    password_manager::PasswordManagerClient* passwordManagerClient =
        _webState ? PasswordTabHelper::FromWebState(_webState)
                        ->GetPasswordManagerClient()
                  : nullptr;
    if (_syncService &&
        _syncService->GetActiveDataTypes().Has(syncer::PASSWORDS) &&
        passwordManagerClient &&
        passwordManagerClient->IsSavingAndFillingEnabled(_URL) &&
        _activeFieldIsPassword) {
      NSString* suggestPasswordTitleString = l10n_util::GetNSString(
          IDS_IOS_MANUAL_FALLBACK_SUGGEST_STRONG_PASSWORD_WITH_DOTS);
      ManualFillActionItem* suggestPasswordItem = [[ManualFillActionItem alloc]
          initWithTitle:suggestPasswordTitleString
                 action:^{
                   base::RecordAction(base::UserMetricsAction(
                       "ManualFallback_Password_OpenSuggestPassword"));
                   [weakSelf.navigator openPasswordSuggestion];
                 }];
      suggestPasswordItem.accessibilityIdentifier =
          manual_fill::SuggestPasswordAccessibilityIdentifier;
      [actions addObject:suggestPasswordItem];
    }

    NSString* otherPasswordsTitleString = l10n_util::GetNSString(
        IDS_IOS_MANUAL_FALLBACK_SELECT_PASSWORD_WITH_DOTS);
    ManualFillActionItem* otherPasswordsItem = [[ManualFillActionItem alloc]
        initWithTitle:otherPasswordsTitleString
               action:^{
                 base::RecordAction(base::UserMetricsAction(
                     "ManualFallback_Password_OpenOtherPassword"));
                 [weakSelf.navigator openAllPasswordsList];
               }];
    otherPasswordsItem.accessibilityIdentifier =
        manual_fill::OtherPasswordsAccessibilityIdentifier;
    [actions addObject:otherPasswordsItem];

    // "Manage Passwords..." is available in both configurations.
    NSString* managePasswordsTitle =
        l10n_util::GetNSString(IDS_IOS_MANUAL_FALLBACK_MANAGE_PASSWORDS);
    ManualFillActionItem* managePasswordsItem = [[ManualFillActionItem alloc]
        initWithTitle:managePasswordsTitle
               action:^{
                 base::RecordAction(base::UserMetricsAction(
                     "ManualFallback_Password_OpenManagePassword"));
                 [weakSelf.navigator openPasswordManager];
               }];
    managePasswordsItem.accessibilityIdentifier =
        manual_fill::ManagePasswordsAccessibilityIdentifier;
    [actions addObject:managePasswordsItem];

    NSString* manageSettingsTitle =
        l10n_util::GetNSString(IDS_IOS_MANUAL_FALLBACK_MANAGE_SETTINGS);
    ManualFillActionItem* manageSettingsItem = [[ManualFillActionItem alloc]
        initWithTitle:manageSettingsTitle
               action:^{
                 base::RecordAction(base::UserMetricsAction(
                     "ManualFallback_Password_OpenManageSettings"));
                 [weakSelf.navigator openPasswordSettings];
               }];
    manageSettingsItem.accessibilityIdentifier =
        manual_fill::ManageSettingsAccessibilityIdentifier;

    [actions addObject:manageSettingsItem];

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

- (void)faviconForPageURL:(CrURL*)URL
               completion:(void (^)(FaviconAttributes*))completion {
  DCHECK(completion);
  self.faviconLoader->FaviconForPageUrlOrHost(URL.gurl, gfx::kFaviconSize,
                                              completion);
}

#pragma mark - FormActivityObserver

- (void)webState:(web::WebState*)webState
    didRegisterFormActivity:(const autofill::FormActivityParams&)params
                    inFrame:(web::WebFrame*)frame {
  DCHECK_EQ(_webState, webState);
  if (_activeFieldIsPassword !=
      (params.field_type == autofill::kPasswordFieldType)) {
    _activeFieldIsPassword = params.field_type == autofill::kPasswordFieldType;
    [self postActionsToConsumer];
  }
}

#pragma mark - CRWWebStateObserver

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
    _webState = nullptr;
  }
  _webStateObserverBridge.reset();
  _formActivityObserverBridge.reset();
}

@end
