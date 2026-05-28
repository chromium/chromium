// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_MODEL_MANUAL_FILL_CREDENTIAL_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_MODEL_MANUAL_FILL_CREDENTIAL_H_

#import "ios/chrome/browser/autofill/manual_fill/model/manual_fill_site_info.h"

// This represents a user credential to use with manual fill.
@interface ManualFillCredential : ManualFillSiteInfo

// The username related to this credential.
@property(nonatomic, readonly) NSString* username;

// The password related to this credential.
@property(nonatomic, readonly) NSString* password;

// Whether this credential is a backup to a regular one.
@property(nonatomic, assign) BOOL isBackupCredential;

// The display name related to this credential, if available.
@property(nonatomic, readonly) NSString* displayName;

// Default init.
- (instancetype)initWithUsername:(NSString*)username
                        password:(NSString*)password
                     displayName:(NSString*)displayName
                        siteName:(NSString*)siteName
                            host:(NSString*)host
                             URL:(const GURL&)URL
              isBackupCredential:(BOOL)isBackupCredential
    NS_DESIGNATED_INITIALIZER;

// Unavailable. Please use
// `initWithUsername:password:displayName:siteName:host:URL:
// isBackupCredential:`.
- (instancetype)initWithSiteName:(NSString*)siteName
                            host:(NSString*)host
                             URL:(const GURL&)URL NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_MODEL_MANUAL_FILL_CREDENTIAL_H_
