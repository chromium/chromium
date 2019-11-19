// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/startup/chrome_app_startup_parameters.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/common/app_group/app_group_constants.h"
#include "ios/chrome/common/x_callback_url.h"
#import "net/base/mac/url_conversions.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Key of the UMA Startup.MobileSessionStartAction histogram.
const char kUMAMobileSessionStartActionHistogram[] =
    "Startup.MobileSessionStartAction";

const char kApplicationGroupCommandDelay[] =
    "Startup.ApplicationGroupCommandDelay";

// URL Query String parameter to indicate that this openURL: request arrived
// here due to a Smart App Banner presentation on a Google.com page.
NSString* const kSmartAppBannerKey = @"safarisab";

const CGFloat kAppGroupTriggersVoiceSearchTimeout = 15.0;

// Values of the UMA Startup.MobileSessionStartAction histogram.
enum MobileSessionStartAction {
  START_ACTION_OPEN_HTTP = 0,
  START_ACTION_OPEN_HTTPS,
  START_ACTION_OPEN_FILE,
  START_ACTION_XCALLBACK_OPEN,
  START_ACTION_XCALLBACK_OTHER,
  START_ACTION_OTHER,
  START_ACTION_XCALLBACK_APPGROUP_COMMAND,
  MOBILE_SESSION_START_ACTION_COUNT,
};

// Values of the UMA iOS.SearchExtension.Action histogram.
enum SearchExtensionAction {
  ACTION_NO_ACTION,
  ACTION_NEW_SEARCH,
  ACTION_NEW_INCOGNITO_SEARCH,
  ACTION_NEW_VOICE_SEARCH,
  ACTION_NEW_QR_CODE_SEARCH,
  ACTION_OPEN_URL,
  ACTION_SEARCH_TEXT,
  ACTION_SEARCH_IMAGE,
  SEARCH_EXTENSION_ACTION_COUNT,
};

}  // namespace

@implementation ChromeAppStartupParameters {
  NSString* _secureSourceApp;
  NSString* _declaredSourceApp;
}

- (instancetype)initWithExternalURL:(const GURL&)externalURL
                  declaredSourceApp:(NSString*)declaredSourceApp
                    secureSourceApp:(NSString*)secureSourceApp
                        completeURL:(NSURL*)completeURL {
  self = [super initWithExternalURL:externalURL
                        completeURL:net::GURLWithNSURL(completeURL)];
  if (self) {
    _declaredSourceApp = [declaredSourceApp copy];
    _secureSourceApp = [secureSourceApp copy];
  }
  return self;
}

+ (instancetype)newChromeAppStartupParametersWithURL:(NSURL*)completeURL
                               fromSourceApplication:(NSString*)appId {
  GURL gurl = net::GURLWithNSURL(completeURL);

  if (!gurl.is_valid() || gurl.scheme().length() == 0)
    return nil;

  // TODO(crbug.com/228098): Temporary fix.
  if (IsXCallbackURL(gurl)) {
    NSString* action = [completeURL path];
    // Currently only "open" and "extension-command" are supported.
    // Other actions are being considered (see b/6914153).
    if ([action
            isEqualToString:
                [NSString
                    stringWithFormat:
                        @"/%s", app_group::kChromeAppGroupXCallbackCommand]]) {
      UMA_HISTOGRAM_ENUMERATION(kUMAMobileSessionStartActionHistogram,
                                START_ACTION_XCALLBACK_APPGROUP_COMMAND,
                                MOBILE_SESSION_START_ACTION_COUNT);
      return [ChromeAppStartupParameters
          newExtensionCommandAppStartupParametersFromWithURL:completeURL
                                       fromSourceApplication:appId];
    }

    if (![action isEqualToString:@"/open"]) {
      UMA_HISTOGRAM_ENUMERATION(kUMAMobileSessionStartActionHistogram,
                                START_ACTION_XCALLBACK_OTHER,
                                MOBILE_SESSION_START_ACTION_COUNT);
      return nil;
    }

    UMA_HISTOGRAM_ENUMERATION(kUMAMobileSessionStartActionHistogram,
                              START_ACTION_XCALLBACK_OPEN,
                              MOBILE_SESSION_START_ACTION_COUNT);

    std::map<std::string, std::string> parameters =
        ExtractQueryParametersFromXCallbackURL(gurl);
    GURL url = GURL(parameters["url"]);
    if (!url.is_valid() ||
        (!url.SchemeIs(url::kHttpScheme) && !url.SchemeIs(url::kHttpsScheme))) {
      return nil;
    }

    return [[ChromeAppStartupParameters alloc] initWithExternalURL:url
                                                 declaredSourceApp:appId
                                                   secureSourceApp:nil
                                                       completeURL:completeURL];

  } else if (gurl.SchemeIsFile()) {
    UMA_HISTOGRAM_ENUMERATION(kUMAMobileSessionStartActionHistogram,
                              START_ACTION_OPEN_FILE,
                              MOBILE_SESSION_START_ACTION_COUNT);
    // |url| is the path to a file received from another application.
    GURL::Replacements replacements;
    const std::string host(kChromeUIExternalFileHost);
    std::string filename = gurl.ExtractFileName();
    replacements.SetPathStr(filename);
    replacements.SetSchemeStr(kChromeUIScheme);
    replacements.SetHostStr(host);
    GURL externalURL = gurl.ReplaceComponents(replacements);
    if (!externalURL.is_valid())
      return nil;
    return [[ChromeAppStartupParameters alloc] initWithExternalURL:externalURL
                                                 declaredSourceApp:appId
                                                   secureSourceApp:nil
                                                       completeURL:completeURL];
  } else {
    // Replace the scheme with https or http depending on whether the input
    // |url| scheme ends with an 's'.
    BOOL useHttps = gurl.scheme()[gurl.scheme().length() - 1] == 's';
    MobileSessionStartAction action =
        useHttps ? START_ACTION_OPEN_HTTPS : START_ACTION_OPEN_HTTP;
    UMA_HISTOGRAM_ENUMERATION(kUMAMobileSessionStartActionHistogram, action,
                              MOBILE_SESSION_START_ACTION_COUNT);
    GURL::Replacements replace_scheme;
    if (useHttps)
      replace_scheme.SetSchemeStr(url::kHttpsScheme);
    else
      replace_scheme.SetSchemeStr(url::kHttpScheme);
    GURL externalURL = gurl.ReplaceComponents(replace_scheme);
    if (!externalURL.is_valid())
      return nil;
    return [[ChromeAppStartupParameters alloc] initWithExternalURL:externalURL
                                                 declaredSourceApp:appId
                                                   secureSourceApp:nil
                                                       completeURL:completeURL];
  }
}

