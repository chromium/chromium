// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_password_mediator.h"

#import <vector>

#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/form_util/form_activity_observer_bridge.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/password_manager/core/browser/password_manager_client.h"
#import "components/password_manager/core/browser/password_save_manager_impl.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/password_manager/ios/ios_password_manager_driver_factory.h"
#import "components/sync/base/model_type.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/passwords/model/password_counter_delegate_bridge.h"
#import "ios/chrome/browser/passwords/model/password_tab_helper.h"
#import "ios/chrome/browser/shared/public/features/features.h"
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
#import "ios/chrome/browser/ui/menu/browser_action_factory.h"
#import "ios/chrome/browser/ui/settings/password/saved_passwords_presenter_observer.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/gfx/favicon_size.h"
#import "url/gurl.h"

using password_manager::PasswordForm;

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
// have the same host.
BOOL AreCredentialsAtIndicesConnected(
    NSArray<ManualFillCredential*>* credentials,
    int first_index,
    int second_index) {
  CHECK(!IsKeyboardAccessoryUpgradeEnabled());
  if (first_index < 0 || first_index >= (int)credentials.count ||
      second_index < 0 || second_index >= (int)credentials.count) {
    return NO;
  }

  return [credentials[first_index].host
      isEqualToString:credentials[second_index].host];
}

@interface ManualFillPasswordMediator () <CRWWebStateObserver,
                                          FormActivityObserver,
                                          ManualFillContentInjector,
                                          PasswordCounterObserver,
                                          SavedPasswordsPresenterObserver>

// The favicon loader used in TableViewFaviconDataSource.
@property(nonatomic, assign) FaviconLoader* faviconLoader;

// A cache of the saved credentials to present.
@property(nonatomic, strong) NSArray<ManualFillCredential*>* credentials;

// A cache of the password forms used to create ManualFillCredentials and
// ManualFillCredentialItems.
@property(nonatomic, assign) std::vector<PasswordForm> passwordForms;

// YES if passwords were fetched at least once.
@property(nonatomic, assign) BOOL passwordsWereFetched;

// YES if the active field is obfuscated.
@property(nonatomic, assign) BOOL activeFieldIsObfuscated;

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

  // Service which gives us a view on users' saved passwords. Only set this
  // property if there's a need to fetch all of the user's saved passwords.
  raw_ptr<password_manager::SavedPasswordsPresenter> _savedPasswordsPresenter;

  // Bridge to observe changes in saved passwords. This variable will only be
  // initialized if the `_savedPasswordsPresenter` variable is set.
  std::unique_ptr<SavedPasswordsPresenterObserverBridge>
      _savedPasswordsPresenterObserver;

  // Bridge to observe the number of passwords in the password stores.
  std::unique_ptr<PasswordCounterDelegateBridge> _passwordCounter;

  // Whether or not the user has passwords saved in the password stores.
  BOOL _hasSavedPasswords;
}

- (instancetype)initWithFaviconLoader:(FaviconLoader*)faviconLoader
                             webState:(web::WebState*)webState
                          syncService:(syncer::SyncService*)syncService
                                  URL:(const GURL&)URL
             invokedOnObfuscatedField:(BOOL)invokedOnObfuscatedField
                 profilePasswordStore:
                     (scoped_refptr<password_manager::PasswordStoreInterface>)
                         profilePasswordStore
                 accountPasswordStore:
                     (scoped_refptr<password_manager::PasswordStoreInterface>)
                         accountPasswordStore {
  self = [super init];
  if (self) {
    _credentials = @[];
    _passwordForms = {};
    _faviconLoader = faviconLoader;
    _webState = webState;
    _syncService = syncService;
    _URL = URL;
    _activeFieldIsObfuscated = invokedOnObfuscatedField;
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _webState->AddObserver(_webStateObserverBridge.get());
    _formActivityObserverBridge =
        std::make_unique<autofill::FormActivityObserverBridge>(_webState, self);

    // A valid `profilePasswordStore` is needed to observe PasswordCounter.
    if (IsKeyboardAccessoryUpgradeEnabled() && profilePasswordStore) {
      _passwordCounter = std::make_unique<PasswordCounterDelegateBridge>(
          self, profilePasswordStore.get(), accountPasswordStore.get());
    }
  }
  return self;
}

- (void)dealloc {
  [self disconnect];
}

