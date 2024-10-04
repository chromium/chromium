// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>
#import <string>
#import <vector>

#import "base/apple/foundation_util.h"
#import "base/functional/callback.h"
#import "base/notreached.h"
#import "base/ranges/algorithm.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "components/autofill/core/browser/browser_autofill_manager.h"
#import "components/autofill/core/browser/form_structure.h"
#import "components/autofill/core/browser/payments/legal_message_line.h"
#import "components/autofill/core/browser/payments/payments_autofill_client.h"
#import "components/autofill/core/browser/ui/suggestion_type.h"
#import "components/autofill/ios/browser/autofill_agent.h"
#import "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/browser/autofill_driver_ios_factory.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/browser/suggestion_controller_java_script_feature.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#import "components/password_manager/ios/shared_password_controller.h"
#import "components/sync/service/sync_service.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web_view/internal/app/application_context.h"
#import "ios/web_view/internal/autofill/cwv_autofill_controller_internal.h"
#import "ios/web_view/internal/autofill/cwv_autofill_form_internal.h"
#import "ios/web_view/internal/autofill/cwv_autofill_profile_internal.h"
#import "ios/web_view/internal/autofill/cwv_autofill_suggestion_internal.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_internal.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_saver_internal.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_verifier_internal.h"
#import "ios/web_view/internal/autofill/web_view_autocomplete_history_manager_factory.h"
#import "ios/web_view/internal/autofill/web_view_autofill_client_ios.h"
#import "ios/web_view/internal/autofill/web_view_autofill_log_router_factory.h"
#import "ios/web_view/internal/autofill/web_view_personal_data_manager_factory.h"
#import "ios/web_view/internal/autofill/web_view_strike_database_factory.h"
#import "ios/web_view/internal/passwords/cwv_password_internal.h"
#import "ios/web_view/internal/signin/web_view_identity_manager_factory.h"
#import "ios/web_view/internal/web_view_browser_state.h"
#import "ios/web_view/public/cwv_autofill_controller_delegate.h"
#import "net/base/apple/url_conversions.h"

using autofill::FieldRendererId;
using autofill::FormData;
using autofill::FormRendererId;
using UserDecision = autofill::AutofillClient::AddressPromptUserDecision;

@implementation CWVAutofillController {
  // Bridge to observe the |webState|.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;

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
  // Held weak because |_delegate| is responsible for maintaing its lifetime.
  __weak CWVCreditCardSaver* _saver;

  // The current credit card verifier. Can be nil if no verification is pending.
  // Held weak because |_delegate| is responsible for maintaing its lifetime.
  __weak CWVCreditCardVerifier* _verifier;

  std::unique_ptr<autofill::FormActivityObserverBridge>
      _formActivityObserverBridge;

  std::string _lastFormActivityWebFrameID;
  NSString* _lastFormActivityTypedValue;
  NSString* _lastFormActivityType;
  FormRendererId _lastFormActivityFormRendererID;
  FieldRendererId _lastFormActivityFieldRendererID;
}

@synthesize delegate = _delegate;

- (instancetype)
         initWithWebState:(web::WebState*)webState
           autofillClient:(std::unique_ptr<autofill::WebViewAutofillClientIOS>)
                              autofillClient
            autofillAgent:(AutofillAgent*)autofillAgent
          passwordManager:(std::unique_ptr<password_manager::PasswordManager>)
                              passwordManager
    passwordManagerClient:
        (std::unique_ptr<ios_web_view::WebViewPasswordManagerClient>)
            passwordManagerClient
       passwordController:(SharedPasswordController*)passwordController
        applicationLocale:(const std::string&)applicationLocale {
  self = [super init];
  if (self) {
    DCHECK(webState);
    _webState = webState;

    _autofillAgent = autofillAgent;

    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _webState->AddObserver(_webStateObserverBridge.get());

    _formActivityObserverBridge =
        std::make_unique<autofill::FormActivityObserverBridge>(webState, self);

    _autofillClient = std::move(autofillClient);
    _autofillClient->set_bridge(self);

    autofill::AutofillDriverIOSFactory::CreateForWebState(
        _webState, _autofillClient.get(), self, applicationLocale);

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
    _formActivityObserverBridge.reset();
    _webState->RemoveObserver(_webStateObserverBridge.get());
    _webStateObserverBridge.reset();
    _webState = nullptr;
  }
}

#pragma mark - Public Methods

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

  // Construct query.
  FormSuggestionProviderQuery* formQuery = [[FormSuggestionProviderQuery alloc]
      initWithFormName:formName
        formRendererID:_lastFormActivityFormRendererID
       fieldIdentifier:fieldIdentifier
       fieldRendererID:_lastFormActivityFieldRendererID
             fieldType:fieldType
                  type:_lastFormActivityType
            typedValue:_lastFormActivityTypedValue
               frameID:frameID];
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

