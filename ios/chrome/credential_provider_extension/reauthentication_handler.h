// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_REAUTHENTICATION_HANDLER_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_REAUTHENTICATION_HANDLER_H_

#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"

#import <UIKit/UIKit.h>

// Handler for showing the hardwarde reauthentication input to user, or
// a dialog about setting a passcode if nothing else is available.
@interface ReauthenticationHandler : NSObject

// Creates a handler with the given |ReauthenticationProtocol| module.
// A test instance can be passed in.
- (instancetype)initWithReauthenticationModule:
    (id<ReauthenticationProtocol>)reauthenticationModule;

// Starts reauthentication flow, which will call |completionHandler| with
// the result status, or present an alert reminding user to set a passcode
// if no hardware for reauthentication is available.
- (void)verifyUserWithCompletionHandler:
            (void (^)(ReauthenticationResult))completionHandler
        presentReminderOnViewController:(UIViewController*)viewController;

@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_REAUTHENTICATION_HANDLER_H_
