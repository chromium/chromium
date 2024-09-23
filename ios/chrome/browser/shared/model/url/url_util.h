// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_URL_URL_UTIL_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_URL_URL_UTIL_H_

#import <Foundation/Foundation.h>

#include <string>

class GURL;
class HostContentSettingsMap;

// Returns whether `url` is an external file reference.
bool UrlIsExternalFileReference(const GURL& url);

// Returns YES if `url` matches the format `chrome://downloads/fileName`
bool UrlIsDownloadedFile(const GURL& url);

// Returns true if the scheme has a chrome scheme.
bool UrlHasChromeScheme(const GURL& url);
bool UrlHasChromeScheme(NSURL* url);

// Returns YES if `url` matches chrome://newtab.
bool IsUrlNtp(const GURL& url);

// Returns true if `scheme` is handled in Chrome, or by default handlers in
// net::URLRequest.
bool IsHandledProtocol(const std::string& scheme);

// Whether or not, by default, `url` should be loaded using Desktop Mode.
bool ShouldLoadUrlInDesktopMode(const GURL& url,
                                HostContentSettingsMap* settings_map);

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

// Returns a set of NSStrings that are URL schemes for iTunes Stores.
NSSet<NSString*>* GetItmsSchemes();

// Returns whether `url` has an app store scheme.
bool UrlHasAppStoreScheme(const GURL& url);

// Returns whether `scheme` is an app store scheme.
bool SchemeIsAppStoreScheme(const std::string& scheme);

@end

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_URL_URL_UTIL_H_
