// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/passwords/cwv_password_controller.h"

#include <memory>

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/ios/browser/autofill_util.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/ios/account_select_fill_data.h"
#import "components/password_manager/ios/password_form_helper.h"
#import "components/password_manager/ios/password_suggestion_helper.h"
#import "ios/web/common/origin_util.h"
#include "ios/web/common/url_scheme_util.h"
#include "ios/web/public/js_messaging/web_frame.h"
#include "ios/web/public/js_messaging/web_frame_util.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ios/web_view/internal/autofill/cwv_autofill_suggestion_internal.h"
#import "ios/web_view/internal/passwords/cwv_password_internal.h"
#import "ios/web_view/internal/passwords/web_view_password_manager_client.h"
#import "ios/web_view/internal/passwords/web_view_password_manager_driver.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#import "ios/web_view/public/cwv_autofill_controller_delegate.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using autofill::FormData;
using autofill::PasswordForm;
using password_manager::AccountSelectFillData;
using password_manager::FillData;
using ios_web_view::WebViewPasswordManagerClient;
using ios_web_view::WebViewPasswordManagerDriver;
using password_manager::AccountSelectFillData;
using password_manager::GetPageURLAndCheckTrustLevel;
using password_manager::PasswordFormManagerForUI;

typedef void (^PasswordSuggestionsAvailableCompletion)(
    const AccountSelectFillData*);

@interface CWVPasswordController ()<CRWWebStateObserver,
                                    CWVPasswordManagerClientDelegate,
                                    CWVPasswordManagerDriverDelegate,
                                    PasswordFormHelperDelegate,
                                    PasswordSuggestionHelperDelegate>

// The PasswordManagerDriver owned by this PasswordController.
@property(nonatomic, readonly)
    password_manager::PasswordManagerDriver* passwordManagerDriver;

// Helper contains common password form processing logic.
@property(nonatomic, readonly) PasswordFormHelper* formHelper;

// Helper contains common password suggestion logic.
@property(nonatomic, readonly) PasswordSuggestionHelper* suggestionHelper;

// Delegate to receive password autofill suggestion callbacks.
@property(nonatomic, weak, nullable) id<CWVPasswordControllerDelegate> delegate;

// Informs the |_passwordManager| of the password forms (if any were present)
// that have been found on the page.
- (void)didFinishPasswordFormExtraction:(const std::vector<FormData>&)forms;

// Finds all password forms in DOM and sends them to the password manager for
// further processing.
- (void)findPasswordFormsAndSendThemToPasswordManager;

@end

@implementation CWVPasswordController {
  std::unique_ptr<password_manager::PasswordManager> _passwordManager;
  std::unique_ptr<WebViewPasswordManagerClient> _passwordManagerClient;
  std::unique_ptr<WebViewPasswordManagerDriver> _passwordManagerDriver;

  // The WebState this instance is observing. Will be null after
  // -webStateDestroyed: has been called.
  web::WebState* _webState;

  // Bridge to observe WebState from Objective-C.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;

  // Bridge to observe form activity in |webState_|.
  std::unique_ptr<autofill::FormActivityObserverBridge>
      _formActivityObserverBridge;
}

#pragma mark - Properties

@synthesize formHelper = _formHelper;
@synthesize suggestionHelper = _suggestionHelper;
@synthesize delegate = _delegate;

- (password_manager::PasswordManagerDriver*)passwordManagerDriver {
  return _passwordManagerDriver.get();
}

#pragma mark - Initialization

- (instancetype)initWithWebState:(web::WebState*)webState
                     andDelegate:
                         (nullable id<CWVPasswordControllerDelegate>)delegate {
  self = [super init];
  if (self) {
    DCHECK(webState);
    _webState = webState;
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _webState->AddObserver(_webStateObserverBridge.get());
    _formHelper =
        [[PasswordFormHelper alloc] initWithWebState:webState delegate:self];
    _suggestionHelper =
        [[PasswordSuggestionHelper alloc] initWithDelegate:self];
    _passwordManagerClient =
        std::make_unique<WebViewPasswordManagerClient>(self);
    _passwordManager = std::make_unique<password_manager::PasswordManager>(
        _passwordManagerClient.get());
    _passwordManagerDriver =
        std::make_unique<WebViewPasswordManagerDriver>(self);

    _delegate = delegate;

    // TODO(crbug.com/865114): Credential manager related logic
  }
  return self;
}

#pragma mark - Dealloc

