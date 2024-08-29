// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/ui_bundled/bottom_sheet/password_suggestion_bottom_sheet_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/image_fetcher/core/image_fetcher_impl.h"
#import "components/image_fetcher/ios/ios_image_decoder_impl.h"
#import "components/password_manager/core/browser/features/password_features.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_manager.h"
#import "components/password_manager/core/browser/password_store/password_store_interface.h"
#import "components/password_manager/core/browser/password_ui_utils.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/ios/ios_password_manager_driver_factory.h"
#import "components/password_manager/ios/shared_password_controller.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_java_script_feature.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/autofill/model/form_input_suggestions_provider.h"
#import "ios/chrome/browser/autofill/model/form_suggestion_tab_helper.h"
#import "ios/chrome/browser/default_browser/model/default_browser_interest_signals.h"
#import "ios/chrome/browser/passwords/model/password_tab_helper.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/web_state_list/active_web_state_observation_forwarder.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/passwords/ui_bundled/bottom_sheet/password_suggestion_bottom_sheet_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/multi_avatar_image_util.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_event.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/gfx/image/image.h"
#import "url/gurl.h"

namespace {

const char kImageFetcherUmaClient[] = "PasswordBottomSheet";
const CGFloat kProfileImageSize = 80.0;

using PasswordSuggestionBottomSheetExitReason::kBadProvider;
using ReauthenticationEvent::kAttempt;
using ReauthenticationEvent::kFailure;
using ReauthenticationEvent::kMissingPasscode;
using ReauthenticationEvent::kSuccess;

int PrimaryActionStringIdFromSuggestion(FormSuggestion* suggestion) {
  return suggestion.metadata.is_single_username_form
             ? IDS_IOS_PASSWORD_BOTTOM_SHEET_CONTINUE
             : IDS_IOS_PASSWORD_BOTTOM_SHEET_USE_PASSWORD;
}

}  // namespace

@interface PasswordSuggestionBottomSheetMediator () <WebStateListObserving,
                                                     CRWWebStateObserver>

// The object that provides suggestions while filling forms.
@property(nonatomic, weak) id<FormInputSuggestionsProvider> suggestionsProvider;

// List of suggestions in the bottom sheet.
@property(nonatomic, strong) NSArray<FormSuggestion*>* suggestions;

// Default globe favicon when no favicon is available.
@property(nonatomic, readonly) FaviconAttributes* defaultGlobeIconAttributes;

@end

@implementation PasswordSuggestionBottomSheetMediator {
  // The interfaces for getting and manipulating a user's saved passwords.
  scoped_refptr<password_manager::PasswordStoreInterface> _profilePasswordStore;
  scoped_refptr<password_manager::PasswordStoreInterface> _accountPasswordStore;

  // Origin to fetch passwords for.
  GURL _URL;

  // The WebStateList observed by this mediator and the observer bridge.
  raw_ptr<WebStateList> _webStateList;

  // Bridge and forwarder for observing WebState events. The forwarder is a
  // scoped observation, so the bridge will automatically be removed from the
  // relevant observer list.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
  std::unique_ptr<ActiveWebStateObservationForwarder> _forwarder;

  // Bridge for observing WebStateList events.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  std::unique_ptr<
      base::ScopedObservation<WebStateList, WebStateListObserverBridge>>
      _webStateListObservation;

  // Vector of credentials related to the current page.
  std::vector<password_manager::CredentialUIEntry> _credentials;

  // Vector of forms that have been received via the password sharing feature
  // and the user has not been notified about them yet.
  std::vector<const password_manager::PasswordForm*> _sharedUnnotifiedForms;

  // Profile images of password senders if any of the passwords were received
  // via the password sharing feature. Empty otherwise.
  NSMutableArray<UIImage*>* _senderImages;

  // FaviconLoader is a keyed service that uses LargeIconService to retrieve
  // favicon images.
  raw_ptr<FaviconLoader> _faviconLoader;

  // Preference service from the application context.
  raw_ptr<PrefService> _prefService;

  // Module containing the reauthentication mechanism.
  __weak id<ReauthenticationProtocol> _reauthenticationModule;

  // Fetches profile pictures.
  std::unique_ptr<image_fetcher::ImageFetcher> _imageFetcher;

  // Feature engagement tracker for notifying promo events.
  raw_ptr<feature_engagement::Tracker> _engagementTracker;
}

