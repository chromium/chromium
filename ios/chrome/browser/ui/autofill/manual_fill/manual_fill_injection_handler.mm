// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_injection_handler.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/json/string_escape.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/browser/js_suggestion_manager.h"
#import "components/autofill/ios/form_util/form_activity_observer_bridge.h"
#include "components/autofill/ios/form_util/form_activity_params.h"
#import "ios/chrome/browser/autofill/form_input_accessory_view_handler.h"
#import "ios/chrome/browser/passwords/password_tab_helper.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/form_observer_helper.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/deprecated/crw_js_injection_receiver.h"
#include "ios/web/public/js_messaging/web_frame.h"
#include "ios/web/public/js_messaging/web_frame_util.h"
#include "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The timeout for any JavaScript call in this file.
const int64_t kJavaScriptExecutionTimeoutInSeconds = 1;
}

@interface ManualFillInjectionHandler ()<FormActivityObserver>

// The object in charge of listening to form events and reporting back.
@property(nonatomic, strong) FormObserverHelper* formHelper;

// Convenience getter for the current injection reciever.
@property(nonatomic, readonly) CRWJSInjectionReceiver* injectionReceiver;

// Convenience getter for the current suggestion manager.
@property(nonatomic, readonly) JsSuggestionManager* suggestionManager;

// The WebStateList with the relevant active web state for the injection.
@property(nonatomic, assign) WebStateList* webStateList;

// YES if the last focused element is secure within its web frame. To be secure
// means the web is HTTPS and the URL is trusted.
@property(nonatomic, assign, getter=isLastFocusedElementSecure)
    BOOL lastFocusedElementSecure;

// YES if the last focused element is a password field.
@property(nonatomic, assign, getter=isLastFocusedElementPasswordField)
    BOOL lastFocusedElementPasswordField;

// The last seen frame ID with focus activity.
@property(nonatomic, assign) std::string lastFocusedElementFrameIdentifier;

// The last seen focused element identifier.
@property(nonatomic, assign) std::string lastFocusedElementIdentifier;

// The view controller this object was initialized with.
@property(weak, nonatomic, nullable, readonly)
    UIViewController* baseViewController;

// Used to present alerts.
@property(nonatomic, weak) id<AutofillSecurityAlertPresenter> alertPresenter;

@end

@implementation ManualFillInjectionHandler

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
              securityAlertPresenter:
                  (id<AutofillSecurityAlertPresenter>)securityAlertPresenter {
  self = [super init];
  if (self) {
    _webStateList = webStateList;
    _alertPresenter = securityAlertPresenter;
    _formHelper =
        [[FormObserverHelper alloc] initWithWebStateList:webStateList];
    _formHelper.delegate = self;
  }
  return self;
}

#pragma mark - ManualFillContentInjector

- (BOOL)canUserInjectInPasswordField:(BOOL)passwordField
                       requiresHTTPS:(BOOL)requiresHTTPS {
  if (passwordField && ![self isLastFocusedElementPasswordField]) {
    NSString* alertBody = l10n_util::GetNSString(
        IDS_IOS_MANUAL_FALLBACK_NOT_SECURE_PASSWORD_BODY);
    [self.alertPresenter presentSecurityWarningAlertWithText:alertBody];
    return NO;
  }
  if (requiresHTTPS && ![self isLastFocusedElementSecure]) {
    NSString* alertBody =
        l10n_util::GetNSString(IDS_IOS_MANUAL_FALLBACK_NOT_SECURE_GENERIC_BODY);
    [self.alertPresenter presentSecurityWarningAlertWithText:alertBody];
    return NO;
  }
  return YES;
}

- (void)userDidPickContent:(NSString*)content
             passwordField:(BOOL)passwordField
             requiresHTTPS:(BOOL)requiresHTTPS {
  if ([self canUserInjectInPasswordField:passwordField
                           requiresHTTPS:requiresHTTPS]) {
    [self fillLastSelectedFieldWithString:content];
  }
}

#pragma mark - FormActivityObserver

- (void)webState:(web::WebState*)webState
    didRegisterFormActivity:(const autofill::FormActivityParams&)params
                    inFrame:(web::WebFrame*)frame {
  if (params.type != "focus") {
    return;
  }
  self.lastFocusedElementSecure =
      autofill::IsContextSecureForWebState(webState);
  self.lastFocusedElementPasswordField = params.field_type == "password";
  self.lastFocusedElementIdentifier = params.field_identifier;
  DCHECK(frame);
  self.lastFocusedElementFrameIdentifier = frame->GetFrameId();
  const GURL frameSecureOrigin = frame->GetSecurityOrigin();
  if (!frameSecureOrigin.SchemeIsCryptographic()) {
    self.lastFocusedElementSecure = NO;
  }
}

#pragma mark - Getters

- (CRWJSInjectionReceiver*)injectionReceiver {
  web::WebState* webState = self.webStateList->GetActiveWebState();
  if (webState) {
    return webState->GetJSInjectionReceiver();
  }
  return nil;
}

- (JsSuggestionManager*)suggestionManager {
  JsSuggestionManager* manager = base::mac::ObjCCastStrict<JsSuggestionManager>(
      [self.injectionReceiver instanceOfClass:[JsSuggestionManager class]]);
  web::WebState* webState = self.webStateList->GetActiveWebState();
  if (webState) {
    [manager setWebFramesManager:webState->GetWebFramesManager()];
  }
  return manager;
}

#pragma mark - Private

// Injects the passed string to the active field and jumps to the next field.
- (void)fillLastSelectedFieldWithString:(NSString*)string {
  web::WebState* activeWebState = self.webStateList->GetActiveWebState();
  if (!activeWebState) {
    return;
  }
  web::WebFrame* activeWebFrame = web::GetWebFrameWithId(
      activeWebState, self.lastFocusedElementFrameIdentifier);
  if (!activeWebFrame || !activeWebFrame->CanCallJavaScriptFunction()) {
    return;
  }

  base::DictionaryValue data = base::DictionaryValue();
  data.SetString("identifier", self.lastFocusedElementIdentifier);
  data.SetString("value", base::SysNSStringToUTF16(string));
  std::vector<base::Value> parameters;
  parameters.push_back(std::move(data));

  activeWebFrame->CallJavaScriptFunction(
      "autofill.fillActiveFormField", parameters,
      base::BindOnce(^(const base::Value*) {
        [self jumpToNextField];
      }),
      base::TimeDelta::FromSeconds(kJavaScriptExecutionTimeoutInSeconds));
}

// Attempts to jump to the next field in the current form.
- (void)jumpToNextField {
  FormInputAccessoryViewHandler* handler =
      [[FormInputAccessoryViewHandler alloc] init];
  handler.JSSuggestionManager = self.suggestionManager;
  [handler setLastFocusFormActivityWebFrameID:
               base::SysUTF8ToNSString(self.lastFocusedElementFrameIdentifier)];
  [handler selectNextElementWithoutButtonPress];
}

@end
