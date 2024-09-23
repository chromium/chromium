// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_REAUTHENTICATION_REAUTHENTICATION_MODULE_H_
#define IOS_CHROME_COMMON_UI_REAUTHENTICATION_REAUTHENTICATION_MODULE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"

// A help article on how to set up a passcode.
extern const char kPasscodeArticleURL[];

@protocol SuccessfulReauthTimeAccessor <NSObject>

// Method meant to be called by the `ReauthenticationModule` to update
// the time of the last successful re-authentication.
- (void)updateSuccessfulReauthTime;

// Returns the time of the last successful re-authentication.
- (NSDate*)lastSuccessfulReauthTime;

@end

/**
 * This is used by `PasswordsDetailsCollectionViewController` and
 * `PasswordExporter|to re-authenticate the user before displaying the password
 * in plain text, allowing it to be copied, or exporting passwords.
 * TODO(crbug.com/40144947): Convert reauthentication module to model object
 * (keyed service or browser agent).
 */
@interface ReauthenticationModule : NSObject <ReauthenticationProtocol>

// The designated initializer. `successfulReauthTimeAccessor` must not be nil.
// Use `init` to have ReauthenticationModule be it's own
// `SuccessfulReauthTimeAccessor`.
- (instancetype)initWithSuccessfulReauthTimeAccessor:
    (id<SuccessfulReauthTimeAccessor>)successfulReauthTimeAccessor
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_COMMON_UI_REAUTHENTICATION_REAUTHENTICATION_MODULE_H_