- (void)dealloc {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
  }
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  DCHECK_EQ(_webState, webState);
  [self.suggestionHelper resetForNewPage];

  // Retrieve the identity of the page. In case the page might be malicous,
  // returns early.
  GURL pageURL;
  if (!GetPageURLAndCheckTrustLevel(webState, &pageURL)) {
    return;
  }

  if (!web::UrlHasWebScheme(pageURL)) {
    return;
  }

  // Notify the password manager that the page loaded so it can clear its own
  // per-page state.
  _passwordManager->DidNavigateMainFrame(/*form_may_be_submitted=*/true);

  if (!webState->ContentIsHTML()) {
    // If the current page is not HTML, it does not contain any HTML forms.
    [self didFinishPasswordFormExtraction:std::vector<FormData>()];
  }

  [self findPasswordFormsAndSendThemToPasswordManager];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
    _webStateObserverBridge.reset();
    _webState = nullptr;
  }
  _passwordManagerDriver.reset();
  _passwordManager.reset();
  _passwordManagerClient.reset();
}

#pragma mark - CWVPasswordManagerClientDelegate

- (ios_web_view::WebViewBrowserState*)browserState {
  return _webState ? ios_web_view::WebViewBrowserState::FromBrowserState(
                         _webState->GetBrowserState())
                   : nullptr;
}

- (web::WebState*)webState {
  return _webState;
}

- (password_manager::PasswordManager*)passwordManager {
  return _passwordManager.get();
}

- (const GURL&)lastCommittedURL {
  return self.formHelper.lastCommittedURL;
}

- (void)showSavePasswordInfoBar:
    (std::unique_ptr<PasswordFormManagerForUI>)formToSave {
  if (!self.delegate) {
    return;
  }
  // Use the same logic as iOS Chrome for saving password, see:
  // ios/chrome/browser/passwords/ios_chrome_save_password_infobar_delegate.mm
  __block std::unique_ptr<PasswordFormManagerForUI> formPtr(
      std::move(formToSave));

  const PasswordForm& credentials = formPtr->GetPendingCredentials();
  CWVPassword* password =
      [[CWVPassword alloc] initWithPasswordForm:credentials];

  [self.delegate passwordController:self
        decideSavePolicyForPassword:password
                    decisionHandler:^(CWVPasswordUserDecision decision) {
                      switch (decision) {
                        case CWVPasswordUserDecisionYes:
                          formPtr->Save();
                          break;
                        case CWVPasswordUserDecisionNever:
                          formPtr->PermanentlyBlacklist();
                          break;
                        default:
                          // Do nothing.
                          break;
                      }
                    }];
}

- (void)showUpdatePasswordInfoBar:
    (std::unique_ptr<PasswordFormManagerForUI>)formToUpdate {
  if (!self.delegate) {
    return;
  }
  // Use the same logic as iOS Chrome for updating password, see:
  // ios/chrome/browser/passwords/
  // ios_chrome_update_password_infobar_delegate.mm
  __block std::unique_ptr<PasswordFormManagerForUI> formPtr(
      std::move(formToUpdate));

  const PasswordForm& credentials = formPtr->GetPendingCredentials();
  CWVPassword* password =
      [[CWVPassword alloc] initWithPasswordForm:credentials];

  [self.delegate passwordController:self
      decideUpdatePolicyForPassword:password
                    decisionHandler:^(CWVPasswordUserDecision decision) {
                      DCHECK_NE(decision, CWVPasswordUserDecisionNever);
                      if (decision == CWVPasswordUserDecisionYes) {
                        formPtr->Update(credentials);
                      }
                    }];
}

- (void)showAutosigninNotification:(std::unique_ptr<PasswordForm>)formSignedIn {
  // TODO(crbug.com/865114): Implement remaining logic.
}

#pragma mark - CWVPasswordManagerDriverDelegate

- (void)fillPasswordForm:(const autofill::PasswordFormFillData&)formData {
  [self.suggestionHelper processWithPasswordFormFillData:formData];
  [self.formHelper fillPasswordForm:formData completionHandler:nil];
}

// Informs delegate that there are no saved credentials for the current page.
- (void)informNoSavedCredentials {
  [self.suggestionHelper processWithNoSavedCredentials];
}

#pragma mark - PasswordFormHelperDelegate

- (void)formHelper:(PasswordFormHelper*)formHelper
     didSubmitForm:(const FormData&)form
       inMainFrame:(BOOL)inMainFrame {
  // TODO(crbug.com/949519): remove using PasswordForm completely when the old
  // parser is gone.
  PasswordForm password_form;
  password_form.form_data = form;
  if (inMainFrame) {
    self.passwordManager->OnPasswordFormSubmitted(self.passwordManagerDriver,
                                                  password_form);
  } else {
    // Show a save prompt immediately because for iframes it is very hard to
    // figure out correctness of password forms submission.
    self.passwordManager->OnPasswordFormSubmittedNoChecksForiOS(
        self.passwordManagerDriver, password_form);
  }
}

#pragma mark - PasswordSuggestionHelperDelegate

