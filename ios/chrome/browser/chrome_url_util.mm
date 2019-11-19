// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/chrome_url_util.h"

#import <UIKit/UIKit.h>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_block.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/net/url_scheme_util.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

bool UrlIsExternalFileReference(const GURL& url) {
  return url.SchemeIs(kChromeUIScheme) &&
         base::LowerCaseEqualsASCII(url.host(), kChromeUIExternalFileHost);
}

bool UrlHasChromeScheme(const GURL& url) {
  return url.SchemeIs(kChromeUIScheme);
}

bool UrlHasChromeScheme(NSURL* url) {
  return net::UrlSchemeIs(url, base::SysUTF8ToNSString(kChromeUIScheme));
}

bool IsURLNtp(const GURL& url) {
  return UrlHasChromeScheme(url) && url.host() == kChromeUINewTabHost;
}

bool IsHandledProtocol(const std::string& scheme) {
  DCHECK_EQ(scheme, base::ToLowerASCII(scheme));
  return (scheme == url::kHttpScheme || scheme == url::kHttpsScheme ||
          scheme == url::kAboutScheme || scheme == url::kDataScheme ||
          scheme == kChromeUIScheme);
}

@implementation ChromeAppConstants {
  NSString* _callbackScheme;
  NSArray* _schemes;
}

+ (ChromeAppConstants*)sharedInstance {
  static ChromeAppConstants* g_instance = [[ChromeAppConstants alloc] init];
  return g_instance;
}

- (NSString*)getBundleURLScheme {
  if (!_callbackScheme) {
    NSSet* allowableSchemes =
        [NSSet setWithObjects:@"googlechrome", @"chromium",
                              @"ios-chrome-unittests.http", nil];
    NSArray* schemes = [self getAllBundleURLSchemes];
    for (NSString* scheme in schemes) {
      if ([allowableSchemes containsObject:scheme])
        _callbackScheme = [scheme copy];
    }
  }
  DCHECK([_callbackScheme length]);
  return _callbackScheme;
}

- (NSArray*)getAllBundleURLSchemes {
  if (!_schemes) {
    NSDictionary* info = [[NSBundle mainBundle] infoDictionary];
    NSArray* urlTypes = [info objectForKey:@"CFBundleURLTypes"];
    for (NSDictionary* urlType in urlTypes) {
      DCHECK([urlType isKindOfClass:[NSDictionary class]]);
      _schemes = [base::mac::ObjCCastStrict<NSArray>(
          urlType[@"CFBundleURLSchemes"]) copy];
    }
  }
  return _schemes;
}

- (void)setCallbackSchemeForTesting:(NSString*)scheme {
  _callbackScheme = [scheme copy];
}

@end
