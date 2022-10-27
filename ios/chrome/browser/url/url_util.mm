// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/url/url_util.h"

#import <UIKit/UIKit.h>

#import "base/check_op.h"
#import "base/mac/foundation_util.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "ios/net/url_scheme_util.h"
#import "url/gurl.h"
#import "url/url_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

bool UrlIsExternalFileReference(const GURL& url) {
  return url.SchemeIs(kChromeUIScheme) &&
         base::EqualsCaseInsensitiveASCII(url.host(),
                                          kChromeUIExternalFileHost);
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

bool ShouldLoadUrlInDesktopMode(const GURL& url,
                                ChromeBrowserState* browser_state) {
  HostContentSettingsMap* settings_map =
      ios::HostContentSettingsMapFactory::GetForBrowserState(browser_state);
  ContentSetting setting = settings_map->GetContentSetting(
      url, url, ContentSettingsType::REQUEST_DESKTOP_SITE);

  return setting == CONTENT_SETTING_ALLOW;
}

@implementation ChromeAppConstants {
  NSString* _callbackScheme;
  NSArray* _schemes;
}

+ (ChromeAppConstants*)sharedInstance {
  static ChromeAppConstants* g_instance = [[ChromeAppConstants alloc] init];
  return g_instance;
}

- (NSString*)bundleURLScheme {
  if (!_callbackScheme) {
    NSSet* allowableSchemes =
        [NSSet setWithObjects:@"googlechrome", @"chromium",
                              @"ios-chrome-unittests.http", nil];
    NSArray* schemes = [self allBundleURLSchemes];
    for (NSString* scheme in schemes) {
      if ([allowableSchemes containsObject:scheme])
        _callbackScheme = [scheme copy];
    }
  }
  DCHECK([_callbackScheme length]);
  return _callbackScheme;
}

- (NSArray*)allBundleURLSchemes {
  if (!_schemes) {
    NSDictionary* info = [[NSBundle mainBundle] infoDictionary];
    NSArray* urlTypes = [info objectForKey:@"CFBundleURLTypes"];
    NSMutableArray* schemes = [[NSMutableArray alloc] init];
    for (NSDictionary* urlType in urlTypes) {
      DCHECK([urlType isKindOfClass:[NSDictionary class]]);
      NSArray* schemesForType =
          base::mac::ObjCCastStrict<NSArray>(urlType[@"CFBundleURLSchemes"]);
      if (schemesForType.count) {
        [schemes addObjectsFromArray:schemesForType];
      }
    }
    _schemes = [schemes copy];
  }
  return _schemes;
}

- (void)setCallbackSchemeForTesting:(NSString*)scheme {
  _callbackScheme = [scheme copy];
}

@end