@synthesize defaultGlobeIconAttributes = _defaultGlobeIconAttributes;

- (instancetype)
      initWithWebStateList:(WebStateList*)webStateList
             faviconLoader:(FaviconLoader*)faviconLoader
               prefService:(PrefService*)prefService
                    params:(const autofill::FormActivityParams&)params
              reauthModule:(id<ReauthenticationProtocol>)reauthModule
                       URL:(const GURL&)URL
      profilePasswordStore:
          (scoped_refptr<password_manager::PasswordStoreInterface>)
              profilePasswordStore
      accountPasswordStore:
          (scoped_refptr<password_manager::PasswordStoreInterface>)
              accountPasswordStore
    sharedURLLoaderFactory:
        (scoped_refptr<network::SharedURLLoaderFactory>)sharedURLLoaderFactory
         engagementTracker:(feature_engagement::Tracker*)engagementTracker {
  if ((self = [super init])) {
    _faviconLoader = faviconLoader;
    _prefService = prefService;
    _reauthenticationModule = reauthModule;

    _profilePasswordStore = profilePasswordStore;
    _accountPasswordStore = accountPasswordStore;
    _URL = URL;
    _imageFetcher = std::make_unique<image_fetcher::ImageFetcherImpl>(
        image_fetcher::CreateIOSImageDecoder(), sharedURLLoaderFactory);
    _senderImages = [NSMutableArray array];

    _webStateList = webStateList;
    web::WebState* activeWebState = _webStateList->GetActiveWebState();

    // Create and register the observers.
    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    _forwarder = std::make_unique<ActiveWebStateObservationForwarder>(
        _webStateList, _webStateObserver.get());
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _webStateListObservation = std::make_unique<
        base::ScopedObservation<WebStateList, WebStateListObserverBridge>>(
        _webStateListObserver.get());
    _webStateListObservation->Observe(_webStateList);

    if (activeWebState) {
      FormSuggestionTabHelper* tabHelper =
          FormSuggestionTabHelper::FromWebState(activeWebState);
      DCHECK(tabHelper);

      self.suggestionsProvider = tabHelper->GetAccessoryViewProvider();
      DCHECK(self.suggestionsProvider);

      // The 'params' argument may go out of scope before the completion block
      // is called, so we need to store variables used in the completion block
      // locally.
      autofill::FormRendererId formId = params.form_renderer_id;
      std::string frameId = params.frame_id;

      __weak __typeof(self) weakSelf = self;
      [self.suggestionsProvider
          retrieveSuggestionsForForm:params
                            webState:activeWebState
            accessoryViewUpdateBlock:^(
                NSArray<FormSuggestion*>* suggestions,
                id<FormInputSuggestionsProvider> formInputSuggestionsProvider) {
              weakSelf.suggestions = suggestions;
              [weakSelf fetchCredentialsForForm:formId
                                       webState:activeWebState
                                     webFrameId:frameId];
            }];
    }

    _engagementTracker = engagementTracker;
  }
  return self;
}

- (void)dealloc {
}

- (void)disconnect {
  _prefService = nullptr;
  _faviconLoader = nullptr;

  _webStateListObservation = nullptr;
  _webStateListObserver = nullptr;
  _forwarder = nullptr;
  _webStateObserver = nullptr;
  _webStateList = nullptr;
}

- (BOOL)hasSuggestions {
  return [self.suggestions count] > 0;
}

