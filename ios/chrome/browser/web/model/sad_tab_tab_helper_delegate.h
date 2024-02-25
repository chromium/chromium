// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_SAD_TAB_TAB_HELPER_DELEGATE_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_SAD_TAB_TAB_HELPER_DELEGATE_H_

#import <Foundation/Foundation.h>

class SadTabTabHelper;

namespace web {
class WebState;
}

// Delegate for SadTabTabHelper.
@protocol SadTabTabHelperDelegate <NSObject>

// Asks the delegate to present Sad Tab UI.
- (void)sadTabTabHelper:(SadTabTabHelper*)tabHelper
    presentSadTabForWebState:(web::WebState*)webState
             repeatedFailure:(BOOL)repeatedFailure;

// Asks the delegate to dismiss Sad Tab UI.
- (void)sadTabTabHelperDismissSadTab:(SadTabTabHelper*)tabHelper;

// Called when WebState with Sad Tab was shown.
- (void)sadTabTabHelper:(SadTabTabHelper*)tabHelper
    didShowForRepeatedFailure:(BOOL)repeatedFailure;

// Called when WebState with Sad Tab was hidden.
- (void)sadTabTabHelperDidHide:(SadTabTabHelper*)tabHelper;

@end

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_SAD_TAB_TAB_HELPER_DELEGATE_H_