+ (instancetype)newExtensionCommandAppStartupParametersFromWithURL:(NSURL*)url
                                             fromSourceApplication:
                                                 (NSString*)appId {
  NSUserDefaults* sharedDefaults = app_group::GetGroupUserDefaults();

  NSString* commandDictionaryPreference =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandPreference);
  NSDictionary* commandDictionary = base::mac::ObjCCast<NSDictionary>(
      [sharedDefaults objectForKey:commandDictionaryPreference]);

  [sharedDefaults removeObjectForKey:commandDictionaryPreference];

  // |sharedDefaults| is used for communication between apps. Synchronize to
  // avoid synchronization issues (like removing the next order).
  [sharedDefaults synchronize];

  if (!commandDictionary) {
    return nil;
  }

  NSString* commandCallerPreference =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandAppPreference);
  NSString* commandCaller = base::mac::ObjCCast<NSString>(
      [commandDictionary objectForKey:commandCallerPreference]);

  NSString* commandPreference = base::SysUTF8ToNSString(
      app_group::kChromeAppGroupCommandCommandPreference);
  NSString* command = base::mac::ObjCCast<NSString>(
      [commandDictionary objectForKey:commandPreference]);

  NSString* commandTimePreference =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandTimePreference);
  id commandTime = base::mac::ObjCCast<NSDate>(
      [commandDictionary objectForKey:commandTimePreference]);

  NSString* commandTextPreference =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandTextPreference);
  NSString* externalText = base::mac::ObjCCast<NSString>(
      [commandDictionary objectForKey:commandTextPreference]);

  NSString* commandDataPreference =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandDataPreference);
  NSData* externalData = base::mac::ObjCCast<NSData>(
      [commandDictionary objectForKey:commandDataPreference]);

  NSString* commandIndexPreference =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandIndexPreference);
  NSNumber* index = base::mac::ObjCCast<NSNumber>(
      [commandDictionary objectForKey:commandIndexPreference]);

  if (!commandCaller || !command || !commandTimePreference) {
    return nil;
  }

  // Check the time of the last request to avoid app from intercepting old
  // open url request and replay it later.
  NSTimeInterval delay = [[NSDate date] timeIntervalSinceDate:commandTime];
  UMA_HISTOGRAM_COUNTS_100(kApplicationGroupCommandDelay, delay);
  if (delay > kAppGroupTriggersVoiceSearchTimeout)
    return nil;
  return [ChromeAppStartupParameters
      newAppStartupParametersForCommand:command
                       withExternalText:externalText
                       withExternalData:externalData
                              withIndex:index
                                withURL:url
                  fromSourceApplication:appId
            fromSecureSourceApplication:commandCaller];
}

