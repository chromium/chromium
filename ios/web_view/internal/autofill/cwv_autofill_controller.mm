// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <algorithm>
#import <memory>
#import <string>
#import <vector>

#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/ios/block_types.h"
#import "base/not_fatal_until.h"
#import "base/notimplemented.h"
#import "base/scoped_multi_source_observation.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "components/autofill/core/browser/autofill_field.h"
#import "components/autofill/core/browser/form_structure.h"
#import "components/autofill/core/browser/foundations/autofill_manager.h"
#import "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#import "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#import "components/autofill/core/browser/payments/credit_card_access_manager.h"
#import "components/autofill/core/browser/payments/legal_message_line.h"
#import "components/autofill/core/browser/payments/payments_autofill_client.h"
#import "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#import "components/autofill/core/browser/suggestions/suggestion_type.h"
#import "components/autofill/core/browser/ui/payments/autofill_progress_ui_type.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/core/common/autofill_util.h"
#import "components/autofill/ios/browser/autofill_agent.h"
#import "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/browser/autofill_driver_ios_factory.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "components/autofill/ios/browser/autofill_manager_observer_bridge.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/browser/suggestion_controller_java_script_feature.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#import "components/password_manager/ios/shared_password_controller.h"
#import "components/prefs/pref_service.h"
#import "components/sync/service/sync_service.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/js_messaging/web_frames_manager_observer_bridge.h"
#import "ios/web/public/web_state.h"
#import "ios/web_view/internal/app/application_context.h"
#import "ios/web_view/internal/autofill/cwv_autofill_controller+testing.h"
#import "ios/web_view/internal/autofill/cwv_autofill_controller_internal.h"
#import "ios/web_view/internal/autofill/cwv_autofill_form_internal.h"
#import "ios/web_view/internal/autofill/cwv_autofill_prefs.h"
#import "ios/web_view/internal/autofill/cwv_autofill_profile_internal.h"
#import "ios/web_view/internal/autofill/cwv_autofill_suggestion_internal.h"
#import "ios/web_view/internal/autofill/cwv_card_unmask_challenge_option_internal.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_internal.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_otp_verifier_internal.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_saver_internal.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_verifier_internal.h"
#import "ios/web_view/internal/autofill/cwv_vcn_enrollment_manager_internal.h"
#import "ios/web_view/internal/autofill/web_view_autocomplete_history_manager_factory.h"
#import "ios/web_view/internal/autofill/web_view_autofill_client_ios.h"
#import "ios/web_view/internal/autofill/web_view_autofill_log_router_factory.h"
#import "ios/web_view/internal/autofill/web_view_personal_data_manager_factory.h"
#import "ios/web_view/internal/autofill/web_view_strike_database_factory.h"
#import "ios/web_view/internal/cwv_web_view_internal.h"
#import "ios/web_view/internal/passwords/cwv_password_internal.h"
#import "ios/web_view/internal/signin/web_view_identity_manager_factory.h"
#import "ios/web_view/internal/web_view_browser_state.h"
#import "ios/web_view/public/cwv_autofill_controller_delegate.h"
#import "ios/web_view/public/cwv_web_view.h"
#import "net/base/apple/url_conversions.h"

using autofill::FieldRendererId;
using autofill::FormData;
using autofill::FormRendererId;
using UserDecision = autofill::AutofillClient::AddressPromptUserDecision;

NSErrorDomain const CWVAutofillErrorDomain =
    @"org.chromium.chromewebview.AutofillErrorDomain";

@interface CWVAutofillController () <AutofillManagerObserver,
                                     CRWWebFramesManagerObserver>

- (void)onAfterFormSubmitted:(autofill::AutofillManager&)manager
                    formData:(const autofill::FormData&)form;

- (void)
    onAutofillManagerStateChanged:(autofill::AutofillManager&)manager
                             from:(autofill::AutofillManager::LifecycleState)
                                      oldState
                               to:(autofill::AutofillManager::LifecycleState)
                                      newState;

@end

namespace {
// Helper function to map C++ enum to Objective-C enum
CWVAutofillProgressDialogType ToCWVAutofillProgressDialogType(
    autofill::AutofillProgressUiType type) {
  switch (type) {
    case autofill::AutofillProgressUiType::kUnspecified:
      return CWVAutofillProgressDialogTypeUnspecified;
    case autofill::AutofillProgressUiType::kVirtualCardUnmaskProgressUi:
      return CWVAutofillProgressDialogTypeVirtualCardUnmask;
    case autofill::AutofillProgressUiType::kServerCardUnmaskProgressUi:
      return CWVAutofillProgressDialogTypeServerCardUnmask;
    case autofill::AutofillProgressUiType::kServerIbanUnmaskProgressUi:
      return CWVAutofillProgressDialogTypeIbanUnmask;
    case autofill::AutofillProgressUiType::k3dsFetchVcnProgressUi:
      return CWVAutofillProgressDialogType3DSFetchVCN;
    case autofill::AutofillProgressUiType::
        kCardInfoRetrievalEnrolledUnmaskProgressUi:
      return CWVAutofillProgressDialogTypeCardInfoRetrievalEnrolledUnmask;
    case autofill::AutofillProgressUiType::kBnplFetchVcnProgressUi:
      return CWVAutofillProgressDialogTypeBNPLFetchVCN;
    case autofill::AutofillProgressUiType::kBnplAmountExtractionProgressUi:
      return CWVAutofillProgressDialogTypeBNPLAmountExtraction;
  }
  return CWVAutofillProgressDialogTypeUnspecified;
}
}  // namespace

