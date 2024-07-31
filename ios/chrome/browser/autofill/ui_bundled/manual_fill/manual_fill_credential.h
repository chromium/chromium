// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_CREDENTIAL_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_CREDENTIAL_H_

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_site_info.h"

// This represents a user credential to use with manual fill.
@interface ManualFillCredential : ManualFillSiteInfo

// The username related to this credential.
@property(nonatomic, readonly) NSString* username;

// The password related to this credential.
@property(nonatomic, readonly) NSString* password;

// Default init.
- (instancetype)initWithUsername:(NSString*)username
                        password:(NSString*)password
                        siteName:(NSString*)siteName
                            host:(NSString*)host
                             URL:(const GURL&)URL NS_DESIGNATED_INITIALIZER;

// Unavailable. Please use `initWithUsername:password:siteName:host:URL`.
- (instancetype)initWithSiteName:(NSString*)siteName
                            host:(NSString*)host
                             URL:(const GURL&)URL NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_CREDENTIAL_H_