- (std::optional<password_manager::CredentialUIEntry>)
    getCredentialForFormSuggestion:(FormSuggestion*)formSuggestion {
  NSString* username = formSuggestion.value;
  if ([username containsString:kPasswordFormSuggestionSuffix]) {
    username = [username
        stringByReplacingOccurrencesOfString:kPasswordFormSuggestionSuffix
                                  withString:@""];
  }
  auto it = base::ranges::find_if(
      _credentials,
      [username](const password_manager::CredentialUIEntry& credential) {
        CHECK(!credential.facets.empty());
        for (auto facet : credential.facets) {
          if ([base::SysUTF16ToNSString(credential.username)
                  isEqualToString:username]) {
            return true;
          }
        }
        return false;
      });
  return it != _credentials.end()
             ? std::optional<password_manager::CredentialUIEntry>(*it)
             : std::nullopt;
}

- (void)logExitReason:(PasswordSuggestionBottomSheetExitReason)exitReason {
  base::UmaHistogramEnumeration("IOS.PasswordBottomSheet.ExitReason",
                                exitReason);
}

- (void)setCredentialsForTesting:
    (std::vector<password_manager::CredentialUIEntry>)credentials {
  _credentials = credentials;
}

#pragma mark - Accessors

- (void)setConsumer:(id<PasswordSuggestionBottomSheetConsumer>)consumer {
  _consumer = consumer;
  if ([self hasSuggestions]) {
    NSString* domain = @"";
    if (!_URL.is_empty()) {
      url::Origin origin = url::Origin::Create(_URL);
      domain =
          base::SysUTF8ToNSString(password_manager::GetShownOrigin(origin));
    }
    [consumer setSuggestions:self.suggestions andDomain:domain];
    if ([self shouldDisplaySharingNotification]) {
      [consumer setTitle:[self sharingNotificationTitle]
                subtitle:[self sharingNotificationSubtitle:domain]];
      [consumer setAvatarImage:CreateMultiAvatarImage(_senderImages,
                                                      kProfileImageSize)];
    }

    // Determine the primary action label only from the first suggestion, which
    // is sufficient as all the suggestions should have the same metadata. There
    // should be at least one suggestion at this point because the consumer is
    // set when there is at least one suggestion.
    [consumer setPrimaryActionString:l10n_util::GetNSString(
                                        PrimaryActionStringIdFromSuggestion(
                                            self.suggestions.firstObject))];
  } else {
    [consumer dismiss];
  }
}

#pragma mark - PasswordSuggestionBottomSheetDelegate

- (void)didSelectSuggestion:(FormSuggestion*)suggestion
                    atIndex:(NSInteger)index
                 completion:(ProceduralBlock)completion {
  [self logReauthEvent:kAttempt];
  [self markSharedPasswordNotificationsDisplayed];

  if (!suggestion.requiresReauth) {
    [self logReauthEvent:kSuccess];
    [self selectSuggestion:suggestion atIndex:index];
    completion();
    return;
  }
  if ([_reauthenticationModule canAttemptReauth]) {
    __weak __typeof(self) weakSelf = self;
    auto completionHandler = ^(ReauthenticationResult result) {
      [weakSelf selectSuggestion:suggestion
                         atIndex:index
          reauthenticationResult:result];
      completion();
    };

    NSString* reason = l10n_util::GetNSString(IDS_IOS_AUTOFILL_REAUTH_REASON);
    [_reauthenticationModule
        attemptReauthWithLocalizedReason:reason
                    canReusePreviousAuth:YES
                                 handler:completionHandler];
  } else {
    [self logReauthEvent:kMissingPasscode];
    [self selectSuggestion:suggestion atIndex:index];
    completion();
  }
}

- (void)dismiss {
  [self incrementDismissCount];
  [self markSharedPasswordNotificationsDisplayed];
}

- (void)disableBottomSheet {
  if (_webStateList) {
    web::WebState* activeWebState = _webStateList->GetActiveWebState();
    if (!activeWebState) {
      return;
    }

    AutofillBottomSheetTabHelper* tabHelper =
        AutofillBottomSheetTabHelper::FromWebState(activeWebState);
    if (!tabHelper) {
      return;
    }

    tabHelper->DetachPasswordListenersForAllFrames();
  }
}