@implementation CWVAutofillController {
  // Bridge to observe the |webState|.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;

  // Bridge to observe the `WebFramesManager`.
  std::unique_ptr<web::WebFramesManagerObserverBridge>
      _webFramesManagerObserverBridge;

  // Bridge to observe `AutofillManager` events.
  std::unique_ptr<autofill::AutofillManagerObserverBridge>
      _autofillManagerObserverBridge;

  // Tracks observations for all AutofillManagers (one per frame).
  std::unique_ptr<
      base::ScopedMultiSourceObservation<autofill::AutofillManager,
                                         autofill::AutofillManager::Observer>>
      _autofillManagerObservations;

  // Autofill agent associated with |webState|.
  AutofillAgent* _autofillAgent;

  // Autofill client associated with |webState|.
  std::unique_ptr<autofill::WebViewAutofillClientIOS> _autofillClient;

  // The |webState| which this autofill controller should observe.
  web::WebState* _webState;

  // Handles password autofilling related logic.
  std::unique_ptr<password_manager::PasswordManager> _passwordManager;
  std::unique_ptr<ios_web_view::WebViewPasswordManagerClient>
      _passwordManagerClient;
  SharedPasswordController* _passwordController;

  // The current credit card saver. Can be nil if no save attempt is pending.
  // Held weak because |_delegate| is responsible for maintaining its lifetime.
  __weak CWVCreditCardSaver* _saver;

  // The current credit card verifier. Can be nil if no verification is pending.
  // Held weak because |_delegate| is responsible for maintaining its lifetime.
  __weak CWVCreditCardVerifier* _verifier;

  // The current VCNEnrollmentManager. Can be nil if no enrollment is pending.
  // Held weak because |_delegate| is responsible for maintaining its lifetime.
  __weak CWVVCNEnrollmentManager* _enrollmentManager;

  // The current CWVCreditCardOTPVerifier. Can be nil if no verification is
  // pending. Held weak because |_delegate| is responsible for maintaining its
  // lifetime.
  __weak CWVCreditCardOTPVerifier* _OTPVerifier;

  std::unique_ptr<autofill::FormActivityObserverBridge>
      _formActivityObserverBridge;

  NSString* _lastFormActivityFormName;
  NSString* _lastFormActivityFieldIdentifier;
  NSString* _lastFormActivityFrameID;
  std::string _lastFormActivityWebFrameID;
  NSString* _lastFormActivityTypedValue;
  NSString* _lastFormActivityType;
  FormRendererId _lastFormActivityFormRendererID;
  FieldRendererId _lastFormActivityFieldRendererID;

  // YES to support xframe submission to correctly handle form submission when
  // autofill across iframes is enabled.
  BOOL _supportXframeSubmission;
}

@synthesize delegate = _delegate;

- (instancetype)initWithWebState:(web::WebState*)webState
                   autofillAgent:(AutofillAgent*)autofillAgent
                 passwordManager:
                     (std::unique_ptr<password_manager::PasswordManager>)
                         passwordManager
           passwordManagerClient:
               (std::unique_ptr<ios_web_view::WebViewPasswordManagerClient>)
                   passwordManagerClient
              passwordController:(SharedPasswordController*)passwordController {
  self = [self initWithWebState:webState
          autofillClientForTest:nullptr
                  autofillAgent:autofillAgent
                passwordManager:std::move(passwordManager)
          passwordManagerClient:std::move(passwordManagerClient)
             passwordController:passwordController];
  return self;
}

- (instancetype)
         initWithWebState:(web::WebState*)webState
    autofillClientForTest:(std::unique_ptr<autofill::WebViewAutofillClientIOS>)
                              autofillClientForTest
            autofillAgent:(AutofillAgent*)autofillAgent
          passwordManager:(std::unique_ptr<password_manager::PasswordManager>)
                              passwordManager
    passwordManagerClient:
        (std::unique_ptr<ios_web_view::WebViewPasswordManagerClient>)
            passwordManagerClient
       passwordController:(SharedPasswordController*)passwordController {
  self = [super init];
  if (self) {
    DCHECK(webState);
    _webState = webState;
    _supportXframeSubmission = base::FeatureList::IsEnabled(
        autofill::features::kAutofillAcrossIframesIos);

    _autofillAgent = autofillAgent;

    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _webState->AddObserver(_webStateObserverBridge.get());

    _autofillManagerObserverBridge =
        std::make_unique<autofill::AutofillManagerObserverBridge>(self);

    if (_supportXframeSubmission) {
      _autofillManagerObservations =
          std::make_unique<base::ScopedMultiSourceObservation<
              autofill::AutofillManager, autofill::AutofillManager::Observer>>(
              _autofillManagerObserverBridge.get());

      _webFramesManagerObserverBridge =
          std::make_unique<web::WebFramesManagerObserverBridge>(self);
      web::WebFramesManager* framesManager =
          autofill::AutofillJavaScriptFeature::GetInstance()
              ->GetWebFramesManager(_webState);
      framesManager->AddObserver(_webFramesManagerObserverBridge.get());

      // Observe existing frames.
      for (web::WebFrame* frame : framesManager->GetAllWebFrames()) {
        [self webFramesManager:framesManager frameBecameAvailable:frame];
      }
    }

    _formActivityObserverBridge =
        std::make_unique<autofill::FormActivityObserverBridge>(webState, self);

    _autofillClient =
        autofillClientForTest
            ? std::move(autofillClientForTest)
            : autofill::WebViewAutofillClientIOS::Create(_webState, self);

    _passwordManagerClient = std::move(passwordManagerClient);
    _passwordManagerClient->set_bridge(self);
    _passwordManager = std::move(passwordManager);
    _passwordController = passwordController;
    _passwordController.delegate = self;
  }
  return self;
}

- (void)dealloc {
  if (_webState) {
    if (_supportXframeSubmission) {
      autofill::AutofillJavaScriptFeature::GetInstance()
          ->GetWebFramesManager(_webState)
          ->RemoveObserver(_webFramesManagerObserverBridge.get());
      _autofillManagerObservations->RemoveAllObservations();
    }
    _formActivityObserverBridge.reset();
    _webState->RemoveObserver(_webStateObserverBridge.get());
    _webStateObserverBridge.reset();
    _webState = nullptr;
  }
}

#pragma mark - Public Methods

- (autofill::WebViewAutofillClientIOS*)autofillClient {
  return _autofillClient.get();
}

- (void)clearFormWithName:(NSString*)formName
          fieldIdentifier:(NSString*)fieldIdentifier
                  frameID:(NSString*)frameID
        completionHandler:(nullable void (^)(void))completionHandler {
  autofill::AutofillJavaScriptFeature* feature =
      autofill::AutofillJavaScriptFeature::GetInstance();
  web::WebFrame* frame =
      feature->GetWebFramesManager(_webState)->GetFrameWithId(
          base::SysNSStringToUTF8(frameID));
  feature->ClearAutofilledFieldsForForm(frame, _lastFormActivityFormRendererID,
                                        _lastFormActivityFieldRendererID,
                                        base::BindOnce(^(NSString*) {
                                          if (completionHandler) {
                                            completionHandler();
                                          }
                                        }));
}