- (void)focusPreviousField {
  web::WebFramesManager* framesManager =
      autofill::AutofillJavaScriptFeature::GetInstance()->GetWebFramesManager(
          _webState);
  web::WebFrame* frame =
      framesManager->GetFrameWithId(_lastFormActivityWebFrameID);

  if (!frame)
    return;

  autofill::SuggestionControllerJavaScriptFeature::GetInstance()
      ->SelectPreviousElementInFrame(frame);
}

- (void)focusNextField {
  web::WebFramesManager* framesManager =
      autofill::AutofillJavaScriptFeature::GetInstance()->GetWebFramesManager(
          _webState);
  web::WebFrame* frame =
      framesManager->GetFrameWithId(_lastFormActivityWebFrameID);

  if (!frame)
    return;

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
  base::ranges::copy_if(
      suggestions, std::back_inserter(filtered_suggestions),
      [](const autofill::Suggestion& suggestion) {
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

- (void)
    confirmSaveCreditCardToCloud:(const autofill::CreditCard&)creditCard
               legalMessageLines:(autofill::LegalMessageLines)legalMessageLines
           saveCreditCardOptions:
               (autofill::payments::PaymentsAutofillClient::
                    SaveCreditCardOptions)saveCreditCardOptions
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

- (void)handleCreditCardUploadCompleted:(BOOL)cardSaved {
  [_saver handleCreditCardUploadCompleted:cardSaved];
}

- (void)showUnmaskPromptForCard:(const autofill::CreditCard&)creditCard
        cardUnmaskPromptOptions:
            (const autofill::CardUnmaskPromptOptions&)cardUnmaskPromptOptions
                       delegate:(base::WeakPtr<autofill::CardUnmaskDelegate>)
                                    delegate {
  if ([_delegate respondsToSelector:@selector
                 (autofillController:verifyCreditCardWithVerifier:)]) {
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

#pragma mark - AutofillDriverIOSBridge

- (void)fillData:(const std::vector<autofill::FormFieldData::FillData>&)form
         inFrame:(web::WebFrame*)frame {
  [_autofillAgent fillData:form inFrame:frame];
}

- (void)fillSpecificFormField:(const autofill::FieldRendererId&)field
                    withValue:(const std::u16string)value
                      inFrame:(web::WebFrame*)frame {
  // Not supported.
}

- (void)handleParsedForms:
            (const std::vector<
                raw_ptr<autofill::FormStructure, VectorExperimental>>&)forms
                  inFrame:(web::WebFrame*)frame {
  if (![_delegate respondsToSelector:@selector(autofillController:
                                                     didFindForms:frameID:)]) {
    return;
  }

  NSMutableArray<CWVAutofillForm*>* autofillForms = [NSMutableArray array];
  for (autofill::FormStructure* form : forms) {
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

- (void)scanFormsInWebState:(web::WebState*)webState
                    inFrame:(web::WebFrame*)webFrame {
  [_autofillAgent scanFormsInWebState:webState inFrame:webFrame];
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
                          inFrame:(web::WebFrame*)frame {
  if ([_delegate respondsToSelector:@selector
                 (autofillController:
                     didSubmitFormWithName:frameID:userInitiated:)]) {
    [_delegate autofillController:self
            didSubmitFormWithName:base::SysUTF16ToNSString(formData.name())
                          frameID:base::SysUTF8ToNSString(frame->GetFrameId())
                    userInitiated:userInitiated];
  }
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
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

  [self.delegate autofillController:self
        decideSavePolicyForPassword:password
                    decisionHandler:^(CWVPasswordUserDecision decision) {
                      switch (decision) {
                        case CWVPasswordUserDecisionYes:
                          formPtr->Save();
                          break;
                        case CWVPasswordUserDecisionNever:
                          formPtr->Blocklist();
                          break;
                        case CWVPasswordUserDecisionNotThisTime:
                          // Do nothing.
                          break;
                      }
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

  [self.delegate autofillController:self
      decideUpdatePolicyForPassword:password
                    decisionHandler:^(CWVPasswordUserDecision decision) {
                      // Marking a password update as "never" makes no sense as
                      // the password has already been saved.
                      DCHECK_NE(decision, CWVPasswordUserDecisionNever)
                          << "A password update can only be accepted or "
                             "ignored.";
                      if (decision == CWVPasswordUserDecisionYes) {
                        formPtr->Save();
                      }
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

#pragma mark - SharedPasswordControllerDelegate

- (password_manager::PasswordManagerClient*)passwordManagerClient {
  return _passwordManagerClient.get();
}

- (void)sharedPasswordController:(SharedPasswordController*)controller
    showGeneratedPotentialPassword:(NSString*)generatedPotentialPassword
                         proactive:(BOOL)proactive
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

@end
