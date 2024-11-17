// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAILTO_HANDLER_MODEL_MAILTO_HANDLER_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_MAILTO_HANDLER_MODEL_MAILTO_HANDLER_CONFIGURATION_H_

#import <Foundation/Foundation.h>

#import "ios/public/provider/chrome/browser/signin/signin_sso_api.h"

class ChromeAccountManagerService;
class PrefService;

namespace signin {
class IdentityManager;
}  // namespace signin

// Configuration object used by the MailtoHandlerService.
@interface MailtoHandlerConfiguration : NSObject

// IdentityManager used by MailtoHandlerService.
@property(nonatomic, assign) signin::IdentityManager* identityManager;

// ChromeAccountManagerService used by MailtoHandlerService.
@property(nonatomic, assign) ChromeAccountManagerService* accountManager;

// PrefService used by MailtoHandlerService.
@property(nonatomic, assign) PrefService* localState;

// SingleSignOnService used by MailtoHandlerService.
@property(nonatomic, strong) id<SingleSignOnService> singleSignOnService;

@end

#endif  // IOS_CHROME_BROWSER_MAILTO_HANDLER_MODEL_MAILTO_HANDLER_CONFIGURATION_H_
