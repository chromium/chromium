// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_UTILS_PASSWORD_UTILS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_UTILS_PASSWORD_UTILS_H_

#import <UIKit/UIKit.h>

#import <utility>

@protocol ReauthenticationProtocol;
@protocol SuccessfulReauthTimeAccessor;

namespace password_manager {

// Returns the title and the message for the password alert from an array of
// `origins`. `first`: title and `second`: message.
std::pair<NSString*, NSString*> GetPasswordAlertTitleAndMessageForOrigins(
    NSArray<NSString*>* origins);

// Creates the module containing the reauthentication mechanism for interactions
// with passwords.
id<ReauthenticationProtocol> BuildReauthenticationModule(
    id<SuccessfulReauthTimeAccessor> successfulReauthTimeAccessor = nullptr);

}  // namespace password_manager

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_UTILS_PASSWORD_UTILS_H_
