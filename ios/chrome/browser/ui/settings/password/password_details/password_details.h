// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_H_

#import <Foundation/Foundation.h>

#include "url/gurl.h"

namespace password_manager {
struct PasswordForm;
}  // namespace password_manager

// Object which is used by |PasswordDetailsViewController| to show
// information about password.
@interface PasswordDetails : NSObject

// Short version of website.
@property(nonatomic, copy, readonly) NSString* origin;

// Associated website.
@property(nonatomic, copy, readonly) NSString* website;

// Associated username.
@property(nonatomic, copy) NSString* username;

// The federation providing this credential, if any.
@property(nonatomic, copy, readonly) NSString* federation;

// Associated password.
@property(nonatomic, copy) NSString* password;

// Whether password is compromised or not.
@property(nonatomic, assign, getter=isCompromised) BOOL compromised;

// URL which allows to change the password of compromised credential.
@property(nonatomic, readonly) GURL changePasswordURL;

- (instancetype)initWithPasswordForm:(const password_manager::PasswordForm&)form
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_H_
