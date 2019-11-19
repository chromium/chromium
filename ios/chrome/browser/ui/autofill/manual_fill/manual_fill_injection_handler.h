// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_INJECTION_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_INJECTION_HANDLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_content_injector.h"

@protocol AutofillSecurityAlertPresenter<NSObject>

// Presents an alert with the passed body. And a title indicating something is
// not secure. Credit card numbers and passwords cannot be filled over HTTP, and
// passwords can only be filled in a password field; when a user attempts to
// autofill these a warning is displayed using the security alert presenter
- (void)presentSecurityWarningAlertWithText:(NSString*)body;

@end

class WebStateList;

// Handler with the common logic for injecting data from manual fill.
@interface ManualFillInjectionHandler : NSObject <ManualFillContentInjector>

// Returns a handler using the |WebStateList| to inject JS to the active web
// state and |securityAlertPresenter| to present alerts.
- (instancetype)initWithWebStateList:(WebStateList*)webStateList
              securityAlertPresenter:
                  (id<AutofillSecurityAlertPresenter>)securityAlertPresenter;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_INJECTION_HANDLER_H_
