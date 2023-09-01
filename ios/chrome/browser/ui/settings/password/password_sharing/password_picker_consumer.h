// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_PICKER_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_PICKER_CONSUMER_H_

#import "base/apple/foundation_util.h"

namespace password_manager {
struct CredentialUIEntry;
}  // namespace password_manager

// Provides credentials to be displayed in the view.
@protocol PasswordPickerConsumer <NSObject>

// Sets array of credentials to be displayed in the password picker view.
- (void)setCredentials:
    (const std::vector<password_manager::CredentialUIEntry>&)credentials;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_PICKER_CONSUMER_H_
