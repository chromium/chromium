// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/url/url_util.h"

#import <UIKit/UIKit.h>

#import "base/apple/bundle_locations.h"
#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "ios/net/url_scheme_util.h"
#import "url/gurl.h"
#import "url/url_constants.h"

bool UrlIsExternalFileReference(const GURL& url) {
  return url.SchemeIs(kChromeUIScheme) &&
         base::EqualsCaseInsensitiveASCII(url.host(),
                                          kChromeUIExternalFileHost);
}

bool UrlIsDownloadedFile(const GURL& url) {
  return url.SchemeIs(kChromeUIScheme) &&
         base::EqualsCaseInsensitiveASCII(url.host(), kChromeUIDownloadsHost);
}

bool UrlHasChromeScheme(const GURL& url) {
  return url.SchemeIs(kChromeUIScheme);
}

bool UrlHasChromeScheme(NSURL* url) {
  return net::UrlSchemeIs(url, base::SysUTF8ToNSString(kChromeUIScheme));
}

bool IsUrlNtp(const GURL& url) {
  // Check for "chrome://newtab".
  if (url.SchemeIs(kChromeUIScheme)) {
    return url.host_piece() == kChromeUINewTabHost;
  }
  // Check for "about://newtab/". Since "about:" scheme is not standardised,
  // check the full path.
  if (url.SchemeIs(url::kAboutScheme)) {
    return url == kChromeUIAboutNewTabURL;
  }
  return false;
}

bool IsHandledProtocol(const std::string& scheme) {
  DCHECK_EQ(scheme, base::ToLowerASCII(scheme));
  return (scheme == url::kHttpScheme || scheme == url::kHttpsScheme ||
          scheme == url::kAboutScheme || scheme == url::kDataScheme ||
          scheme == kChromeUIScheme || SchemeIsAppStoreScheme(scheme));
}

bool ShouldLoadUrlInDesktopMode(const GURL& url,
                                HostContentSettingsMap* settings_map) {
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
      if ([allowableSchemes containsObject:scheme]) {
        _callbackScheme = [scheme copy];
      }
    }
  }
  DCHECK([_callbackScheme length]);
  return _callbackScheme;
}

- (NSArray*)allBundleURLSchemes {
  if (!_schemes) {
    NSDictionary* info = [base::apple::FrameworkBundle() infoDictionary];
    NSArray* urlTypes = [info objectForKey:@"CFBundleURLTypes"];
    NSMutableArray* schemes = [[NSMutableArray alloc] init];
    for (NSDictionary* urlType in urlTypes) {
      DCHECK([urlType isKindOfClass:[NSDictionary class]]);
      NSArray* schemesForType =
          base::apple::ObjCCastStrict<NSArray>(urlType[@"CFBundleURLSchemes"]);
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

NSSet<NSString*>* GetItmsSchemes() {
  static NSSet<NSString*>* schemes;
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    schemes = [NSSet<NSString*>
        setWithObjects:@"itms", @"itmss", @"itms-apps", @"itms-appss",
                       // There's no evidence that itms-bookss is actually
                       // supported, but over-inclusion costs less than
                       // under-inclusion.
                       @"itms-books", @"itms-bookss", nil];
  });
  return schemes;
}

bool UrlHasAppStoreScheme(const GURL& url) {
  return SchemeIsAppStoreScheme(url.scheme());
}

bool SchemeIsAppStoreScheme(const std::string& scheme) {
  return [GetItmsSchemes() containsObject:base::SysUTF8ToNSString(scheme)];
}

@end
