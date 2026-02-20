// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/credential_suggestion_bottom_sheet_mediator.h"

#import "base/feature_list.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/browser/form_suggestion_provider.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/image_fetcher/core/image_fetcher_impl.h"
#import "components/image_fetcher/ios/ios_image_decoder_impl.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_manager.h"
#import "components/password_manager/core/browser/password_store/password_store_interface.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/ios/features.h"
#import "components/password_manager/ios/ios_password_manager_driver_factory.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_java_script_feature.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/autofill/model/form_input_suggestions_provider.h"
#import "ios/chrome/browser/autofill/model/form_suggestion_tab_helper.h"
#import "ios/chrome/browser/default_browser/model/default_browser_interest_signals.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/credential_suggestion_bottom_sheet_mediator_base+Subclassing.h"
#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/password_suggestion_bottom_sheet_exit_reason.h"
#import "ios/chrome/browser/passwords/bottom_sheet/ui/credential_suggestion_bottom_sheet_consumer.h"
#import "ios/chrome/browser/passwords/model/password_tab_helper.h"
#import "ios/chrome/browser/passwords/password_suggestion/ui/password_suggestion_utils.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_sharing/multi_avatar_image_util.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_event.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/gfx/image/image.h"
#import "url/gurl.h"

namespace {

constexpr char kImageFetcherUmaClient[] = "PasswordBottomSheet";
constexpr CGFloat kProfileImageSize = 80.0;

using PasswordSuggestionBottomSheetExitReason::kBadProvider;
using ReauthenticationEvent::kAttempt;
using ReauthenticationEvent::kFailure;
using ReauthenticationEvent::kMissingPasscode;
using ReauthenticationEvent::kSuccess;

int PrimaryActionStringIdFromSuggestion(FormSuggestion* suggestion) {
  return suggestion.metadata.is_single_username_form
             ? IDS_IOS_CREDENTIAL_BOTTOM_SHEET_CONTINUE
             : IDS_IOS_CREDENTIAL_BOTTOM_SHEET_USE_PASSWORD;
}

// Makes a query to retrieve suggestions from a FormSuggestionProvider from the
// provided `params`. Only ask for suggestions with passwords.
FormSuggestionProviderQuery* MakeQueryFromParameters(
    const autofill::FormActivityParams& params) {
  return [[FormSuggestionProviderQuery alloc]
      initWithFormName:base::SysUTF8ToNSString(params.form_name)
        formRendererID:params.form_renderer_id
       fieldIdentifier:base::SysUTF8ToNSString(params.field_identifier)
       fieldRendererID:params.field_renderer_id
             fieldType:base::SysUTF8ToNSString(params.field_type)
                  type:base::SysUTF8ToNSString(params.type)
            typedValue:base::SysUTF8ToNSString(params.value)
               frameID:base::SysUTF8ToNSString(params.frame_id)
          onlyPassword:YES];
}

// Makes a copy of suggestions with `params` and `provider` set in the copies.
NSArray<FormSuggestion*>* SetParamsAndProviderInSuggestions(
    NSArray<FormSuggestion*>* suggestions,
    const autofill::FormActivityParams& params,
    id<FormSuggestionProvider> provider) {
  NSMutableArray<FormSuggestion*>* suggestions_copy =
      [NSMutableArray<FormSuggestion*> arrayWithCapacity:[suggestions count]];
  for (FormSuggestion* suggestion in suggestions) {
    [suggestions_copy addObject:[FormSuggestion copy:suggestion
                                        andSetParams:params
                                            provider:provider]];
  }
  return suggestions_copy;
}

}  // namespace

// TODO(crbug.com/372426818): Move this is to its own specific file/module.
@interface BottomSheetFormSuggestionProviderWrapper : NSObject

- (instancetype)
    initWithFormSuggestionProvider:(id<FormSuggestionProvider>)provider
                            params:(autofill::FormActivityParams)params;