- (NSString*)usernameAtRow:(NSInteger)row {
  FormSuggestion* suggestion = [self.suggestions objectAtIndex:row];

  // Removing suffix ' ••••••••' appended to the username in the suggestion.
  NSString* username = suggestion.value;
  if ([username containsString:kPasswordFormSuggestionSuffix]) {
    username = [username
        stringByReplacingOccurrencesOfString:kPasswordFormSuggestionSuffix
                                  withString:@""];
  }
  return username;
}

- (void)loadFaviconWithBlockHandler:
    (FaviconLoader::FaviconAttributesCompletionBlock)faviconLoadedBlock {
  if (!_faviconLoader) {
    // Mediator is disconnecting (bottom sheet is being closed). No need to
    // fetch for the favicon anymore.
    return;
  }
  if (!_URL.is_empty()) {
    _faviconLoader->FaviconForPageUrl(
        _URL, kDesiredMediumFaviconSizePt, kMinFaviconSizePt,
        /*fallback_to_google_server=*/NO, faviconLoadedBlock);
  } else {
    faviconLoadedBlock([self defaultGlobeIconAttributes]);
  }
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  DCHECK_EQ(_webStateList, webStateList);
  if (status.active_web_state_change()) {
    [self onWebStateChange];
  }
}

- (void)webStateListDestroyed:(WebStateList*)webStateList {
  DCHECK_EQ(webStateList, _webStateList);
  // `disconnect` cleans up all references to `_webStateList` and objects that
  // depend on it.
  [self disconnect];
  [self onWebStateChange];
}

#pragma mark - CRWWebStateObserver

- (void)webStateDestroyed:(web::WebState*)webState {
  [self onWebStateChange];
}

- (void)renderProcessGoneForWebState:(web::WebState*)webState {
  [self onWebStateChange];
}

#pragma mark - Private

- (void)onWebStateChange {
  [self.consumer dismiss];
}

// Perform suggestion selection
- (void)selectSuggestion:(FormSuggestion*)suggestion atIndex:(NSInteger)index {
  default_browser::NotifyPasswordAutofillSuggestionUsed(_engagementTracker);
  if (self.suggestionsProvider.type == SuggestionProviderTypePassword) {
    [self.suggestionsProvider didSelectSuggestion:suggestion atIndex:index];
  } else {
    [self logExitReason:kBadProvider];
  }
  [self disconnect];
}

// Perform suggestion selection based on the reauthentication result.
- (void)selectSuggestion:(FormSuggestion*)suggestion
                   atIndex:(NSInteger)index
    reauthenticationResult:(ReauthenticationResult)result {
  if (result != ReauthenticationResult::kFailure) {
    [self logReauthEvent:kSuccess];
    [self selectSuggestion:suggestion atIndex:index];
  } else {
    [self logReauthEvent:kFailure];
    [self disconnect];
  }
}

// Returns the default favicon attributes after making sure they are
// initialized.
- (FaviconAttributes*)defaultGlobeIconAttributes {
  if (!_defaultGlobeIconAttributes) {
    _defaultGlobeIconAttributes = [FaviconAttributes
        attributesWithImage:DefaultSymbolWithPointSize(
                                kGlobeAmericasSymbol,
                                kDesiredMediumFaviconSizePt)];
  }
  return _defaultGlobeIconAttributes;
}

// Increments the dismiss count preference.
- (void)incrementDismissCount {
  if (_prefService) {
    int currentDismissCount =
        _prefService->GetInteger(prefs::kIosPasswordBottomSheetDismissCount);
    if (currentDismissCount <
        AutofillBottomSheetTabHelper::kPasswordBottomSheetMaxDismissCount) {
      _prefService->SetInteger(prefs::kIosPasswordBottomSheetDismissCount,
                               currentDismissCount + 1);
    }
  }
}

// Logs reauthentication events.
- (void)logReauthEvent:(ReauthenticationEvent)event {
  base::UmaHistogramEnumeration("IOS.Reauth.Password.BottomSheet", event);
}

