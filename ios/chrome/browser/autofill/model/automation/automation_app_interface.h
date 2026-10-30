// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOMATION_AUTOMATION_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOMATION_AUTOMATION_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// AutomationAppInterface contains the app-side implementations for
// automation_egtest helpers which involve app-side classes.
@interface AutomationAppInterface : NSObject

// Sets autofill automation profile data from JSON string `profileJSON` in the
// app side. Returns nil on success, or an NSError if parsing or setting the
// profile fails.
+ (NSError*)setAutofillAutomationProfile:(NSString*)profileJSON;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOMATION_AUTOMATION_APP_INTERFACE_H_
