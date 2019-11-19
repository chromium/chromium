// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/cwv_autofill_controller_internal.h"

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#import "components/autofill/ios/browser/autofill_agent.h"
#include "components/autofill/ios/browser/autofill_driver_ios.h"
#include "components/autofill/ios/browser/autofill_driver_ios_bridge.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/browser/js_autofill_manager.h"
#import "components/autofill/ios/browser/js_suggestion_manager.h"
#import "components/autofill/ios/form_util/form_activity_observer_bridge.h"
#include "components/autofill/ios/form_util/form_activity_params.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/sync/driver/sync_service.h"
#import "ios/web/public/deprecated/crw_js_injection_receiver.h"
#include "ios/web/public/js_messaging/web_frame.h"
#include "ios/web/public/js_messaging/web_frame_util.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#include "ios/web_view/internal/app/application_context.h"
#import "ios/web_view/internal/autofill/cwv_autofill_client_ios_bridge.h"
#import "ios/web_view/internal/autofill/cwv_autofill_form_internal.h"
#import "ios/web_view/internal/autofill/cwv_autofill_profile_internal.h"
#import "ios/web_view/internal/autofill/cwv_autofill_suggestion_internal.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_internal.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_saver_internal.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_verifier_internal.h"
#include "ios/web_view/internal/autofill/web_view_autocomplete_history_manager_factory.h"
#import "ios/web_view/internal/autofill/web_view_autofill_client_ios.h"
#include "ios/web_view/internal/autofill/web_view_personal_data_manager_factory.h"
#include "ios/web_view/internal/autofill/web_view_strike_database_factory.h"
#import "ios/web_view/internal/passwords/cwv_password_controller.h"
#include "ios/web_view/internal/signin/web_view_identity_manager_factory.h"
#import "ios/web_view/internal/sync/web_view_profile_sync_service_factory.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#include "ios/web_view/internal/webdata_services/web_view_web_data_service_wrapper_factory.h"
#import "ios/web_view/public/cwv_autofill_controller_delegate.h"

@interface CWVAutofillController ()<AutofillDriverIOSBridge,
                                    CRWWebStateObserver,
                                    CWVAutofillClientIOSBridge,
                                    FormActivityObserver,
                                    CWVPasswordControllerDelegate>

// For the field identified by |fieldIdentifier|, with type |fieldType| in the
// form named |formName|, fetches non-password suggestions that can be used to
// autofill.
// No-op if no such form and field can be found in the current page.
// |fieldIdentifier| identifies the field that had focus. It is passed to
// CWVAutofillControllerDelegate and forwarded to this method.
// |fieldType| is the 'type' attribute of the html field.
// |frameID| is the ID of the web frame containing the form.
// |completionHandler| will only be called on success.
- (void)
fetchNonPasswordSuggestionsForFormWithName:(NSString*)formName
                           fieldIdentifier:(NSString*)fieldIdentifier
                                 fieldType:(NSString*)fieldType
                                   frameID:(NSString*)frameID
                         completionHandler:
                             (void (^)(NSArray<CWVAutofillSuggestion*>*))
                                 completionHandler;

@end

@implementation CWVAutofillController {
  // Bridge to observe the |webState|.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;

  // Autofill agent associated with |webState|.
  AutofillAgent* _autofillAgent;

  // Handles password autofilling related logic.
  CWVPasswordController* _passwordController;

  // Autofill client associated with |webState|.
  std::unique_ptr<autofill::WebViewAutofillClientIOS> _autofillClient;

  // Javascript autofill manager associated with |webState|.
  JsAutofillManager* _JSAutofillManager;

  // Javascript suggestion manager associated with |webState|.
  JsSuggestionManager* _JSSuggestionManager;

  // The |webState| which this autofill controller should observe.
  web::WebState* _webState;

  // The current credit card saver. Can be nil if no save attempt is pending.
  // Held weak because |_delegate| is responsible for maintaing its lifetime.
  __weak CWVCreditCardSaver* _saver;

  // The current credit card verifier. Can be nil if no verification is pending.
  // Held weak because |_delegate| is responsible for maintaing its lifetime.
  __weak CWVCreditCardVerifier* _verifier;

  std::unique_ptr<autofill::FormActivityObserverBridge>
      _formActivityObserverBridge;

  NSString* _lastFocusFormActivityWebFrameID;
}

