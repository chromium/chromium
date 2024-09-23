// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_PLUS_ADDRESS_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_PLUS_ADDRESS_H_

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_site_info.h"

// This represents a plus address to use with manual fill.
@interface ManualFillPlusAddress : ManualFillSiteInfo

@property(nonatomic, readonly) NSString* plusAddress;

// Default init.
- (instancetype)initWithPlusAddress:(NSString*)plusAddress
                           siteName:(NSString*)siteName
                               host:(NSString*)host
                                URL:(const GURL&)URL NS_DESIGNATED_INITIALIZER;

// Unavailable. Please use `initWithPlusAddress::siteName:host:URL:`.
- (instancetype)initWithSiteName:(NSString*)siteName
                            host:(NSString*)host
                             URL:(const GURL&)URL NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_PLUS_ADDRESS_H_