- (void)disconnect {
  if (_webState) {
    [self webStateDestroyed:_webState];
  }
  if (_savedPasswordsPresenter) {
    _savedPasswordsPresenter->RemoveObserver(
        _savedPasswordsPresenterObserver.get());
    _savedPasswordsPresenterObserver.reset();
    _savedPasswordsPresenter = nullptr;
  }
  if (_passwordCounter) {
    _passwordCounter.reset();
  }
}

- (void)fetchPasswordsForForm:(const autofill::FormRendererId)formID
                        frame:(const std::string&)frameID {
  self.passwordForms = [self fetchCredentialsForForm:formID
                                            webState:self.webState
                                          webFrameId:frameID];
  self.credentials =
      [self createManualFillCredentialsFromPasswordForms:self.passwordForms];
  self.passwordsWereFetched = YES;
  [self postDataToConsumer];
}

- (void)fetchAllPasswords {
  CHECK(_savedPasswordsPresenter);

  std::vector<password_manager::CredentialUIEntry> savedCredentials =
      _savedPasswordsPresenter->GetSavedCredentials();
  self.passwordForms = [self passwordFormsFromCredentials:savedCredentials];
  self.credentials =
      [self createManualFillCredentialsFromPasswordForms:self.passwordForms];
  self.passwordsWereFetched = YES;
  [self postDataToConsumer];
}

#pragma mark - UISearchResultsUpdating

- (void)updateSearchResultsForSearchController:
    (UISearchController*)searchController {
  NSString* searchText = searchController.searchBar.text;
  if (!searchText.length) {
    NSArray<ManualFillCredentialItem*>* credentialItems =
        [self createItemsForCredentials:self.credentials];
    [self.consumer presentCredentials:credentialItems];
    return;
  }

  NSPredicate* predicate = [NSPredicate
      predicateWithFormat:@"host CONTAINS[cd] %@ OR username CONTAINS[cd] %@",
                          searchText, searchText];
  NSArray* filteredCredentials =
      [self.credentials filteredArrayUsingPredicate:predicate];
  NSArray<ManualFillCredentialItem*>* credentialItems =
      [self createItemsForCredentials:filteredCredentials];
  [self.consumer presentCredentials:credentialItems];
}

#pragma mark - Private