@synthesize delegate = _delegate;

- (instancetype)initWithWebState:(web::WebState*)webState
                   autofillAgent:(AutofillAgent*)autofillAgent
               JSAutofillManager:(JsAutofillManager*)JSAutofillManager
             JSSuggestionManager:(JsSuggestionManager*)JSSuggestionManager {
  self = [super init];
  if (self) {
    DCHECK(webState);
    _webState = webState;

    ios_web_view::WebViewBrowserState* browserState =
        ios_web_view::WebViewBrowserState::FromBrowserState(
            _webState->GetBrowserState());
    _autofillAgent = autofillAgent;

    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _webState->AddObserver(_webStateObserverBridge.get());

    _formActivityObserverBridge =
        std::make_unique<autofill::FormActivityObserverBridge>(webState, self);

    _autofillClient.reset(new autofill::WebViewAutofillClientIOS(
        browserState->GetPrefs(),
        ios_web_view::WebViewPersonalDataManagerFactory::GetForBrowserState(
            browserState->GetRecordingBrowserState()),
        ios_web_view::WebViewAutocompleteHistoryManagerFactory::
            GetForBrowserState(browserState),
        _webState, self,
        ios_web_view::WebViewIdentityManagerFactory::GetForBrowserState(
            browserState->GetRecordingBrowserState()),
        ios_web_view::WebViewStrikeDatabaseFactory::GetForBrowserState(
            browserState->GetRecordingBrowserState()),
        ios_web_view::WebViewWebDataServiceWrapperFactory::
            GetAutofillWebDataForBrowserState(
                browserState, ServiceAccessType::EXPLICIT_ACCESS),
        ios_web_view::WebViewProfileSyncServiceFactory::GetForBrowserState(
            browserState)));
    autofill::AutofillDriverIOS::PrepareForWebStateWebFrameAndDelegate(
        _webState, _autofillClient.get(), self,
        ios_web_view::ApplicationContext::GetInstance()->GetApplicationLocale(),
        autofill::AutofillManager::ENABLE_AUTOFILL_DOWNLOAD_MANAGER);

    _JSAutofillManager = JSAutofillManager;

    _JSSuggestionManager = JSSuggestionManager;

    _passwordController =
        [[CWVPasswordController alloc] initWithWebState:webState
                                            andDelegate:self];
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
  web::WebFrame* frame =
      web::GetWebFrameWithId(_webState, base::SysNSStringToUTF8(frameID));
  [_JSAutofillManager clearAutofilledFieldsForFormName:formName
                                       fieldIdentifier:fieldIdentifier
                                               inFrame:frame
                                     completionHandler:^{
                                       if (completionHandler) {
                                         completionHandler();
                                       }
                                     }];
}

- (void)fetchSuggestionsForFormWithName:(NSString*)formName
                        fieldIdentifier:(NSString*)fieldIdentifier
                              fieldType:(NSString*)fieldType
                                frameID:(NSString*)frameID
                      completionHandler:
                          (void (^)(NSArray<CWVAutofillSuggestion*>* _Nonnull))
                              completionHandler {
  __block NSArray<CWVAutofillSuggestion*>* passwordSuggestions;
  __block NSArray<CWVAutofillSuggestion*>* nonPasswordSuggestions;
  void (^resultHandler)() = ^{
    // Will continue process after both fetch operations are done.
    if (passwordSuggestions && nonPasswordSuggestions) {
      // If password suggestion is found, show password suggestion only,
      // otherwise show other autofill suggestions.
      completionHandler(passwordSuggestions.count > 0 ? passwordSuggestions
                                                      : nonPasswordSuggestions);
    }
  };

  // Fetch password suggestion first.
  [_passwordController
      fetchSuggestionsForFormWithName:formName
                      fieldIdentifier:fieldIdentifier
                            fieldType:fieldType
                              frameID:frameID
                    completionHandler:^(
                        NSArray<CWVAutofillSuggestion*>* suggestions) {
                      passwordSuggestions = suggestions;
                      resultHandler();
                    }];

  [self fetchNonPasswordSuggestionsForFormWithName:formName
                                   fieldIdentifier:fieldIdentifier
                                         fieldType:fieldType
                                           frameID:frameID
                                 completionHandler:^(
                                     NSArray<CWVAutofillSuggestion*>*
                                         suggestions) {
                                   nonPasswordSuggestions = suggestions;
                                   resultHandler();
                                 }];
}

- (void)
fetchNonPasswordSuggestionsForFormWithName:(NSString*)formName
                           fieldIdentifier:(NSString*)fieldIdentifier
                                 fieldType:(NSString*)fieldType
                                   frameID:(NSString*)frameID
                         completionHandler:
                             (void (^)(NSArray<CWVAutofillSuggestion*>*))
                                 completionHandler {
  __weak CWVAutofillController* weakSelf = self;
  id availableHandler = ^(BOOL suggestionsAvailable) {
    CWVAutofillController* strongSelf = weakSelf;
    if (!strongSelf) {
      completionHandler(@[]);
      return;
    }
    id retrieveHandler = ^(NSArray* suggestions,
                           id<FormSuggestionProvider> delegate) {
      NSMutableArray* autofillSuggestions = [NSMutableArray array];
      for (FormSuggestion* formSuggestion in suggestions) {
        CWVAutofillSuggestion* autofillSuggestion = [
            [CWVAutofillSuggestion alloc] initWithFormSuggestion:formSuggestion
                                                        formName:formName
                                                 fieldIdentifier:fieldIdentifier
                                                         frameID:frameID
                                            isPasswordSuggestion:NO];
        [autofillSuggestions addObject:autofillSuggestion];
      }
      completionHandler([autofillSuggestions copy]);
    };
    [strongSelf->_autofillAgent retrieveSuggestionsForForm:formName
                                           fieldIdentifier:fieldIdentifier
                                                 fieldType:fieldType
                                                      type:nil
                                                typedValue:nil
                                                   frameID:frameID
                                                  webState:strongSelf->_webState
                                         completionHandler:retrieveHandler];
  };
  // It is necessary to call |checkIfSuggestionsAvailableForForm| before
  // |retrieveSuggestionsForForm| because the former actually queries the db,
  // while the latter merely returns them.
  [_autofillAgent checkIfSuggestionsAvailableForForm:formName
                                     fieldIdentifier:fieldIdentifier
                                           fieldType:fieldType
                                                type:nil
                                          typedValue:nil
                                             frameID:frameID
                                         isMainFrame:YES
                                      hasUserGesture:YES
                                            webState:_webState
                                   completionHandler:availableHandler];
}

- (void)acceptSuggestion:(CWVAutofillSuggestion*)suggestion
       completionHandler:(nullable void (^)(void))completionHandler {
  if (suggestion.isPasswordSuggestion) {
    [_passwordController fillSuggestion:suggestion
                      completionHandler:^{
                        if (completionHandler) {
                          completionHandler();
                        }
                      }];
  } else {
    [_autofillAgent didSelectSuggestion:suggestion.formSuggestion
                                   form:suggestion.formName
                        fieldIdentifier:suggestion.fieldIdentifier
                                frameID:suggestion.frameID
                      completionHandler:^{
                        if (completionHandler) {
                          completionHandler();
                        }
                      }];
  }
}

- (BOOL)removeSuggestion:(CWVAutofillSuggestion*)suggestion {
  // Identifier is greater than 0 for Autofill suggestions.
  DCHECK_LT(0, suggestion.formSuggestion.identifier);

  web::WebFrame* frame = web::GetWebFrameWithId(
      _webState, base::SysNSStringToUTF8(suggestion.frameID));
  autofill::AutofillManager* manager = [self autofillManagerForFrame:frame];
  if (!manager) {
    return NO;
  }
  return manager->RemoveAutofillProfileOrCreditCard(
      suggestion.formSuggestion.identifier);
}

- (void)focusPreviousField {
  [_JSSuggestionManager
      selectPreviousElementInFrameWithID:_lastFocusFormActivityWebFrameID];
}

- (void)focusNextField {
  [_JSSuggestionManager
      selectNextElementInFrameWithID:_lastFocusFormActivityWebFrameID];
}

- (void)checkIfPreviousAndNextFieldsAreAvailableForFocusWithCompletionHandler:
    (void (^)(BOOL previous, BOOL next))completionHandler {
  [_JSSuggestionManager
      fetchPreviousAndNextElementsPresenceInFrameWithID:
          _lastFocusFormActivityWebFrameID
                                      completionHandler:completionHandler];
}

- (void)findAllFormsWithCompletionHandler:
    (void (^)(NSArray<CWVAutofillForm*>*))completionHandler {
  web::WebFramesManager* framesManager = _webState->GetWebFramesManager();
  DCHECK(framesManager);
  web::WebFrame* webFrame = framesManager->GetMainWebFrame();
  if (!webFrame) {
    completionHandler(nil);
    return;
  }

  GURL pageURL = _webState->GetLastCommittedURL();
  GURL frameOrigin = webFrame->GetSecurityOrigin();
  id fetchCompletionHandler = ^(NSString* formJSON) {
    std::vector<autofill::FormData> formDataVector;
    bool success = autofill::ExtractFormsData(
        formJSON, /*filtered=*/NO, /*form_name=*/base::string16(), pageURL,
        frameOrigin, &formDataVector);
    if (!success) {
      completionHandler(nil);
      return;
    }
    NSMutableArray<CWVAutofillForm*>* autofillForms = [NSMutableArray array];
    for (const autofill::FormData& formData : formDataVector) {
      autofill::FormStructure formStructure(formData);
      formStructure.DetermineHeuristicTypes();
      CWVAutofillForm* autofillForm =
          [[CWVAutofillForm alloc] initWithFormStructure:formStructure];
      [autofillForms addObject:autofillForm];
    }
    completionHandler([autofillForms copy]);
  };

  // Ignore empty forms.
  NSUInteger minRequiredFieldsCount = 1;
  [_JSAutofillManager
      fetchFormsWithMinimumRequiredFieldsCount:minRequiredFieldsCount
                                       inFrame:webFrame
                             completionHandler:fetchCompletionHandler];
}

#pragma mark - Utility Methods

- (autofill::AutofillManager*)autofillManagerForFrame:(web::WebFrame*)frame {
  if (!_webState || !frame) {
    return nil;
  }
  return autofill::AutofillDriverIOS::FromWebStateAndWebFrame(_webState, frame)
      ->autofill_manager();
}

#pragma mark - CWVAutofillClientIOSBridge

- (void)showAutofillPopup:(const std::vector<autofill::Suggestion>&)suggestions
            popupDelegate:
                (const base::WeakPtr<autofill::AutofillPopupDelegate>&)
                    delegate {
  // frontend_id is > 0 for Autofill suggestions, == 0 for Autocomplete
  // suggestions, and < 0 for special suggestions such as clear form.
  std::vector<autofill::Suggestion> filtered_suggestions;
  std::copy_if(suggestions.begin(), suggestions.end(),
               std::back_inserter(filtered_suggestions),
               [](autofill::Suggestion suggestion) {
                 return suggestion.frontend_id > 0 ||
                        suggestion.frontend_id ==
                            autofill::POPUP_ITEM_ID_SHOW_ACCOUNT_CARDS ||
                        suggestion.frontend_id ==
                            autofill::POPUP_ITEM_ID_CLEAR_FORM;
               });
  [_autofillAgent showAutofillPopup:filtered_suggestions
                      popupDelegate:delegate];
}

- (void)hideAutofillPopup {
  [_autofillAgent hideAutofillPopup];
}

- (void)confirmSaveAutofillProfile:(const autofill::AutofillProfile&)profile
                          callback:(base::OnceClosure)callback {
  if (![_delegate respondsToSelector:@selector
                  (autofillController:
                      decideSavePolicyForAutofillProfile:decisionHandler:)]) {
    return;
  }

  __block base::OnceClosure scopedCallback = std::move(callback);
  CWVAutofillProfile* autofillProfile =
      [[CWVAutofillProfile alloc] initWithProfile:profile];
  [_delegate autofillController:self
      decideSavePolicyForAutofillProfile:autofillProfile
                         decisionHandler:^(BOOL save) {
                           if (save) {
                             std::move(scopedCallback).Run();
                           }
                         }];
}

- (void)confirmSaveCreditCardLocally:(const autofill::CreditCard&)creditCard
               saveCreditCardOptions:
                   (autofill::AutofillClient::SaveCreditCardOptions)
                       saveCreditCardOptions
                            callback:(autofill::AutofillClient::
                                          LocalSaveCardPromptCallback)callback {
  if (![_delegate respondsToSelector:@selector(autofillController:
                                          saveCreditCardWithSaver:)]) {
    return;
  }

  CWVCreditCardSaver* saver = [[CWVCreditCardSaver alloc]
            initWithCreditCard:creditCard
                   saveOptions:saveCreditCardOptions
             willUploadToCloud:NO
             legalMessageLines:autofill::LegalMessageLines()
      uploadSavePromptCallback:autofill::AutofillClient::
                                   UploadSaveCardPromptCallback()
       localSavePromptCallback:std::move(callback)];
  [_delegate autofillController:self saveCreditCardWithSaver:saver];
  _saver = saver;
}

- (void)
    confirmSaveCreditCardToCloud:(const autofill::CreditCard&)creditCard
               legalMessageLines:(autofill::LegalMessageLines)legalMessageLines
           saveCreditCardOptions:
               (autofill::AutofillClient::SaveCreditCardOptions)
                   saveCreditCardOptions
                        callback:(autofill::AutofillClient::
                                      UploadSaveCardPromptCallback)callback {
  if (![_delegate respondsToSelector:@selector(autofillController:
                                          saveCreditCardWithSaver:)]) {
    return;
  }
  CWVCreditCardSaver* saver = [[CWVCreditCardSaver alloc]
            initWithCreditCard:creditCard
                   saveOptions:saveCreditCardOptions
             willUploadToCloud:YES
             legalMessageLines:legalMessageLines
      uploadSavePromptCallback:std::move(callback)
       localSavePromptCallback:autofill::AutofillClient::
                                   LocalSaveCardPromptCallback()];
  [_delegate autofillController:self saveCreditCardWithSaver:saver];
  _saver = saver;
}

- (void)handleCreditCardUploadCompleted:(BOOL)cardSaved {
  [_saver handleCreditCardUploadCompleted:cardSaved];
}

- (void)
showUnmaskPromptForCard:(const autofill::CreditCard&)creditCard
                 reason:(autofill::AutofillClient::UnmaskCardReason)reason
               delegate:(base::WeakPtr<autofill::CardUnmaskDelegate>)delegate {
  if ([_delegate respondsToSelector:@selector
                 (autofillController:verifyCreditCardWithVerifier:)]) {
    ios_web_view::WebViewBrowserState* browserState =
        ios_web_view::WebViewBrowserState::FromBrowserState(
            _webState->GetBrowserState());
    CWVCreditCardVerifier* verifier = [[CWVCreditCardVerifier alloc]
         initWithPrefs:browserState->GetPrefs()
        isOffTheRecord:browserState->IsOffTheRecord()
            creditCard:creditCard
                reason:reason
              delegate:delegate];
    [_delegate autofillController:self verifyCreditCardWithVerifier:verifier];

    // Store so verifier can receive unmask verification results later on.
    _verifier = verifier;
  }
}

- (void)didReceiveUnmaskVerificationResult:
    (autofill::AutofillClient::PaymentsRpcResult)result {
  [_verifier didReceiveUnmaskVerificationResult:result];
}

- (void)loadRiskData:(base::OnceCallback<void(const std::string&)>)callback {
  if (_verifier) {
    [_verifier loadRiskData:std::move(callback)];
  } else if (_saver) {
    [_saver loadRiskData:std::move(callback)];
  }
}

- (void)propagateAutofillPredictionsForForms:
    (const std::vector<autofill::FormStructure*>&)forms {
  // Not supported.
}

#pragma mark - AutofillDriverIOSBridge

- (void)fillFormData:(const autofill::FormData&)form
             inFrame:(web::WebFrame*)frame {
  [_autofillAgent fillFormData:form inFrame:frame];
}

- (void)fillFormDataPredictions:
            (const std::vector<autofill::FormDataPredictions>&)forms
                        inFrame:(web::WebFrame*)frame {
  // Not supported.
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  if (!success) {
    return;
  }

  web::WebFramesManager* framesManager = _webState->GetWebFramesManager();
  DCHECK(framesManager);
  web::WebFrame* webFrame = framesManager->GetMainWebFrame();
  if (!webFrame) {
    return;
  }

  // Start listening for any form mutations.
  [_JSAutofillManager toggleTrackingFormMutations:YES inFrame:webFrame];
}

- (void)webState:(web::WebState*)webState
    didRegisterFormActivity:(const autofill::FormActivityParams&)params
                    inFrame:(web::WebFrame*)frame {
  DCHECK_EQ(_webState, webState);

  NSString* nsFormName = base::SysUTF8ToNSString(params.form_name);
  NSString* nsFieldIdentifier =
      base::SysUTF8ToNSString(params.field_identifier);
  NSString* nsFieldType = base::SysUTF8ToNSString(params.field_type);
  NSString* nsFrameID = base::SysUTF8ToNSString(GetWebFrameId(frame));
  NSString* nsValue = base::SysUTF8ToNSString(params.value);
  if (params.type == "focus") {
    _lastFocusFormActivityWebFrameID = nsFrameID;
    if ([_delegate respondsToSelector:@selector
                   (autofillController:didFocusOnFieldWithIdentifier:fieldType
                                         :formName:frameID:value:)]) {
      [_delegate autofillController:self
          didFocusOnFieldWithIdentifier:nsFieldIdentifier
                              fieldType:nsFieldType
                               formName:nsFormName
                                frameID:nsFrameID
                                  value:nsValue];
    }
  } else if (params.type == "input") {
    _lastFocusFormActivityWebFrameID = nsFrameID;
    if ([_delegate respondsToSelector:@selector
                   (autofillController:didInputInFieldWithIdentifier:fieldType
                                         :formName:frameID:value:)]) {
      [_delegate autofillController:self
          didInputInFieldWithIdentifier:nsFieldIdentifier
                              fieldType:nsFieldType
                               formName:nsFormName
                                frameID:nsFrameID
                                  value:nsValue];
    }
  } else if (params.type == "blur") {
    if ([_delegate respondsToSelector:@selector
                   (autofillController:didBlurOnFieldWithIdentifier:fieldType
                                         :formName:frameID:value:)]) {
      [_delegate autofillController:self
          didBlurOnFieldWithIdentifier:nsFieldIdentifier
                             fieldType:nsFieldType
                              formName:nsFormName
                               frameID:nsFrameID
                                 value:nsValue];
    }
  } else if (params.type == "form_changed") {
    if ([_delegate respondsToSelector:@selector
                   (autofillControllerDidInsertFormElements:)]) {
      [_delegate autofillControllerDidInsertFormElements:self];
    }
  }
}

- (void)webState:(web::WebState*)webState
    didSubmitDocumentWithFormNamed:(const std::string&)formName
                          withData:(const std::string&)formData
                    hasUserGesture:(BOOL)userInitiated
                   formInMainFrame:(BOOL)isMainFrame
                           inFrame:(web::WebFrame*)frame {
  if ([_delegate respondsToSelector:@selector
                 (autofillController:didSubmitFormWithName:userInitiated
                                       :isMainFrame:)]) {
    [_delegate autofillController:self
            didSubmitFormWithName:base::SysUTF8ToNSString(formName)
                    userInitiated:userInitiated
                      isMainFrame:isMainFrame];
  }
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  _formActivityObserverBridge.reset();
  _autofillClient.reset();
  _webState->RemoveObserver(_webStateObserverBridge.get());
  _webStateObserverBridge.reset();
  _webState = nullptr;
}

#pragma mark - CWVPasswordControllerDelegate

- (void)passwordController:(CWVPasswordController*)passwordController
    decideSavePolicyForPassword:(CWVPassword*)password
                decisionHandler:(void (^)(CWVPasswordUserDecision decision))
                                    decisionHandler {
  if ([self.delegate respondsToSelector:@selector
                     (autofillController:
                         decideSavePolicyForPassword:decisionHandler:)]) {
    [self.delegate autofillController:self
          decideSavePolicyForPassword:password
                      decisionHandler:decisionHandler];
  }
}

- (void)passwordController:(CWVPasswordController*)passwordController
    decideUpdatePolicyForPassword:(CWVPassword*)password
                  decisionHandler:(void (^)(CWVPasswordUserDecision decision))
                                      decisionHandler {
  if ([self.delegate respondsToSelector:@selector
                     (autofillController:
                         decideUpdatePolicyForPassword:decisionHandler:)]) {
    [self.delegate autofillController:self
        decideUpdatePolicyForPassword:password
                      decisionHandler:decisionHandler];
  }
}

@end