+ (instancetype)newAppStartupParametersForCommand:(NSString*)command
                                 withExternalText:(NSString*)externalText
                                 withExternalData:(NSData*)externalData
                                        withIndex:(NSNumber*)index
                                          withURL:(NSURL*)url
                            fromSourceApplication:(NSString*)appId
                      fromSecureSourceApplication:(NSString*)secureSourceApp {
  SearchExtensionAction action = ACTION_NO_ACTION;
  ChromeAppStartupParameters* params = nil;

  if ([command
          isEqualToString:base::SysUTF8ToNSString(
                              app_group::kChromeAppGroupVoiceSearchCommand)]) {
    params = [[ChromeAppStartupParameters alloc]
        initWithExternalURL:GURL(kChromeUINewTabURL)
          declaredSourceApp:appId
            secureSourceApp:secureSourceApp
                completeURL:url];
    [params setPostOpeningAction:START_VOICE_SEARCH];
    action = ACTION_NEW_VOICE_SEARCH;
  }

  if ([command isEqualToString:base::SysUTF8ToNSString(
                                   app_group::kChromeAppGroupNewTabCommand)]) {
    params = [[ChromeAppStartupParameters alloc]
        initWithExternalURL:GURL(kChromeUINewTabURL)
          declaredSourceApp:appId
            secureSourceApp:secureSourceApp
                completeURL:url];
    action = ACTION_NO_ACTION;
  }

  if ([command
          isEqualToString:base::SysUTF8ToNSString(
                              app_group::kChromeAppGroupFocusOmniboxCommand)]) {
    params = [[ChromeAppStartupParameters alloc]
        initWithExternalURL:GURL(kChromeUINewTabURL)
          declaredSourceApp:appId
            secureSourceApp:secureSourceApp
                completeURL:url];
    [params setPostOpeningAction:FOCUS_OMNIBOX];
    action = ACTION_NEW_SEARCH;
  }

  if ([command isEqualToString:base::SysUTF8ToNSString(
                                   app_group::kChromeAppGroupOpenURLCommand)]) {
    if (!externalText || ![externalText isKindOfClass:[NSString class]])
      return nil;
    GURL externalGURL(base::SysNSStringToUTF8(externalText));
    if (!externalGURL.is_valid() || !externalGURL.SchemeIsHTTPOrHTTPS())
      return nil;
    params =
        [[ChromeAppStartupParameters alloc] initWithExternalURL:externalGURL
                                              declaredSourceApp:appId
                                                secureSourceApp:secureSourceApp
                                                    completeURL:url];
    action = ACTION_OPEN_URL;
  }

  if ([command
          isEqualToString:base::SysUTF8ToNSString(
                              app_group::kChromeAppGroupSearchTextCommand)]) {
    if (!externalText) {
      return nil;
    }

    params = [[ChromeAppStartupParameters alloc]
        initWithExternalURL:GURL(kChromeUINewTabURL)
          declaredSourceApp:appId
            secureSourceApp:secureSourceApp
                completeURL:url];

    params.textQuery = externalText;

    action = ACTION_SEARCH_TEXT;
  }

  if ([command
          isEqualToString:base::SysUTF8ToNSString(
                              app_group::kChromeAppGroupSearchImageCommand)]) {
    if (!externalData) {
      return nil;
    }

    params = [[ChromeAppStartupParameters alloc]
        initWithExternalURL:GURL(kChromeUINewTabURL)
          declaredSourceApp:appId
            secureSourceApp:secureSourceApp
                completeURL:url];

    params.imageSearchData = externalData;

    action = ACTION_SEARCH_IMAGE;
  }

  if ([command
          isEqualToString:base::SysUTF8ToNSString(
                              app_group::kChromeAppGroupQRScannerCommand)]) {
    params = [[ChromeAppStartupParameters alloc]
        initWithExternalURL:GURL(kChromeUINewTabURL)
          declaredSourceApp:appId
            secureSourceApp:secureSourceApp
                completeURL:url];
    [params setPostOpeningAction:START_QR_CODE_SCANNER];
    action = ACTION_NEW_QR_CODE_SEARCH;
  }

  if ([command isEqualToString:
                   base::SysUTF8ToNSString(
                       app_group::kChromeAppGroupIncognitoSearchCommand)]) {
    params = [[ChromeAppStartupParameters alloc]
        initWithExternalURL:GURL(kChromeUINewTabURL)
          declaredSourceApp:appId
            secureSourceApp:secureSourceApp
                completeURL:url];
    [params setLaunchInIncognito:YES];
    [params setPostOpeningAction:FOCUS_OMNIBOX];
    action = ACTION_NEW_INCOGNITO_SEARCH;
  }

  if ([secureSourceApp
          isEqualToString:app_group::kOpenCommandSourceSearchExtension]) {
    UMA_HISTOGRAM_ENUMERATION("IOS.SearchExtension.Action", action,
                              SEARCH_EXTENSION_ACTION_COUNT);
  }
  if ([secureSourceApp
          isEqualToString:app_group::kOpenCommandSourceContentExtension] &&
      index) {
    UMA_HISTOGRAM_COUNTS_100("IOS.ContentExtension.Index",
                             [index integerValue]);
  }
  return params;
}