- (void)retrieveSuggestionsForForm:(autofill::FormActivityParams)params
                          webState:(web::WebState*)webState
                        completion:
                            (void (^)(NSArray<FormSuggestion*>* suggestions))
                                completion;

- (void)didSelectSuggestion:(FormSuggestion*)suggestion
                    atIndex:(NSInteger)index
                   webState:(web::WebState*)webState;

@end

@implementation BottomSheetFormSuggestionProviderWrapper {
  // Suggestions provider for the bottom sheet.
  __weak id<FormSuggestionProvider> _providerWrapper;

  // Form activity parameters giving the context around the sheet trigger.
  autofill::FormActivityParams _params;
}

- (instancetype)
    initWithFormSuggestionProvider:(id<FormSuggestionProvider>)provider
                            params:(autofill::FormActivityParams)params {
  if ((self = [super init])) {
    _providerWrapper = provider;
    _params = params;
  }
  return self;
}

- (void)retrieveSuggestionsForForm:(autofill::FormActivityParams)params
                          webState:(web::WebState*)webState
                        completion:
                            (void (^)(NSArray<FormSuggestion*>* suggestions))
                                completion {
  FormSuggestionProviderQuery* formQuery = MakeQueryFromParameters(params);
  [_providerWrapper
      retrieveSuggestionsForForm:formQuery
                        webState:webState
               completionHandler:^(NSArray<FormSuggestion*>* suggestions,
                                   id<FormSuggestionProvider> delegate) {
                 bool stateless = base::FeatureList::IsEnabled(
                     password_manager::features::kIOSStatelessFillDataFlow);
                 NSArray<FormSuggestion*>* wrappedSuggestions =
                     stateless ? SetParamsAndProviderInSuggestions(
                                     suggestions, params, delegate)
                               : suggestions;
                 completion(wrappedSuggestions);
               }];
}

- (void)didSelectSuggestion:(FormSuggestion*)suggestion
                    atIndex:(NSInteger)index
                   webState:(web::WebState*)webState {
  __weak UIView* weakView = webState->GetView();
  [_providerWrapper
      didSelectSuggestion:suggestion
                  atIndex:index
                     form:base::SysUTF8ToNSString(_params.form_name)
           formRendererID:_params.form_renderer_id
          fieldIdentifier:base::SysUTF8ToNSString(_params.field_identifier)
          fieldRendererID:_params.field_renderer_id
                  frameID:base::SysUTF8ToNSString(_params.frame_id)
        completionHandler:^{
          // Close the keyboard after filling the suggestion. This is the same
          // approach as used when filling with the FormInputSuggestionProvider
          // in V1. Not doing this will result he re-popping the keyboard after
          // filling is done which is a bad UX.
          [weakView endEditing:YES];
        }];
}

- (SuggestionProviderType)type {
  return _providerWrapper.type;
}

@end

@interface CredentialSuggestionBottomSheetMediator ()

// Default globe favicon when no favicon is available.
@property(nonatomic, readonly) FaviconAttributes* defaultGlobeIconAttributes;

@end

@implementation CredentialSuggestionBottomSheetMediator {
  // The interfaces for getting and manipulating a user's saved passwords.
  scoped_refptr<password_manager::PasswordStoreInterface> _profilePasswordStore;
  scoped_refptr<password_manager::PasswordStoreInterface> _accountPasswordStore;

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

  // Parameters that give the details on the field that triggered the bottom
  // sheet. The sheet is tied to these fields during its entire lifetime.
  autofill::FormActivityParams _params;

  // Provider wrapper that gives suggestions and handles suggestion selection.
  // The underlying concrete provider will be determined during initialization
  // depending on the version of the sheet.
  BottomSheetFormSuggestionProviderWrapper* _suggestionsProviderWrapper;
}

@synthesize defaultGlobeIconAttributes = _defaultGlobeIconAttributes;