// Fetches all credentials for the current form.
- (void)fetchCredentialsForForm:(autofill::FormRendererId)formId
                       webState:(web::WebState*)webState
                     webFrameId:(const std::string&)frameId {
  _credentials.clear();

  if (![self hasSuggestions]) {
    return;
  }

  PasswordTabHelper* tabHelper = PasswordTabHelper::FromWebState(webState);
  if (!tabHelper) {
    return;
  }

  password_manager::PasswordManager* passwordManager =
      tabHelper->GetPasswordManager();
  CHECK(passwordManager);

  web::WebFramesManager* webFramesManager =
      AutofillBottomSheetJavaScriptFeature::GetInstance()->GetWebFramesManager(
          webState);
  web::WebFrame* frame = webFramesManager->GetFrameWithId(frameId);

  password_manager::PasswordManagerDriver* driver =
      IOSPasswordManagerDriverFactory::FromWebStateAndWebFrame(webState, frame);
  const base::span<const password_manager::PasswordForm> passwordForms =
      passwordManager->GetBestMatches(driver, formId);

  for (const password_manager::PasswordForm& form : passwordForms) {
    if (form.type ==
            password_manager::PasswordForm::Type::kReceivedViaSharing &&
        !form.sharing_notification_displayed) {
        _sharedUnnotifiedForms.push_back(&form);
        __weak __typeof__(self) weakSelf = self;
        image_fetcher::ImageFetcherParams params(NO_TRAFFIC_ANNOTATION_YET,
                                                 kImageFetcherUmaClient);
        _imageFetcher->FetchImage(
            form.sender_profile_image_url,
            base::BindOnce(^(const gfx::Image& image,
                             const image_fetcher::RequestMetadata& metadata) {
              if (!image.IsEmpty()) {
                [weakSelf onSenderImageFetched:[image.ToUIImage() copy]];
              }
            }),
            params);
    }
    _credentials.push_back(password_manager::CredentialUIEntry(form));
  }
}

// Returns whether the bottom sheet should contain a notification about shared
// passwords.
- (BOOL)shouldDisplaySharingNotification {
  return (_sharedUnnotifiedForms.size() > 0);
}

// Marks sharing notification as displayed in password store for all credentials
// on `_sharedUnnotifiedForms`.
- (void)markSharedPasswordNotificationsDisplayed {
  if (![self shouldDisplaySharingNotification]) {
    return;
  }

  for (const password_manager::PasswordForm* form : _sharedUnnotifiedForms) {
    // Make a non-const copy so we can modify it.
    password_manager::PasswordForm updatedForm = *form;
    updatedForm.sharing_notification_displayed = true;
    if (form->IsUsingAccountStore()) {
      _accountPasswordStore->UpdateLogin(std::move(updatedForm));
    } else {
      _profilePasswordStore->UpdateLogin(std::move(updatedForm));
    }
  }
  _sharedUnnotifiedForms.clear();
}

// Creates title to be displayed when the user needs to be notified about new
// shared passwords.
- (NSString*)sharingNotificationTitle {
  return base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
      IDS_IOS_PASSWORD_SHARING_NOTIFICATION_TITLE,
      _sharedUnnotifiedForms.size()));
}

// Creates subtitle to be displayed when the user needs to be notified about new
// shared passwords.
- (NSString*)sharingNotificationSubtitle:(NSString*)domain {
  if (_sharedUnnotifiedForms.size() == 1) {
    return base::SysUTF16ToNSString(l10n_util::GetStringFUTF16(
        IDS_IOS_PASSWORD_SHARING_NOTIFICATION_SINGLE_PASSWORD_SUBTITLE,
        _sharedUnnotifiedForms[0]->sender_name,
        base::SysNSStringToUTF16(domain)));
  } else {
    return base::SysUTF16ToNSString(l10n_util::GetStringFUTF16(
        IDS_IOS_PASSWORD_SHARING_NOTIFICATION_MULTIPLE_PASSWORDS_SUBTITLE,
        base::SysNSStringToUTF16(domain)));
  }
}

// Stores the fetched `image` and passes it to the consumer.
- (void)onSenderImageFetched:(UIImage*)image {
  [_senderImages addObject:image];
  [_consumer
      setAvatarImage:CreateMultiAvatarImage(_senderImages, kProfileImageSize)];
}

@end