- (void)fetchSuggestionsForFormWithName:(NSString*)formName
                        fieldIdentifier:(NSString*)fieldIdentifier
                              fieldType:(NSString*)fieldType
                                frameID:(NSString*)frameID
                      completionHandler:
                          (void (^)(NSArray<CWVAutofillSuggestion*>* _Nonnull))
                              completionHandler {
  NSMutableArray<CWVAutofillSuggestion*>* allSuggestions =
      [NSMutableArray array];
  __block NSInteger pendingFetches = 0;
  void (^resultHandler)(NSArray<CWVAutofillSuggestion*>*) =
      ^(NSArray<CWVAutofillSuggestion*>* suggestions) {
        [allSuggestions addObjectsFromArray:suggestions];
        if (pendingFetches == 0) {
          completionHandler(allSuggestions);
        }
      };

  _lastFormActivityFormName = formName;
  _lastFormActivityFrameID = frameID;
  _lastFormActivityFieldIdentifier = fieldIdentifier;

  // Construct query.
  FormSuggestionProviderQuery* formQuery = [[FormSuggestionProviderQuery alloc]
      initWithFormName:formName
        formRendererID:_lastFormActivityFormRendererID
       fieldIdentifier:fieldIdentifier
       fieldRendererID:_lastFormActivityFieldRendererID
             fieldType:fieldType
                  type:_lastFormActivityType
            typedValue:_lastFormActivityTypedValue
               frameID:frameID
          onlyPassword:NO];
  // It is necessary to call |checkIfSuggestionsAvailableForForm| before
  // |retrieveSuggestionsForForm| because the former actually queries the db,
  // while the latter merely returns them.

  // Check both autofill and password suggestions.
  NSArray<id<FormSuggestionProvider>>* providers =
      @[ _passwordController, _autofillAgent ];
  pendingFetches = providers.count;
  for (id<FormSuggestionProvider> suggestionProvider in providers) {
    __weak CWVAutofillController* weakSelf = self;
    id availableHandler = ^(BOOL suggestionsAvailable) {
      pendingFetches--;
      CWVAutofillController* strongSelf = weakSelf;
      if (!strongSelf) {
        resultHandler(@[]);
        return;
      }
      BOOL isPasswordSuggestion =
          suggestionProvider == strongSelf->_passwordController;
      id retrieveHandler =
          ^(NSArray* suggestions, id<FormSuggestionProvider> delegate) {
            NSMutableArray* autofillSuggestions = [NSMutableArray array];
            for (FormSuggestion* formSuggestion in suggestions) {
              CWVAutofillSuggestion* autofillSuggestion =
                  [[CWVAutofillSuggestion alloc]
                      initWithFormSuggestion:formSuggestion
                                    formName:formName
                             fieldIdentifier:fieldIdentifier
                                     frameID:frameID
                        isPasswordSuggestion:isPasswordSuggestion];
              [autofillSuggestions addObject:autofillSuggestion];
            }
            resultHandler([autofillSuggestions copy]);
          };
      [suggestionProvider retrieveSuggestionsForForm:formQuery
                                            webState:strongSelf->_webState
                                   completionHandler:retrieveHandler];
    };

    [suggestionProvider checkIfSuggestionsAvailableForForm:formQuery
                                            hasUserGesture:YES
                                                  webState:_webState
                                         completionHandler:availableHandler];
  }
}

- (void)acceptSuggestion:(CWVAutofillSuggestion*)suggestion
                 atIndex:(NSInteger)index
       completionHandler:(nullable void (^)(void))completionHandler {
  if (suggestion.isPasswordSuggestion) {
    [_passwordController didSelectSuggestion:suggestion.formSuggestion
                                     atIndex:index
                                        form:suggestion.formName
                              formRendererID:_lastFormActivityFormRendererID
                             fieldIdentifier:suggestion.fieldIdentifier
                             fieldRendererID:_lastFormActivityFieldRendererID
                                     frameID:suggestion.frameID
                           completionHandler:^{
                             if (completionHandler) {
                               completionHandler();
                             }
                           }];
  } else {
    [_autofillAgent didSelectSuggestion:suggestion.formSuggestion
                                atIndex:index
                                   form:suggestion.formName
                         formRendererID:_lastFormActivityFormRendererID
                        fieldIdentifier:suggestion.fieldIdentifier
                        fieldRendererID:_lastFormActivityFieldRendererID
                                frameID:suggestion.frameID
                      completionHandler:^{
                        if (completionHandler) {
                          completionHandler();
                        }
                      }];
  }
}

- (void)acceptCreditCardAsSuggestion:(CWVCreditCard*)card
                             atIndex:(NSInteger)index
                   completionHandler:
                       (nullable void (^)(void))completionHandler {
  FormSuggestion* suggestion = [FormSuggestion
      suggestionWithValue:nil
       displayDescription:nil
                     icon:nil
                     type:autofill::SuggestionType::kCreditCardEntry
                  payload:autofill::Suggestion::Guid(card.internalCard->guid())
           requiresReauth:NO];

  [_autofillAgent didSelectSuggestion:suggestion
                              atIndex:index
                                 form:_lastFormActivityFormName
                       formRendererID:_lastFormActivityFormRendererID
                      fieldIdentifier:_lastFormActivityFieldIdentifier
                      fieldRendererID:_lastFormActivityFieldRendererID
                              frameID:_lastFormActivityFrameID
                    completionHandler:^{
                      if (completionHandler) {
                        completionHandler();
                      }
                    }];
}

- (void)fetchFullCardDetailsForCard:(CWVCreditCard*)card
                  completionHandler:(CWVFetchFullCardDetailsCompletionHandler)
                                        completionHandler {
  web::WebFrame* frame = autofill::AutofillJavaScriptFeature::GetInstance()
                             ->GetWebFramesManager(_webState)
                             ->GetFrameWithId(_lastFormActivityWebFrameID);

  if (!frame) {
    if (completionHandler) {
      NSError* error = [NSError errorWithDomain:CWVAutofillErrorDomain
                                           code:CWVAutofillErrorNoWebFrame
                                       userInfo:nil];
      completionHandler(/*fullCard=*/nil, error);
    }
    return;
  }

  autofill::AutofillDriverIOS* driver =
      autofill::AutofillDriverIOS::FromWebStateAndWebFrame(_webState, frame);
  if (!driver) {
    if (completionHandler) {
      NSError* error = [NSError errorWithDomain:CWVAutofillErrorDomain
                                           code:CWVAutofillErrorNoAutofillDriver
                                       userInfo:nil];
      completionHandler(/*fullCard=*/nil, error);
    }
    return;
  }

  autofill::BrowserAutofillManager& manager = driver->GetAutofillManager();
  autofill::CreditCardAccessManager* accessManager =
      manager.GetCreditCardAccessManager();

  accessManager->FetchCreditCard(
      card.internalCard, base::BindOnce(
                             ^(CWVFetchFullCardDetailsCompletionHandler handler,
                               const autofill::CreditCard& fetchedCard) {
                               CWVCreditCard* fullCard = [[CWVCreditCard alloc]
                                   initWithCreditCard:fetchedCard];
                               if (handler) {
                                 handler(fullCard, /*error=*/nil);
                               }
                             },
                             completionHandler));
}

