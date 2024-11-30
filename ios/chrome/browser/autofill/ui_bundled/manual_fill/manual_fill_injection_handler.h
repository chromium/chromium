// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_INJECTION_HANDLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_INJECTION_HANDLER_H_

#import <UIKit/UIKit.h>

#import "base/functional/callback_forward.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_content_injector.h"
#import "ios/web/public/web_state.h"

@class ReauthenticationModule;
@protocol FormSuggestionClient;
@protocol FormSuggestionProvider;
@protocol SecurityAlertCommands;
class WebStateList;

using AutofillProviderGetter =
    base::RepeatingCallback<id<FormSuggestionProvider>(
        web::WebState* webState)>;

// Handler with the common logic for injecting data from manual fill.
// TODO(crbug.com/40144948): Convert ManualFillInjectionHandler to browser
// agent.
@interface ManualFillInjectionHandler : NSObject <ManualFillContentInjector>

// TODO(crbug.com/380293917): Remove the `autofillProviderGetter` once we solve
// the model <-> ui_bundled dependency cycle for this.
//
// Returns a handler using the `WebStateList` to inject JS to the active web
// state and `securityAlertPresenter` to present alerts.
- (instancetype)
      initWithWebStateList:(WebStateList*)webStateList
      securityAlertHandler:(id<SecurityAlertCommands>)securityAlertHandler
    reauthenticationModule:(ReauthenticationModule*)reauthenticationModule
      formSuggestionClient:(id<FormSuggestionClient>)formSuggestionClient
    autofillProviderGetter:(AutofillProviderGetter)autofillProviderGetter;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_INJECTION_HANDLER_H_
