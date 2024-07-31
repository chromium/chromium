// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_SITE_INFO_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_SITE_INFO_H_

#import <Foundation/Foundation.h>

class GURL;

// This represents the site info to use with manual fill for passwords and
// plus_addresses.
@interface ManualFillSiteInfo : NSObject

// The site name is the last part of the domain. In some cases it will be the
// same as the host, i.e. if it is not identified, or the host is equal to the
// site name.
@property(nonatomic, readonly) NSString* siteName;

// The host part of the credential, it should have "www." stripped if present.
@property(nonatomic, readonly) NSString* host;

// URL for the credential.
@property(nonatomic, readonly) const GURL& URL;

// Default init.
- (instancetype)initWithSiteName:(NSString*)siteName
                            host:(NSString*)host
                             URL:(const GURL&)URL NS_DESIGNATED_INITIALIZER;

// Unavailable. Please use `initWithSiteName:host:URL`.
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_SITE_INFO_H_