- (void)focusPreviousField {
  web::WebFramesManager* framesManager =
      autofill::AutofillJavaScriptFeature::GetInstance()->GetWebFramesManager(
          _webState);
  web::WebFrame* frame =
      framesManager->GetFrameWithId(_lastFormActivityWebFrameID);

  if (!frame) {
    return;
  }

  autofill::SuggestionControllerJavaScriptFeature::GetInstance()
      ->SelectPreviousElementInFrame(frame);
}

- (void)focusNextField {
  web::WebFramesManager* framesManager =
      autofill::AutofillJavaScriptFeature::GetInstance()->GetWebFramesManager(
          _webState);
  web::WebFrame* frame =
      framesManager->GetFrameWithId(_lastFormActivityWebFrameID);

  if (!frame) {
    return;
  }

  autofill::SuggestionControllerJavaScriptFeature::GetInstance()
      ->SelectNextElementInFrame(frame);
}

- (void)checkIfPreviousAndNextFieldsAreAvailableForFocusWithCompletionHandler:
    (void (^)(BOOL previous, BOOL next))completionHandler {
  web::WebFramesManager* framesManager =
      autofill::AutofillJavaScriptFeature::GetInstance()->GetWebFramesManager(
          _webState);
  web::WebFrame* frame =
      framesManager->GetFrameWithId(_lastFormActivityWebFrameID);

  autofill::SuggestionControllerJavaScriptFeature::GetInstance()
      ->FetchPreviousAndNextElementsPresenceInFrame(
          frame, base::BindOnce(completionHandler));
}

#pragma mark - CWVAutofillClientIOSBridge

- (void)showAutofillPopup:(const std::vector<autofill::Suggestion>&)suggestions
       suggestionDelegate:
           (const base::WeakPtr<autofill::AutofillSuggestionDelegate>&)
               delegate {
  // We only want Autofill suggestions.
  std::vector<autofill::Suggestion> filtered_suggestions;

  web::WebState* currentWebState = _webState;

  std::ranges::copy_if(
      suggestions, std::back_inserter(filtered_suggestions),
      [currentWebState](const autofill::Suggestion& suggestion) {
        if (!currentWebState) {
          return false;
        }
        PrefService* prefService =
            ios_web_view::WebViewBrowserState::FromBrowserState(
                currentWebState->GetBrowserState())
                ->GetPrefs();
        if (prefService->GetBoolean(
                ios_web_view::kCWVAutofillVCNUsageEnabled)) {
          return suggestion.type == autofill::SuggestionType::kAddressEntry ||
                 suggestion.type ==
                     autofill::SuggestionType::kCreditCardEntry ||
                 suggestion.type ==
                     autofill::SuggestionType::kVirtualCreditCardEntry;
        }
        return suggestion.type == autofill::SuggestionType::kAddressEntry ||
               suggestion.type == autofill::SuggestionType::kCreditCardEntry;
      });
  [_autofillAgent showAutofillPopup:filtered_suggestions
                 suggestionDelegate:delegate];
}

- (void)hideAutofillPopup {
  [_autofillAgent hideAutofillPopup];
}

- (bool)isLastQueriedField:(autofill::FieldGlobalId)fieldId {
  return [_autofillAgent isLastQueriedField:fieldId];
}

- (void)showPlusAddressEmailOverrideNotification:
    (base::OnceClosure)emailOverrideUndoCallback {
  NOTIMPLEMENTED();
}

- (void)
    showSaveCreditCardToCloud:(const autofill::CreditCard&)creditCard
            legalMessageLines:(autofill::LegalMessageLines)legalMessageLines
        saveCreditCardOptions:
            (autofill::payments::PaymentsAutofillClient::SaveCreditCardOptions)
                saveCreditCardOptions
                     callback:(autofill::payments::PaymentsAutofillClient::
                                   UploadSaveCardPromptCallback)callback {
  if (![_delegate respondsToSelector:@selector(autofillController:
                                          saveCreditCardWithSaver:)]) {
    return;
  }
  CWVCreditCardSaver* saver =
      [[CWVCreditCardSaver alloc] initWithCreditCard:creditCard
                                         saveOptions:saveCreditCardOptions
                                   legalMessageLines:legalMessageLines
                                  savePromptCallback:std::move(callback)];
  [_delegate autofillController:self saveCreditCardWithSaver:saver];
  _saver = saver;
}

- (void)handleCreditCardUploadCompleted:(BOOL)cardSaved
                               callback:(base::OnceClosure)callback {
  [_saver handleCreditCardUploadCompleted:cardSaved];
  PrefService* prefService =
      ios_web_view::WebViewBrowserState::FromBrowserState(
          _webState->GetBrowserState())
          ->GetPrefs();

  if (prefService->GetBoolean(ios_web_view::kCWVAutofillVCNUsageEnabled)) {
    std::move(callback).Run();
  }
}

- (void)showUnmaskPromptForCard:(const autofill::CreditCard&)creditCard
        cardUnmaskPromptOptions:
            (const autofill::CardUnmaskPromptOptions&)cardUnmaskPromptOptions
                       delegate:(base::WeakPtr<autofill::CardUnmaskDelegate>)
                                    delegate {
  if ([_delegate respondsToSelector:@selector(autofillController:
                                        verifyCreditCardWithVerifier:)]) {
    ios_web_view::WebViewBrowserState* browserState =
        ios_web_view::WebViewBrowserState::FromBrowserState(
            _webState->GetBrowserState());
    CWVCreditCardVerifier* verifier = [[CWVCreditCardVerifier alloc]
         initWithPrefs:browserState->GetPrefs()
        isOffTheRecord:browserState->IsOffTheRecord()
            creditCard:creditCard
                reason:cardUnmaskPromptOptions.reason
              delegate:delegate];
    [_delegate autofillController:self verifyCreditCardWithVerifier:verifier];

    // Store so verifier can receive unmask verification results later on.
    _verifier = verifier;
  }
}