- (instancetype)
      initWithWebStateList:(WebStateList*)webStateList
             faviconLoader:(FaviconLoader*)faviconLoader
               prefService:(PrefService*)prefService
                    params:(const autofill::FormActivityParams&)params
              reauthModule:(id<ReauthenticationProtocol>)reauthModule
      profilePasswordStore:
          (scoped_refptr<password_manager::PasswordStoreInterface>)
              profilePasswordStore
      accountPasswordStore:
          (scoped_refptr<password_manager::PasswordStoreInterface>)
              accountPasswordStore
    sharedURLLoaderFactory:
        (scoped_refptr<network::SharedURLLoaderFactory>)sharedURLLoaderFactory
         engagementTracker:(feature_engagement::Tracker*)engagementTracker {
  self = [super initWithWebStateList:webStateList];
  if (self) {
    _faviconLoader = faviconLoader;
    _prefService = prefService;
    _reauthenticationModule = reauthModule;

    _profilePasswordStore = profilePasswordStore;
    _accountPasswordStore = accountPasswordStore;
    _imageFetcher = std::make_unique<image_fetcher::ImageFetcherImpl>(
        image_fetcher::CreateIOSImageDecoder(), sharedURLLoaderFactory);
    _senderImages = [NSMutableArray array];
    _params = params;

    web::WebState* activeWebState = webStateList->GetActiveWebState();
    if (activeWebState) {
      PasswordTabHelper* passwordTabHelper =
          PasswordTabHelper::FromWebState(activeWebState);
      CHECK(passwordTabHelper);
      id<FormSuggestionProvider> provider =
          passwordTabHelper->GetSuggestionProvider();
      CHECK(provider);
      _suggestionsProviderWrapper =
          [[BottomSheetFormSuggestionProviderWrapper alloc]
              initWithFormSuggestionProvider:provider
                                      params:_params];

      // The 'params' argument may go out of scope before the completion block
      // is called, so we need to store variables used in the completion block
      // locally.
      autofill::FormRendererId formId = params.form_renderer_id;
      __weak __typeof(self) weakSelf = self;
      [_suggestionsProviderWrapper
          retrieveSuggestionsForForm:params
                            webState:activeWebState
                          completion:^(NSArray<FormSuggestion*>* suggestions) {
                            weakSelf.suggestions = suggestions;
                            [weakSelf fetchCredentialsForForm:formId
                                                     webState:activeWebState];
                          }];
    }

    _engagementTracker = engagementTracker;
  }
  return self;
}

