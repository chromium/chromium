// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CHROME_URL_UTIL_H_
#define IOS_CHROME_BROWSER_CHROME_URL_UTIL_H_

#import <Foundation/Foundation.h>

#include <string>

class GURL;

// Returns whether |url| is an external file reference.
bool UrlIsExternalFileReference(const GURL& url);

// Returns true if the scheme has a chrome scheme.
bool UrlHasChromeScheme(const GURL& url);
bool UrlHasChromeScheme(NSURL* url);

// Returns YES if |url| matches chrome://newtab.
bool IsURLNtp(const GURL& url);

// Returns true if |scheme| is handled in Chrome, or by default handlers in
// net::URLRequest.
bool IsHandledProtocol(const std::string& scheme);

// Singleton object that generates constants for Chrome iOS applications.
// Behavior of this object can be overridden by unit tests.
@interface ChromeAppConstants : NSObject

// Class method returning the singleton instance.
+ (ChromeAppConstants*)sharedInstance;

// Returns the URL scheme that launches Chrome.
- (NSString*)bundleURLScheme;

// Returns all the URL schemes that are registered on the Application Bundle.
- (NSArray*)allBundleURLSchemes;

// Method to set the scheme to callback Chrome iOS for testing.
- (void)setCallbackSchemeForTesting:(NSString*)callbackScheme;

@end

#endif  // IOS_CHROME_BROWSER_CHROME_URL_UTIL_H_