- (void)didReceiveUnmaskVerificationResult:
    (autofill::payments::PaymentsAutofillClient::PaymentsRpcResult)result {
  [_verifier didReceiveUnmaskVerificationResult:result];
}

- (void)loadRiskData:(base::OnceCallback<void(const std::string&)>)callback {
  if (_verifier) {
    [_verifier loadRiskData:std::move(callback)];
  } else if (_saver) {
    [_saver loadRiskData:std::move(callback)];
  } else {
    // Request risk data from the delegate. This is needed early for VCN
    // flows, before a credit card verifier or saver object is created.
    // The delegate will call the handler block asynchronously with the data.
    if ([_delegate respondsToSelector:@selector
                   (autofillControllerLoadRiskData:riskDataHandler:)]) {
      auto wrappedRiskDataCallback =
          base::BindOnce(&base::SysNSStringToUTF8).Then(std::move(callback));

      void (^riskDataBlock)(NSString*) =
          base::CallbackToBlock(std::move(wrappedRiskDataCallback));

      [_delegate autofillControllerLoadRiskData:self
                                riskDataHandler:riskDataBlock];
    }
  }
}

- (void)showUnmaskAuthenticatorSelectorWithOptions:
            (const std::vector<autofill::CardUnmaskChallengeOption>&)
                challenge_options
                                    acceptCallback:
                                        (base::OnceCallback<void(
                                             const std::string&)>)acceptCallback
                                    cancelCallback:
                                        (base::OnceClosure)cancelCallback {
  if ([_delegate
          respondsToSelector:@selector
          (autofillController:
              showUnmaskCreditCardAuthenticatorWithChallengeOptions:acceptBlock
                                                                   :cancelBlock
                                                                   :)]) {
    NSMutableArray<CWVCardUnmaskChallengeOption*>* options =
        [NSMutableArray arrayWithCapacity:challenge_options.size()];
    for (const auto& option : challenge_options) {
      CWVCardUnmaskChallengeOption* objcOption =
          [[CWVCardUnmaskChallengeOption alloc] initWithChallengeOption:option];
      [options addObject:objcOption];
    }

    auto wrappedAcceptCallback = base::BindOnce(&base::SysNSStringToUTF8)
                                     .Then(std::move(acceptCallback));
    void (^acceptBlock)(NSString*) =
        base::CallbackToBlock(std::move(wrappedAcceptCallback));

    void (^cancelBlock)(void) =
        base::CallbackToBlock(std::move(cancelCallback));

    [_delegate autofillController:self
        showUnmaskCreditCardAuthenticatorWithChallengeOptions:options
                                                  acceptBlock:acceptBlock
                                                  cancelBlock:cancelBlock];
  }
}

- (void)
    confirmSaveAddressProfile:(const autofill::AutofillProfile&)profile
              originalProfile:(const autofill::AutofillProfile*)originalProfile
                     callback:(autofill::AutofillClient::
                                   AddressProfileSavePromptCallback)callback {
  if ([_delegate
          respondsToSelector:@selector
          (autofillController:
              confirmSaveForNewAutofillProfile:oldProfile:decisionHandler:)]) {
    CWVAutofillProfile* newProfile =
        [[CWVAutofillProfile alloc] initWithProfile:profile];
    CWVAutofillProfile* oldProfile = nil;
    if (originalProfile) {
      oldProfile =
          [[CWVAutofillProfile alloc] initWithProfile:*originalProfile];
    }
    __block auto scopedCallback = std::move(callback);
    [_delegate autofillController:self
        confirmSaveForNewAutofillProfile:newProfile
                              oldProfile:oldProfile
                         decisionHandler:^(
                             CWVAutofillProfileUserDecision decision) {
                           UserDecision userDecision;
                           switch (decision) {
                             case CWVAutofillProfileUserDecisionAccepted:
                               userDecision = UserDecision::kAccepted;
                               break;
                             case CWVAutofillProfileUserDecisionDeclined:
                               userDecision = UserDecision::kDeclined;
                               break;
                             case CWVAutofillProfileUserDecisionIgnored:
                               userDecision = UserDecision::kIgnored;
                               break;
                           }
                           std::move(scopedCallback)
                               .Run(userDecision, *newProfile.internalProfile);
                         }];
  } else {
    std::move(callback).Run(UserDecision::kUserNotAsked, profile);
  }
}

- (void)showAutofillProgressDialogOfType:(autofill::AutofillProgressUiType)type
                          cancelCallback:(base::OnceClosure)cancelCallback {
  if ([_delegate respondsToSelector:@selector
                 (autofillController:showProgressDialogOfType:cancelAction:)]) {
    CWVAutofillProgressDialogType cwvType =
        ToCWVAutofillProgressDialogType(type);

    ProceduralBlock block = base::CallbackToBlock(std::move(cancelCallback));
    [_delegate autofillController:self
         showProgressDialogOfType:cwvType
                     cancelAction:block];
  }
}

- (void)closeAutofillProgressDialogWithConfirmation:(BOOL)showConfirmation
                                 completionCallback:
                                     (base::OnceClosure)callback {
  if ([_delegate respondsToSelector:@selector
                 (autofillController:
                     closeProgressDialogWithConfirmation:completion:)]) {
    ProceduralBlock block = callback
                                ? base::CallbackToBlock(std::move(callback))
                                : (ProceduralBlock)nil;
    [_delegate autofillController:self
        closeProgressDialogWithConfirmation:showConfirmation
                                 completion:block];
  }
}

