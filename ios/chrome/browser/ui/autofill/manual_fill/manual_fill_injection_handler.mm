// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_injection_handler.h"

#include "base/mac/foundation_util.h"
#import "components/autofill/ios/browser/js_suggestion_manager.h"
#import "components/autofill/ios/form_util/form_activity_observer_bridge.h"
#include "components/autofill/ios/form_util/form_activity_params.h"
#import "ios/chrome/browser/autofill/form_input_accessory_view_handler.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/form_observer_helper.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"
#include "ios/web/public/web_state/web_frames_manager.h"
#include "ios/web/public/web_state/web_state.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
// means it has a password type, the web is https and the URL can trusted.
@property(nonatomic, assign) BOOL lastActiveElementIsSecure;
@end

@implementation ManualFillInjectionHandler
@synthesize formHelper = _formHelper;
@synthesize lastActiveElementIsSecure = _lastActiveElementIsSecure;
@synthesize webStateList = _webStateList;

- (instancetype)initWithWebStateList:(WebStateList*)webStateList {
  self = [super init];
  if (self) {
    _webStateList = webStateList;
    _formHelper =
        [[FormObserverHelper alloc] initWithWebStateList:webStateList];
    _formHelper.delegate = self;
  }
  return self;
}
#pragma mark - ManualFillViewDelegate

- (void)userDidPickContent:(NSString*)content isSecure:(BOOL)isSecure {
  if (isSecure && !self.lastActiveElementIsSecure) {
    return;
  }
  [self fillLastSelectedFieldWithString:content];
}

#pragma mark - FormActivityObserver

- (void)webState:(web::WebState*)webState
    didRegisterFormActivity:(const autofill::FormActivityParams&)params
                    inFrame:(web::WebFrame*)frame {
  if (params.type != "focus") {
    return;
  }
  web::URLVerificationTrustLevel trustLevel;
  const GURL pageURL(webState->GetCurrentURL(&trustLevel));
  self.lastActiveElementIsSecure = YES;
  if (trustLevel != web::URLVerificationTrustLevel::kAbsolute ||
      !pageURL.SchemeIs(url::kHttpsScheme) || !webState->ContentIsHTML() ||
      params.field_type != "password") {
    self.lastActiveElementIsSecure = NO;
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
    [manager setWebFramesManager:web::WebFramesManager::FromWebState(webState)];
  }
  return manager;
}

#pragma mark - Document Interaction

// Injects the passed string to the active field and jumps to the next field.
- (void)fillLastSelectedFieldWithString:(NSString*)string {
  // TODO:(https://crbug.com/878388) validation / escaping of string.
  NSString* javaScriptQuery =
      [NSString stringWithFormat:
                    @"__gCrWeb.fill.setInputElementValue(\"%@\", "
                    @"document.activeElement);",
                    string];
  [self.injectionReceiver executeJavaScript:javaScriptQuery
                          completionHandler:^(id, NSError*) {
                            [self jumpToNextField];
                          }];
}

// Attempts to jump to the next field in the current form.
- (void)jumpToNextField {
  FormInputAccessoryViewHandler* handler =
      [[FormInputAccessoryViewHandler alloc] init];
  handler.JSSuggestionManager = self.suggestionManager;
  [handler selectNextElementWithoutButtonPress];
}

@end
