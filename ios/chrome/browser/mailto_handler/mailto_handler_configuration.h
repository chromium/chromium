// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAILTO_HANDLER_MAILTO_HANDLER_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_MAILTO_HANDLER_MAILTO_HANDLER_CONFIGURATION_H_

#import <Foundation/Foundation.h>

#import "ios/public/provider/chrome/browser/signin/signin_sso_api.h"

class AuthenticationService;
class PrefService;
class SyncSetupService;

namespace syncer {
class SyncService;
}

// Configuration object used by the MailtoHandlerService.
@interface MailtoHandlerConfiguration : NSObject

// AuthenticationService used by MailtoHandlerService.
@property(nonatomic, assign) AuthenticationService* authService;

// SyncService used by MailtoHandlerService.
@property(nonatomic, assign) syncer::SyncService* syncService;

// SyncSetupService used by MailtoHandlerService.
@property(nonatomic, assign) SyncSetupService* syncSetupService;

// PrefService used by MailtoHandlerService.
@property(nonatomic, assign) PrefService* localState;

// SingleSignOnService used by MailtoHandlerService.
@property(nonatomic, strong) id<SingleSignOnService> ssoService;

@end

#endif  // IOS_CHROME_BROWSER_MAILTO_HANDLER_MAILTO_HANDLER_CONFIGURATION_H_