- (void)showVirtualCardEnrollmentWithEnrollmentFields:
            (const autofill::VirtualCardEnrollmentFields&)enrollmentFields
                                       acceptCallback:
                                           (base::OnceClosure)acceptCallback
                                      declineCallback:
                                          (base::OnceClosure)declineCallback {
  if ([_delegate
          respondsToSelector:@selector(autofillController:
                                 enrollCreditCardWithVCNEnrollmentManager:)]) {
    autofill::LegalMessageLines allLegalMessages;

    std::ranges::copy(enrollmentFields.google_legal_message,
                      std::back_inserter(allLegalMessages));

    std::ranges::copy(enrollmentFields.issuer_legal_message,
                      std::back_inserter(allLegalMessages));

    CWVVCNEnrollmentManager* enrollmentManager =
        [[CWVVCNEnrollmentManager alloc]
            initWithCreditCard:enrollmentFields.credit_card
             legalMessageLines:allLegalMessages
                enrollCallback:std::move(acceptCallback)
               declineCallback:std::move(declineCallback)];

    [_delegate autofillController:self
        enrollCreditCardWithVCNEnrollmentManager:enrollmentManager];

    _enrollmentManager = enrollmentManager;
  }
}

- (void)handleVirtualCardEnrollmentResult:(BOOL)cardEnrolled {
  [_enrollmentManager handleCreditCardVCNEnrollmentCompleted:cardEnrolled];
}

- (void)showCardUnmaskOtpInputDialogForCardType:
            (autofill::CreditCard::RecordType)cardType
                                challengeOption:
                                    (const autofill::CardUnmaskChallengeOption&)
                                        challengeOption
                                       delegate:
                                           (base::WeakPtr<
                                               autofill::OtpUnmaskDelegate>)
                                               delegate {
  if ([_delegate respondsToSelector:@selector(autofillController:
                                        verifyCreditCardWithOTPVerifier:)]) {
    CWVCreditCardOTPVerifier* OTPVerifier =
        [[CWVCreditCardOTPVerifier alloc] initWithCardType:cardType
                                           challengeOption:challengeOption
                                            unmaskDelegate:delegate];
    [_delegate autofillController:self
        verifyCreditCardWithOTPVerifier:OTPVerifier];

    _OTPVerifier = OTPVerifier;
  }
}

- (void)didReceiveUnmaskOtpVerificationResult:
    (autofill::OtpUnmaskResult)unmaskResult {
  [_OTPVerifier didReceiveUnmaskOtpVerificationResult:unmaskResult];
}

#pragma mark - AutofillDriverIOSBridge

- (void)fillData:(const std::vector<autofill::FormFieldData::FillData>&)fields
           section:(const autofill::Section&)section
           inFrame:(web::WebFrame*)frame
    withActionType:(autofill::mojom::FormActionType)actionType {
  [_autofillAgent fillData:fields
                   section:section
                   inFrame:frame
            withActionType:(autofill::mojom::FormActionType::kFill)];
}

- (void)fillSpecificFormField:(const autofill::FieldRendererId&)field
                    withValue:(const std::u16string)value
                      inFrame:(web::WebFrame*)frame {
  // Not supported.
}

- (void)handleParsedForms:
            (const std::vector<raw_ref<const autofill::FormStructure>>&)forms
                  inFrame:(web::WebFrame*)frame {
  if (![_delegate respondsToSelector:@selector(autofillController:
                                                     didFindForms:frameID:)]) {
    return;
  }

  NSMutableArray<CWVAutofillForm*>* autofillForms = [NSMutableArray array];
  for (const raw_ref<const autofill::FormStructure>& form : forms) {
    CWVAutofillForm* autofillForm =
        [[CWVAutofillForm alloc] initWithFormStructure:*form];
    [autofillForms addObject:autofillForm];
  }
  [_delegate autofillController:self
                   didFindForms:autofillForms
                        frameID:base::SysUTF8ToNSString(frame->GetFrameId())];
}

- (void)fillFormDataPredictions:
            (const std::vector<autofill::FormDataPredictions>&)forms
                        inFrame:(web::WebFrame*)frame {
  // Not supported.
}

- (void)notifyFormsSeen:(const std::vector<autofill::FormData>&)updatedForms
                inFrame:(web::WebFrame*)frame {
  [_autofillAgent notifyFormsSeen:updatedForms inFrame:frame];
}