- (void)suggestionHelperShouldTriggerFormExtraction:
    (PasswordSuggestionHelper*)suggestionHelper {
  [self findPasswordFormsAndSendThemToPasswordManager];
}

#pragma mark - Private methods

- (void)didFinishPasswordFormExtraction:(const std::vector<FormData>&)forms {
  // Do nothing if |self| has been detached.
  if (!_passwordManager) {
    return;
  }

  // TODO(crbug.com/949519): remove using PasswordForm completely when the old
  // parser is gone.
  std::vector<PasswordForm> password_forms(forms.size());
  for (size_t i = 0; i < forms.size(); ++i)
    password_forms[i].form_data = forms[i];

  if (!password_forms.empty()) {
    // TODO(crbug.com/865114):
    // Notify web_state about password forms, so that this can be taken into
    // account for the security state.

    [self.suggestionHelper updateStateOnPasswordFormExtracted];
    // Invoke the password manager callback to autofill password forms
    // on the loaded page.
    _passwordManager->OnPasswordFormsParsed(self.passwordManagerDriver,
                                            password_forms);
  } else {
    [self informNoSavedCredentials];
  }
  // Invoke the password manager callback to check if password was
  // accepted or rejected.
  _passwordManager->OnPasswordFormsRendered(self.passwordManagerDriver,
                                            password_forms,
                                            /*did_stop_loading=*/true);
}

- (void)findPasswordFormsAndSendThemToPasswordManager {
  // Read all password forms from the page and send them to the password
  // manager.
  __weak CWVPasswordController* weakSelf = self;
  [self.formHelper findPasswordFormsWithCompletionHandler:^(
                       const std::vector<FormData>& forms) {
    [weakSelf didFinishPasswordFormExtraction:forms];
  }];
}

#pragma mark - Public

- (void)fetchSuggestionsForFormWithName:(NSString*)formName
                        fieldIdentifier:(NSString*)fieldIdentifier
                              fieldType:(NSString*)fieldType
                                frameID:(NSString*)frameID
                      completionHandler:
                          (void (^)(NSArray<CWVAutofillSuggestion*>*))
                              completionHandler {
  if (!GetPageURLAndCheckTrustLevel(_webState, /*page_url=*/nullptr)) {
    completionHandler(@[]);
    return;
  }
  __weak CWVPasswordController* weakSelf = self;
  // It is necessary to call |checkIfSuggestionsAvailableForForm| before
  // |retrieveSuggestionsForForm| because the former actually queries the db,
  // while the latter merely returns them.
  // Set |type| to "focus" to trigger form extraction in
  // |PasswordSuggestionHelper|.
  [self.suggestionHelper
      checkIfSuggestionsAvailableForForm:formName
                         fieldIdentifier:fieldIdentifier
                               fieldType:fieldType
                                    type:@"focus"
                                 frameID:frameID
                             isMainFrame:YES
                                webState:_webState
                       completionHandler:^(BOOL suggestionsAvailable) {
                         CWVPasswordController* strongSelf = weakSelf;
                         if (!strongSelf || !suggestionsAvailable) {
                           completionHandler(@[]);
                           return;
                         }
                         NSArray<FormSuggestion*>* suggestions =
                             [strongSelf.suggestionHelper
                                 retrieveSuggestionsWithFormName:formName
                                                 fieldIdentifier:fieldIdentifier
                                                       fieldType:fieldType];
                         NSMutableArray<CWVAutofillSuggestion*>*
                             autofillSuggestions = [NSMutableArray array];
                         for (FormSuggestion* formSuggestion in suggestions) {
                           CWVAutofillSuggestion* autofillSuggestion =
                               [[CWVAutofillSuggestion alloc]
                                   initWithFormSuggestion:formSuggestion
                                                 formName:formName
                                          fieldIdentifier:fieldIdentifier
                                                  frameID:frameID
                                     isPasswordSuggestion:YES];
                           [autofillSuggestions addObject:autofillSuggestion];
                         }
                         completionHandler([autofillSuggestions copy]);
                       }];
}

- (void)fillSuggestion:(CWVAutofillSuggestion*)suggestion
     completionHandler:(void (^)(void))completionHandler {
  std::unique_ptr<password_manager::FillData> fillData = [self.suggestionHelper
      getFillDataForUsername:suggestion.formSuggestion.value];
  if (!fillData) {
    DLOG(WARNING) << "Failed to fill password suggestion: Fill data not found.";
    return;
  }
  [self.formHelper fillPasswordFormWithFillData:*fillData
                              completionHandler:^(BOOL success) {
                                if (!success) {
                                  DLOG(WARNING) << "Failed to fill password "
                                                   "suggestion with fill data.";
                                } else {
                                  completionHandler();
                                }
                              }];
}

@end