- (void)postDataToConsumer {
  // To avoid duplicating the metric tracking how many passwords are sent to the
  // consumer, only post credentials if at least the first fetch is done. Or
  // else there will be spam metrics with 0 passwords everytime the screen is
  // open.
  if (self.passwordsWereFetched) {
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
    // Credentials from the same affiliated group are never connected when the
    // Keyboard Accessory Upgrade feature is enabled.
    BOOL isConnectedToPreviousItem =
        IsKeyboardAccessoryUpgradeEnabled()
            ? NO
            : AreCredentialsAtIndicesConnected(credentials, i, i - 1);
    BOOL isConnectedToNextItem =
        IsKeyboardAccessoryUpgradeEnabled()
            ? NO
            : AreCredentialsAtIndicesConnected(credentials, i, i + 1);
    ManualFillCredential* credential = credentials[i];
    NSArray<UIAction*>* menuActions = IsKeyboardAccessoryUpgradeEnabled()
                                          ? @[ [self createMenuEditAction] ]
                                          : @[];

    ManualFillCredentialItem* item = [[ManualFillCredentialItem alloc]
               initWithCredential:credential
        isConnectedToPreviousItem:isConnectedToPreviousItem
            isConnectedToNextItem:isConnectedToNextItem
                  contentInjector:self
                      menuActions:menuActions];
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
        _activeFieldIsObfuscated) {
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

    if (!IsKeyboardAccessoryUpgradeEnabled() ||
        (IsKeyboardAccessoryUpgradeEnabled() && _hasSavedPasswords)) {
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
    }

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

// Fetches passwords related to the current form.
- (std::vector<PasswordForm>)
    fetchCredentialsForForm:(autofill::FormRendererId)formId
                   webState:(web::WebState*)webState
                 webFrameId:(const std::string&)frameId {
  PasswordTabHelper* tabHelper = PasswordTabHelper::FromWebState(webState);
  if (!tabHelper) {
    return {};
  }

  password_manager::PasswordManager* passwordManager =
      tabHelper->GetPasswordManager();
  CHECK(passwordManager);

  web::WebFramesManager* webFramesManager =
      autofill::AutofillJavaScriptFeature::GetInstance()->GetWebFramesManager(
          webState);
  web::WebFrame* frame = webFramesManager->GetFrameWithId(frameId);

  password_manager::PasswordManagerDriver* driver =
      IOSPasswordManagerDriverFactory::FromWebStateAndWebFrame(webState, frame);
  const base::span<const password_manager::PasswordForm> passwordForms =
      passwordManager->GetBestMatches(driver, formId);

  return std::vector<PasswordForm>(passwordForms.begin(), passwordForms.end());
}

// Fetches all Password Forms related to the given list of saved credentials.
- (std::vector<PasswordForm>)passwordFormsFromCredentials:
    (const std::vector<password_manager::CredentialUIEntry>&)credentials {
  std::vector<PasswordForm> passwordforms;
  passwordforms.reserve(credentials.size());

  for (const auto& credential : credentials) {
    std::vector<PasswordForm> correspondingPasswordForms =
        _savedPasswordsPresenter->GetCorrespondingPasswordForms(credential);
    passwordforms.insert(passwordforms.end(),
                         correspondingPasswordForms.begin(),
                         correspondingPasswordForms.end());
  }
  return passwordforms;
}

// Creates and returns a list of manual fill credentials built off of a list of
// password forms.
- (NSMutableArray<ManualFillCredential*>*)
    createManualFillCredentialsFromPasswordForms:
        (const std::vector<PasswordForm>&)passwordForms {
  NSMutableArray<ManualFillCredential*>* manualFillCredentials =
      [[NSMutableArray alloc] initWithCapacity:passwordForms.size()];
  for (const auto& passwordForm : passwordForms) {
    ManualFillCredential* manualFillCredential =
        [[ManualFillCredential alloc] initWithPasswordForm:passwordForm];
    [manualFillCredentials addObject:manualFillCredential];
  }

  return manualFillCredentials;
}

// Creates an "Edit" UIAction to be used with a UIMenu.
- (UIAction*)createMenuEditAction {
  MenuScenarioHistogram menuScenario =
      self.isActionSectionEnabled
          ? kMenuScenarioHistogramAutofillManualFallbackAllPasswordsEntry
          : kMenuScenarioHistogramAutofillManualFallbackPasswordEntry;
  ActionFactory* actionFactory =
      [[ActionFactory alloc] initWithScenario:menuScenario];
  UIAction* editAction = [actionFactory actionToEditWithBlock:^{
      // TODO(crbug.com/326413057): Handle tap.
  }];

  return editAction;
}

#pragma mark - Setters

- (void)setConsumer:(id<ManualFillPasswordConsumer>)consumer {
  if (consumer == _consumer) {
    return;
  }
  _consumer = consumer;
  [self postDataToConsumer];
}

- (void)setSavedPasswordsPresenter:
    (password_manager::SavedPasswordsPresenter*)savedPasswordsPresenter {
  if (_savedPasswordsPresenter == savedPasswordsPresenter) {
    return;
  }

  _savedPasswordsPresenter = savedPasswordsPresenter;
  _savedPasswordsPresenterObserver =
      std::make_unique<SavedPasswordsPresenterObserverBridge>(
          self, _savedPasswordsPresenter);
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

- (void)autofillFormWithSuggestion:(FormSuggestion*)formSuggestion {
  [self.delegate manualFillPasswordMediatorWillInjectContent:self];
  [self.contentInjector autofillFormWithSuggestion:formSuggestion];
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
  if (_activeFieldIsObfuscated !=
      (params.field_type == autofill::kObfuscatedFieldType)) {
    _activeFieldIsObfuscated =
        params.field_type == autofill::kObfuscatedFieldType;
    [self postActionsToConsumer];
  }
}

#pragma mark - CRWWebStateObserver

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  if (_webState) {
    if (_webStateObserverBridge) {
      _webState->RemoveObserver(_webStateObserverBridge.get());
    }
    _webState = nullptr;
  }
  _webStateObserverBridge.reset();
  _formActivityObserverBridge.reset();
}

#pragma mark - SavedPasswordsPresenterBridge

- (void)savedPasswordsDidChange {
  [self fetchAllPasswords];
}

#pragma mark - PasswordCounterObserver

- (void)passwordCounterChanged:(size_t)totalPasswords {
  if (_hasSavedPasswords == (totalPasswords > 0)) {
    return;
  }

  _hasSavedPasswords = totalPasswords;
  [self postActionsToConsumer];
}

@end