- (void)fetchFormsFiltered:(std::optional<std::u16string>)formNameFilter
                   inFrame:(web::WebFrame*)frame
         completionHandler:(FormFetchCompletion)completionHandler {
  [_autofillAgent fetchFormsFiltered:std::move(formNameFilter)
                             inFrame:frame
                   completionHandler:std::move(completionHandler)];
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState
    didRegisterFormActivity:(const autofill::FormActivityParams&)params
                    inFrame:(web::WebFrame*)frame {
  DCHECK_EQ(_webState, webState);

  std::string frame_id = frame ? frame->GetFrameId() : "";

  NSString* nsFormName = base::SysUTF8ToNSString(params.form_name);
  _lastFormActivityFormRendererID = params.form_renderer_id;
  NSString* nsFieldIdentifier =
      base::SysUTF8ToNSString(params.field_identifier);
  _lastFormActivityFieldRendererID = params.field_renderer_id;
  NSString* nsFieldType = base::SysUTF8ToNSString(params.field_type);
  NSString* nsFrameID = base::SysUTF8ToNSString(frame_id);
  NSString* nsValue = base::SysUTF8ToNSString(params.value);
  NSString* nsType = base::SysUTF8ToNSString(params.type);
  BOOL userInitiated = params.has_user_gesture;

  _lastFormActivityWebFrameID = frame_id;
  _lastFormActivityTypedValue = nsValue;
  _lastFormActivityType = nsType;
  if (params.type == "focus") {
    if ([_delegate respondsToSelector:@selector
                   (autofillController:
                       didFocusOnFieldWithIdentifier:fieldType:formName:frameID
                                                    :value:userInitiated:)]) {
      [_delegate autofillController:self
          didFocusOnFieldWithIdentifier:nsFieldIdentifier
                              fieldType:nsFieldType
                               formName:nsFormName
                                frameID:nsFrameID
                                  value:nsValue
                          userInitiated:userInitiated];
    }
  } else if (params.type == "input" || params.type == "keyup") {
    // Some fields only emit 'keyup' events and not 'input' events, which would
    // result in the delegate not being notified when the field is updated.
    if ([_delegate respondsToSelector:@selector
                   (autofillController:
                       didInputInFieldWithIdentifier:fieldType:formName:frameID
                                                    :value:userInitiated:)]) {
      [_delegate autofillController:self
          didInputInFieldWithIdentifier:nsFieldIdentifier
                              fieldType:nsFieldType
                               formName:nsFormName
                                frameID:nsFrameID
                                  value:nsValue
                          userInitiated:userInitiated];
    }
  } else if (params.type == "blur") {
    if ([_delegate respondsToSelector:@selector
                   (autofillController:
                       didBlurOnFieldWithIdentifier:fieldType:formName:frameID
                                                   :value:userInitiated:)]) {
      [_delegate autofillController:self
          didBlurOnFieldWithIdentifier:nsFieldIdentifier
                             fieldType:nsFieldType
                              formName:nsFormName
                               frameID:nsFrameID
                                 value:nsValue
                         userInitiated:userInitiated];
    }
  }
}

- (void)webState:(web::WebState*)webState
    didSubmitDocumentWithFormData:(const autofill::FormData&)formData
                   hasUserGesture:(BOOL)userInitiated
                          inFrame:(web::WebFrame*)frame
                   perfectFilling:(BOOL)perfectFilling {
  if (_supportXframeSubmission) {
    return;
  }
  if ([_delegate respondsToSelector:@selector
                 (autofillController:
                     didSubmitFormWithName:frameID:perfectFilling:)]) {
    [_delegate autofillController:self
            didSubmitFormWithName:base::SysUTF16ToNSString(formData.name())
                          frameID:base::SysUTF8ToNSString(frame->GetFrameId())
                   perfectFilling:perfectFilling];
  }

  if ([_delegate
          respondsToSelector:@selector
          (autofillController:
              didSubmitFormWithName:frameID:userInitiated:perfectFilling:)]) {
    [_delegate autofillController:self
            didSubmitFormWithName:base::SysUTF16ToNSString(formData.name())
                          frameID:base::SysUTF8ToNSString(frame->GetFrameId())
                    userInitiated:userInitiated
                   perfectFilling:perfectFilling];
  }
}

#pragma mark - CRWWebFramesManagerObserver

- (void)webFramesManager:(web::WebFramesManager*)webFramesManager
    frameBecameAvailable:(web::WebFrame*)webFrame {
  CHECK(_webState);
  CHECK(_autofillManagerObservations);
  if (autofill::AutofillDriverIOS* driver =
          autofill::AutofillDriverIOS::FromWebStateAndWebFrame(_webState,
                                                               webFrame)) {
    autofill::AutofillManager& manager = driver->GetAutofillManager();
    bool observed = _autofillManagerObservations->IsObservingSource(&manager);
    // TODO(crbug.com/477633290): Cleanup check milestone.
    // Expect -frameBecameAvailable to be only called once during the frame
    // lifecyle.
    CHECK(!observed, base::NotFatalUntil::M147);
    if (!observed) {
      _autofillManagerObservations->AddObservation(&manager);
    }
  }
}

#pragma mark - Callbacks from AutofillManagerObserverBridge

- (void)onAfterFormSubmitted:(autofill::AutofillManager&)manager
                    formData:(const autofill::FormData&)form {
  // Skip immediately if the delegate doesn't handle submission to save
  // computation.
  if (![_delegate respondsToSelector:@selector
                  (autofillController:
                      didSubmitFormWithName:frameID:perfectFilling:)] &&
      ![_delegate
          respondsToSelector:@selector
          (autofillController:
              didSubmitFormWithName:frameID:userInitiated:perfectFilling:)]) {
    return;
  }

  // Cast to AutofillDriverIOS to access the web frame which is safe because
  // this code is exclusive to ios.
  web::WebFrame* frame =
      static_cast<autofill::AutofillDriverIOS&>(manager.driver()).web_frame();
  NSString* nsFrameID =
      frame ? base::SysUTF8ToNSString(frame->GetFrameId()) : @"";

  BOOL perfectFilling = autofill::IsFormDataPerfectlyFilled(form);

  if ([_delegate respondsToSelector:@selector
                 (autofillController:
                     didSubmitFormWithName:frameID:perfectFilling:)]) {
    [_delegate autofillController:self
            didSubmitFormWithName:base::SysUTF16ToNSString(form.name())
                          frameID:nsFrameID
                   perfectFilling:perfectFilling];
  }

  // Use YES as a dummy value for `userInitiated` since that bit
  // isn't supported by the _delegate implementations and we can't get that
  // information on -onAfterFormSubmitted. Also, a value of YES should cover
  // most of the submission cases that probably consist of user initiated
  // submissions, and note that computing that bit is based on a best guess so
  // it isn't that reliable.
  if ([_delegate
          respondsToSelector:@selector
          (autofillController:
              didSubmitFormWithName:frameID:userInitiated:perfectFilling:)]) {
    [_delegate autofillController:self
            didSubmitFormWithName:base::SysUTF16ToNSString(form.name())
                          frameID:nsFrameID
                    userInitiated:YES
                   perfectFilling:perfectFilling];
  }
}

- (void)
    onAutofillManagerStateChanged:(autofill::AutofillManager&)manager
                             from:(autofill::AutofillManager::LifecycleState)
                                      oldState
                               to:(autofill::AutofillManager::LifecycleState)
                                      newState {
  if (newState == autofill::AutofillManager::LifecycleState::kPendingDeletion) {
    // Stop observation when the manager is about to be deleted.
    _autofillManagerObservations->RemoveObservation(&manager);
  }
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  if (_supportXframeSubmission) {
    autofill::AutofillJavaScriptFeature::GetInstance()
        ->GetWebFramesManager(_webState)
        ->RemoveObserver(_webFramesManagerObserverBridge.get());
    _autofillManagerObservations->RemoveAllObservations();
  }
  _formActivityObserverBridge.reset();
  _autofillClient.reset();
  _webState->RemoveObserver(_webStateObserverBridge.get());
  _webStateObserverBridge.reset();
  _passwordManager.reset();
  _passwordManagerClient.reset();
  _webState = nullptr;
}

#pragma mark - PasswordManagerClientBridge

- (web::WebState*)webState {
  return _webState;
}

- (password_manager::PasswordManager*)passwordManager {
  return _passwordManager.get();
}

- (const GURL&)lastCommittedURL {
  return _webState ? _webState->GetLastCommittedURL() : GURL::EmptyGURL();
}

- (void)showSavePasswordInfoBar:
            (std::unique_ptr<password_manager::PasswordFormManagerForUI>)
                formToSave
                         manual:(BOOL)manual {
  if (![self.delegate respondsToSelector:@selector
                      (autofillController:
                          decideSavePolicyForPassword:decisionHandler:)]) {
    return;
  }

  __block std::unique_ptr<password_manager::PasswordFormManagerForUI> formPtr(
      std::move(formToSave));

  const password_manager::PasswordForm& credentials =
      formPtr->GetPendingCredentials();
  CWVPassword* password =
      [[CWVPassword alloc] initWithPasswordForm:credentials];

  __weak CWVAutofillController* weakSelf = self;
  [self.delegate autofillController:self
        decideSavePolicyForPassword:password
                    decisionHandler:^(CWVPasswordUserDecision decision) {
                      [weakSelf onDecidedSavePolicy:decision
                                    forPasswordForm:formPtr.get()];
                    }];
}

- (void)showUpdatePasswordInfoBar:
            (std::unique_ptr<password_manager::PasswordFormManagerForUI>)
                formToUpdate
                           manual:(BOOL)manual {
  if (![self.delegate respondsToSelector:@selector
                      (autofillController:
                          decideUpdatePolicyForPassword:decisionHandler:)]) {
    return;
  }

  __block std::unique_ptr<password_manager::PasswordFormManagerForUI> formPtr(
      std::move(formToUpdate));

  const password_manager::PasswordForm& credentials =
      formPtr->GetPendingCredentials();
  CWVPassword* password =
      [[CWVPassword alloc] initWithPasswordForm:credentials];

  __weak CWVAutofillController* weakSelf = self;
  [self.delegate autofillController:self
      decideUpdatePolicyForPassword:password
                    decisionHandler:^(CWVPasswordUserDecision decision) {
                      [weakSelf onDecidedUpdatePolicy:decision
                                      forPasswordForm:formPtr.get()];
                    }];
}

- (void)removePasswordInfoBarManualFallback:(BOOL)manual {
  // No op.
}

- (void)showPasswordBreachForLeakType:(CredentialLeakType)leakType
                                  URL:(const GURL&)URL
                             username:(const std::u16string&)username {
  CWVPasswordLeakType cwvLeakType = 0;
  if (password_manager::IsPasswordSaved(leakType)) {
    cwvLeakType |= CWVPasswordLeakTypeSaved;
  }
  if (password_manager::IsPasswordUsedOnOtherSites(leakType)) {
    cwvLeakType |= CWVPasswordLeakTypeUsedOnOtherSites;
  }
  if (password_manager::IsPasswordSynced(leakType)) {
    cwvLeakType |= CWVPasswordLeakTypeSynced;
  }
  if ([self.delegate
          respondsToSelector:@selector(autofillController:
                                 notifyUserOfPasswordLeakOnURL:leakType:)]) {
    [self.delegate autofillController:self
        notifyUserOfPasswordLeakOnURL:net::NSURLWithGURL(URL)
                             leakType:cwvLeakType];
  }
  if ([self.delegate respondsToSelector:@selector
                     (autofillController:
                         notifyUserOfPasswordLeakOnURL:leakType:username:)]) {
    [self.delegate autofillController:self
        notifyUserOfPasswordLeakOnURL:net::NSURLWithGURL(URL)
                             leakType:cwvLeakType
                             username:base::SysUTF16ToNSString(username)];
  }
}

- (void)showPasswordProtectionWarning:(NSString*)warningText
                           completion:(void (^)(safe_browsing::WarningAction))
                                          completion {
  // No op.
}

- (void)showCredentialProviderPromo:(CredentialProviderPromoTrigger)trigger {
  // No op
}

- (void)showSignedInWithSavedCredentialMessage {
  [self didLoginWithExistingPassword];
}

- (void)didLoginWithExistingPassword {
  if ([self.delegate respondsToSelector:@selector
                     (autofillControllerDidLoginWithExistingPassword:)]) {
    [self.delegate autofillControllerDidLoginWithExistingPassword:self];
  }
}

#pragma mark - SharedPasswordControllerDelegate

- (password_manager::PasswordManagerClient*)passwordManagerClient {
  return _passwordManagerClient.get();
}

- (void)sharedPasswordController:(SharedPasswordController*)controller
    showGeneratedPotentialPassword:(NSString*)generatedPotentialPassword
                         proactive:(BOOL)proactive
                             frame:(base::WeakPtr<web::WebFrame>)frame
                   decisionHandler:(void (^)(BOOL accept))decisionHandler {
  if ([self.delegate
          respondsToSelector:@selector(autofillController:
                                 suggestGeneratedPassword:decisionHandler:)]) {
    [self.delegate autofillController:self
             suggestGeneratedPassword:generatedPotentialPassword
                      decisionHandler:decisionHandler];
  } else {
    // If not implemented, just reject.
    decisionHandler(/*accept=*/NO);
  }
}

- (void)attachListenersForBottomSheet:
            (const std::vector<autofill::FieldRendererId>&)rendererIds
                           forFrameId:(const std::string&)frameId {
  // No op.
}

- (void)attachListenersForPasswordGenerationBottomSheet:
            (const std::vector<autofill::FieldRendererId>&)rendererIds
                                             forFrameId:
                                                 (const std::string&)frameId {
  // No op.
}

- (void)detachListenersForBottomSheet:(const std::string&)frameId {
  // No op.
}

- (void)sharedPasswordController:(SharedPasswordController*)controller
             didAcceptSuggestion:(FormSuggestion*)suggestion {
  // No op.
}

#pragma mark - Private

- (void)onDecidedSavePolicy:(CWVPasswordUserDecision)decision
            forPasswordForm:(password_manager::PasswordFormManagerForUI*)form {
  // The state may be invalid by the time this is called.
  if (![self hasValidState]) {
    return;
  }
  switch (decision) {
    case CWVPasswordUserDecisionYes:
      form->Save();
      break;
    case CWVPasswordUserDecisionNever:
      form->Blocklist();
      break;
    case CWVPasswordUserDecisionNotThisTime:
      // Do nothing.
      break;
  }
}

- (void)onDecidedUpdatePolicy:(CWVPasswordUserDecision)decision
              forPasswordForm:
                  (password_manager::PasswordFormManagerForUI*)form {
  // The state may be invalid by the time this is called.
  if (![self hasValidState]) {
    return;
  }
  // Marking a password update as "never" makes no sense as
  // the password has already been saved.
  DCHECK_NE(decision, CWVPasswordUserDecisionNever)
      << "A password update can only be accepted or "
         "ignored.";
  if (decision == CWVPasswordUserDecisionYes) {
    form->Save();
  }
}

- (BOOL)hasValidState {
  return _webState && _passwordManagerClient;
}

@end
