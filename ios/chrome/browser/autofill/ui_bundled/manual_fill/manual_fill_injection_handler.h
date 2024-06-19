// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_INJECTION_HANDLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_INJECTION_HANDLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_content_injector.h"

@class ReauthenticationModule;
@protocol FormSuggestionClient;
@protocol SecurityAlertCommands;
class WebStateList;

// Handler with the common logic for injecting data from manual fill.
// TODO(crbug.com/40144948): Convert ManualFillInjectionHandler to browser
// agent.
@interface ManualFillInjectionHandler : NSObject <ManualFillContentInjector>

// Returns a handler using the `WebStateList` to inject JS to the active web
// state and `securityAlertPresenter` to present alerts.
- (instancetype)
      initWithWebStateList:(WebStateList*)webStateList
      securityAlertHandler:(id<SecurityAlertCommands>)securityAlertHandler
    reauthenticationModule:(ReauthenticationModule*)reauthenticationModule
      formSuggestionClient:(id<FormSuggestionClient>)formSuggestionClient;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_INJECTION_HANDLER_H_