- (MobileSessionCallerApp)callerApp {
  if ([_secureSourceApp
          isEqualToString:app_group::kOpenCommandSourceTodayExtension])
    return CALLER_APP_GOOGLE_CHROME_TODAY_EXTENSION;
  if ([_secureSourceApp
          isEqualToString:app_group::kOpenCommandSourceSearchExtension])
    return CALLER_APP_GOOGLE_CHROME_SEARCH_EXTENSION;
  if ([_secureSourceApp
          isEqualToString:app_group::kOpenCommandSourceContentExtension])
    return CALLER_APP_GOOGLE_CHROME_CONTENT_EXTENSION;
  if ([_secureSourceApp
          isEqualToString:app_group::kOpenCommandSourceShareExtension])
    return CALLER_APP_GOOGLE_CHROME_SHARE_EXTENSION;

  if (![_declaredSourceApp length])
    return CALLER_APP_NOT_AVAILABLE;
  if ([_declaredSourceApp
          isEqualToString:[[NSBundle mainBundle] bundleIdentifier]])
    return CALLER_APP_GOOGLE_CHROME;
  if ([_declaredSourceApp isEqualToString:@"com.google.GoogleMobile"])
    return CALLER_APP_GOOGLE_SEARCH;
  if ([_declaredSourceApp isEqualToString:@"com.google.Gmail"])
    return CALLER_APP_GOOGLE_GMAIL;
  if ([_declaredSourceApp isEqualToString:@"com.google.GooglePlus"])
    return CALLER_APP_GOOGLE_PLUS;
  if ([_declaredSourceApp isEqualToString:@"com.google.Drive"])
    return CALLER_APP_GOOGLE_DRIVE;
  if ([_declaredSourceApp isEqualToString:@"com.google.b612"])
    return CALLER_APP_GOOGLE_EARTH;
  if ([_declaredSourceApp isEqualToString:@"com.google.ios.youtube"])
    return CALLER_APP_GOOGLE_YOUTUBE;
  if ([_declaredSourceApp isEqualToString:@"com.google.Maps"])
    return CALLER_APP_GOOGLE_MAPS;
  if ([_declaredSourceApp hasPrefix:@"com.google."])
    return CALLER_APP_GOOGLE_OTHER;
  if ([_declaredSourceApp isEqualToString:@"com.apple.mobilesafari"])
    return CALLER_APP_APPLE_MOBILESAFARI;
  if ([_declaredSourceApp hasPrefix:@"com.apple."])
    return CALLER_APP_APPLE_OTHER;

  return CALLER_APP_OTHER;
}

- (first_run::ExternalLaunch)launchSource {
  if ([self callerApp] != CALLER_APP_APPLE_MOBILESAFARI) {
    return first_run::LAUNCH_BY_OTHERS;
  }

  NSString* query = base::SysUTF8ToNSString(self.completeURL.query());
  // Takes care of degenerated case of no QUERY_STRING.
  if (![query length])
    return first_run::LAUNCH_BY_MOBILESAFARI;
  // Look for |kSmartAppBannerKey| anywhere within the query string.
  NSRange found = [query rangeOfString:kSmartAppBannerKey];
  if (found.location == NSNotFound)
    return first_run::LAUNCH_BY_MOBILESAFARI;
  // |kSmartAppBannerKey| can be at the beginning or end of the query
  // string and may also be optionally followed by a equal sign and a value.
  // For now, just look for the presence of the key and ignore the value.
  if (found.location + found.length < [query length]) {
    // There are characters following the found location.
    unichar charAfter =
        [query characterAtIndex:(found.location + found.length)];
    if (charAfter != '&' && charAfter != '=')
      return first_run::LAUNCH_BY_MOBILESAFARI;
  }
  if (found.location > 0) {
    unichar charBefore = [query characterAtIndex:(found.location - 1)];
    if (charBefore != '&')
      return first_run::LAUNCH_BY_MOBILESAFARI;
  }
  return first_run::LAUNCH_BY_SMARTAPPBANNER;
}

@end
