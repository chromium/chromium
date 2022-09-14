// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_CREDENTIAL_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_CREDENTIAL_H_

#import <Foundation/Foundation.h>

class GURL;

// This represents a user credential to use with manual fill.
@interface ManualFillCredential : NSObject

// The username related to this credential.
@property(nonatomic, readonly) NSString* username;

// The password related to this credential.
@property(nonatomic, readonly) NSString* password;

// The site name is the last part of the domain. In some cases it will be the
// same as the host, i.e. if is not identified, or the host is equal to the site
// name.
@property(nonatomic, readonly) NSString* siteName;

// The host part of the credential, it should have "www." stripped if present.
@property(nonatomic, readonly) NSString* host;

// URL for the credential.
@property(nonatomic, readonly) const GURL& URL;

// Default init.
- (instancetype)initWithUsername:(NSString*)username
                        password:(NSString*)password
                        siteName:(NSString*)siteName
                            host:(NSString*)host
                             URL:(const GURL&)URL NS_DESIGNATED_INITIALIZER;

// Unavailable. Please use `initWithUsername:password:siteName:host:`.
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_CREDENTIAL_H_