- (std::optional<password_manager::CredentialUIEntry>)
    getCredentialForFormSuggestion:(FormSuggestion*)formSuggestion {
  NSString* username = formSuggestion.value;
  auto it = std::ranges::find_if(
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

- (void)setCredentialsForTesting:
    (std::vector<password_manager::CredentialUIEntry>)credentials {
  _credentials = credentials;
}

#pragma mark - CredentialSuggestionBottomSheetMediatorBase

- (void)setConsumer:(id<CredentialSuggestionBottomSheetConsumer>)consumer {
  [super setConsumer:consumer];

  // The bottom sheet isn't presented when there are no suggestions to show, so
  // there's no need to update the consumer.
  if (![self hasSuggestions]) {
    return;
  }

  if ([self shouldDisplaySharingNotification]) {
    [self.consumer setTitle:[self sharingNotificationTitle]
                   subtitle:[self sharingNotificationSubtitle:self.domain]];
    [self.consumer setAvatarImage:CreateMultiAvatarImage(_senderImages,
                                                         kProfileImageSize)];
  }

  // Determine the primary action label only from the first suggestion, which
  // is sufficient as all the suggestions should have the same metadata.
  [self.consumer setPrimaryActionString:l10n_util::GetNSString(
                                            PrimaryActionStringIdFromSuggestion(
                                                self.suggestions.firstObject))];
}

- (void)disconnect {
  [super disconnect];

  _prefService = nullptr;
  _faviconLoader = nullptr;

  _suggestionsProviderWrapper = nil;
}

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

- (void)logExitReason:(PasswordSuggestionBottomSheetExitReason)exitReason {
  base::UmaHistogramEnumeration("IOS.PasswordBottomSheet.ExitReason",
                                exitReason);
}

- (void)onDismissWithoutAnyCredentialAction {
  [self incrementDismissCount];
  [self markSharedPasswordNotificationsDisplayed];
}

#pragma mark - CredentialSuggestionBottomSheetDelegate

- (void)disableBottomSheet {
  if (self.webStateList) {
    web::WebState* activeWebState = self.webStateList->GetActiveWebState();
    if (!activeWebState) {
      return;
    }

    AutofillBottomSheetTabHelper* tabHelper =
        AutofillBottomSheetTabHelper::FromWebState(activeWebState);
    if (!tabHelper) {
      return;
    }

    tabHelper->DetachPasswordListenersForAllFrames(/*refocus=*/true);
  }
}

- (void)loadFaviconWithBlockHandler:
    (FaviconLoader::FaviconAttributesCompletionBlock)faviconLoadedBlock {
  if (!_faviconLoader) {
    // Mediator is disconnecting (bottom sheet is being closed). No need to
    // fetch for the favicon anymore.
    return;
  }
  if (!self.URL.is_empty()) {
    _faviconLoader->FaviconForPageUrl(
        self.URL, kDesiredMediumFaviconSizePt, kMinFaviconSizePt,
        /*fallback_to_google_server=*/NO, faviconLoadedBlock);
  } else {
    faviconLoadedBlock([self defaultGlobeIconAttributes], /*cached*/ true);
  }
}

#pragma mark - Private

// Perform suggestion selection
- (void)selectSuggestion:(FormSuggestion*)suggestion atIndex:(NSInteger)index {
  default_browser::NotifyPasswordAutofillSuggestionUsed(_engagementTracker);

  if (!self.webStateList) {
    return;
  }

  web::WebState* activeWebState = self.webStateList->GetActiveWebState();
  if (!activeWebState) {
    return;
  }

  if ([_suggestionsProviderWrapper type] == SuggestionProviderTypePassword) {
    [_suggestionsProviderWrapper didSelectSuggestion:suggestion
                                             atIndex:index
                                            webState:activeWebState];
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
    _defaultGlobeIconAttributes = GetDefaultGlobeFaviconAttributes();
  }
  return _defaultGlobeIconAttributes;
}

// Increments the dismiss count preference.
- (void)incrementDismissCount {
  if (_prefService) {
    int currentDismissCount =
        _prefService->GetInteger(prefs::kIosPasswordBottomSheetDismissCount);
    if (currentDismissCount <
        AutofillBottomSheetTabHelper::kCredentialBottomSheetMaxDismissCount) {
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
                       webState:(web::WebState*)webState {
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
  web::WebFrame* frame = webFramesManager->GetFrameWithId(_params.frame_id);

  if (!frame) {
    return;
  }

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
  [self.consumer
      setAvatarImage:CreateMultiAvatarImage(_senderImages, kProfileImageSize)];
}

// Returns the AutofillBottomSheetTabHelper for the active webstate or nil if
// it can't be retrieved.
- (AutofillBottomSheetTabHelper*)tabHelper {
  if (!self.webStateList) {
    return nil;
  }

  web::WebState* activeWebState = self.webStateList->GetActiveWebState();
  if (!activeWebState) {
    return nil;
  }

  return AutofillBottomSheetTabHelper::FromWebState(activeWebState);
}

// Refocuses the login fields that was blurred to show this bottom sheet, if
// deemded needed.
- (void)refocus {
  if (AutofillBottomSheetTabHelper* tabHelper = [self tabHelper]) {
    tabHelper->RefocusElementIfNeeded(_params.frame_id);
  }
}

@end
