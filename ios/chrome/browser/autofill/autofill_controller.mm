// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_controller.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#import "components/autofill/ios/browser/autofill_agent.h"
#import "components/autofill/ios/browser/autofill_client_ios_bridge.h"
#include "components/autofill/ios/browser/autofill_driver_ios.h"
#include "components/autofill/ios/browser/autofill_driver_ios_bridge.h"
#include "components/autofill/ios/browser/autofill_switches.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/browser/form_suggestion_provider.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/ui/autofill/chrome_autofill_client_ios.h"
#include "ios/web/public/web_state/web_frame.h"
#include "ios/web/public/web_state/web_frame_util.h"
#import "ios/web/public/web_state/web_frames_manager.h"
#import "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using autofill::AutofillPopupDelegate;

@interface AutofillController ()<AutofillClientIOSBridge,
                                 AutofillDriverIOSBridge> {
  AutofillAgent* _autofillAgent;
  std::unique_ptr<autofill::ChromeAutofillClientIOS> _autofillClient;
  web::WebState* _webState;
}

@end

@implementation AutofillController

@synthesize browserState = _browserState;

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                            webState:(web::WebState*)webState
                       autofillAgent:(AutofillAgent*)autofillAgent
           passwordGenerationManager:
               (password_manager::PasswordGenerationManager*)
                   passwordGenerationManager
                     downloadEnabled:(BOOL)downloadEnabled {
  DCHECK(browserState);
  DCHECK(webState);
  self = [super init];
  if (self) {
    _browserState = browserState;
    _webState = webState;
    infobars::InfoBarManager* infobarManager =
        InfoBarManagerImpl::FromWebState(webState);
    DCHECK(infobarManager);
    _autofillClient.reset(new autofill::ChromeAutofillClientIOS(
        browserState, webState, infobarManager, self,
        passwordGenerationManager));
    autofill::AutofillDriverIOS::PrepareForWebStateWebFrameAndDelegate(
        webState, _autofillClient.get(), self,
        GetApplicationContext()->GetApplicationLocale(),
        downloadEnabled
            ? autofill::AutofillManager::ENABLE_AUTOFILL_DOWNLOAD_MANAGER
            : autofill::AutofillManager::DISABLE_AUTOFILL_DOWNLOAD_MANAGER);
    _autofillAgent = autofillAgent;
  }
  return self;
}

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
           passwordGenerationManager:
               (password_manager::PasswordGenerationManager*)
                   passwordGenerationManager
                            webState:(web::WebState*)webState {
  AutofillAgent* autofillAgent =
      [[AutofillAgent alloc] initWithPrefService:browserState->GetPrefs()
                                        webState:webState];
  return [self initWithBrowserState:browserState
                           webState:webState
                      autofillAgent:autofillAgent
          passwordGenerationManager:passwordGenerationManager
                    downloadEnabled:YES];
}

- (void)dealloc {
  DCHECK(!_autofillAgent);  // detachFromWebController must have been called.
}

- (id<FormSuggestionProvider>)suggestionProvider {
  return _autofillAgent;
}

- (void)detachFromWebState {
  [_autofillAgent detachFromWebState];
  _autofillAgent = nil;
  _webState = nullptr;
}

- (void)setBaseViewController:(UIViewController*)baseViewController {
  _autofillClient->SetBaseViewController(baseViewController);
}

// Return the AutofillManager associated to |frame|.
// If autofill in iframes is disabled, ignore the frame parameter and return the
// AutofillManager associated with |_webState|.
- (autofill::AutofillManager*)autofillManagerForFrame:(web::WebFrame*)frame {
  if (!_webState) {
    return nil;
  }
  if (autofill::switches::IsAutofillIFrameMessagingEnabled() && !frame) {
    return nil;
  }
  return autofill::AutofillDriverIOS::FromWebStateAndWebFrame(_webState, frame)
      ->autofill_manager();
}

#pragma mark - AutofillClientIOSBridge

- (void)
showAutofillPopup:(const std::vector<autofill::Suggestion>&)popup_suggestions
    popupDelegate:(const base::WeakPtr<AutofillPopupDelegate>&)delegate {
  // Convert the suggestions into an NSArray for the keyboard.
  NSMutableArray* suggestions = [[NSMutableArray alloc] init];
  for (size_t i = 0; i < popup_suggestions.size(); ++i) {
    // In the Chromium implementation the identifiers represent rows on the
    // drop down of options. These include elements that aren't relevant to us
    // such as separators ... see blink::WebAutofillClient::MenuItemIDSeparator
    // for example. We can't include that enum because it's from WebKit, but
    // fortunately almost all the entries we are interested in (profile or
    // autofill entries) are zero or positive. Negative entries we are
    // interested in is autofill::POPUP_ITEM_ID_CLEAR_FORM, used to show the
    // "clear form" button and autofill::POPUP_ITEM_ID_GOOGLE_PAY_BRANDING, used
    // to show the "Google Pay" branding.
    NSString* value = nil;
    NSString* displayDescription = nil;
    if (popup_suggestions[i].frontend_id >= 0) {
      // Value will contain the text to be filled in the selected element while
      // displayDescription will contain a summary of the data to be filled in
      // the other elements.
      value = base::SysUTF16ToNSString(popup_suggestions[i].value);
      displayDescription = base::SysUTF16ToNSString(popup_suggestions[i].label);
    } else if (popup_suggestions[i].frontend_id ==
               autofill::POPUP_ITEM_ID_CLEAR_FORM) {
      // Show the "clear form" button.
      value = base::SysUTF16ToNSString(popup_suggestions[i].value);
    } else if (popup_suggestions[i].frontend_id ==
               autofill::POPUP_ITEM_ID_GOOGLE_PAY_BRANDING) {
      // Show "GPay branding" icon
      value = base::SysUTF16ToNSString(popup_suggestions[i].value);
    }

    if (!value)
      continue;

    FormSuggestion* suggestion = [FormSuggestion
        suggestionWithValue:value
         displayDescription:displayDescription
                       icon:base::SysUTF16ToNSString(popup_suggestions[i].icon)
                 identifier:popup_suggestions[i].frontend_id];
    [suggestions addObject:suggestion];
  }
  [_autofillAgent onSuggestionsReady:suggestions popupDelegate:delegate];

  // The parameter is an optional callback.
  if (delegate)
    delegate->OnPopupShown();
}

- (void)hideAutofillPopup {
  [_autofillAgent onSuggestionsReady:@[]
                       popupDelegate:base::WeakPtr<AutofillPopupDelegate>()];
}

#pragma mark - AutofillDriverIOSBridge

- (void)onFormDataFilled:(uint16_t)query_id
                 inFrame:(web::WebFrame*)frame
                  result:(const autofill::FormData&)result {
  [_autofillAgent onFormDataFilled:result inFrame:frame];
  autofill::AutofillManager* manager = [self autofillManagerForFrame:frame];
  if (manager)
    manager->OnDidFillAutofillFormData(result, base::TimeTicks::Now());
}

- (void)sendAutofillTypePredictionsToRenderer:
            (const std::vector<autofill::FormDataPredictions>&)forms
                                      toFrame:(web::WebFrame*)frame {
  [_autofillAgent renderAutofillTypePredictions:forms inFrame:frame];
}

@end
