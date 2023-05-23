// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/startup/chrome_app_startup_parameters.h"

#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/default_browser/utils.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/x_callback_url.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "net/base/mac/url_conversions.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UmaHistogramEnumeration;

namespace {

// Key of the UMA Startup.MobileSessionStartAction histogram.
const char kUMAMobileSessionStartActionHistogram[] =
    "Startup.MobileSessionStartAction";

const char kApplicationGroupCommandDelay[] =
    "Startup.ApplicationGroupCommandDelay";

// URL Query String parameter to indicate that this openURL: request arrived
// here due to a Smart App Banner presentation on a Google.com page.
NSString* const kSmartAppBannerKey = @"safarisab";

// TODO(crbug.com/1138702): When swift is supported move WidgetKit constants to
// a file where they can be shared with the extension. Currently these are also
// declared as URLs in ios/c/widget_kit_extension/widget_constants.swift.
//
// Scheme used by the widget extension actions. It's important that this scheme
// is never defined as Custom URL Scheme for Chrome so only the widgets can use
// the actions on it.
NSString* const kWidgetKitSchemeChrome = @"chromewidgetkit";
// Host used to identify Search (small) widget.
NSString* const kWidgetKitHostSearchWidget = @"search-widget";
// Host used to identify Quick Actions (medium) widget.
NSString* const kWidgetKitHostQuickActionsWidget = @"quick-actions-widget";
// Host used to identify Dino Game (small) widget.
NSString* const kWidgetKitHostDinoGameWidget = @"dino-game-widget";
// Host used to identify the Lockscreen Launcher widget.
NSString* const kWidgetKitHostLockscreenLauncherWidget =
    @"lockscreen-launcher-widget";
// Host used to identify the Chrome Shortcuts widget.
NSString* const kWidgetKitHostShortcutsWidget = @"shortcuts-widget";
// Path for search action.
NSString* const kWidgetKitActionSearch = @"/search";
// Path for incognito action.
NSString* const kWidgetKitActionIncognito = @"/incognito";
// Path for Voice Search action.
NSString* const kWidgetKitActionVoiceSearch = @"/voicesearch";
// Path for QR Reader action.
NSString* const kWidgetKitActionQRReader = @"/qrreader";
// Path for Lens action.
NSString* const kWidgetKitActionLens = @"/lens";
// Path for Game action.
NSString* const kWidgetKitActionGame = @"/game";
// Path for open URL action.
NSString* const kWidgetKitActionOpenURL = @"/open";

const CGFloat kAppGroupTriggersVoiceSearchTimeout = 15.0;

// Values of the UMA Startup.MobileSessionStartAction histogram.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum MobileSessionStartAction {
  // Logged when an application passes an http URL to Chrome using the custom
  // registered scheme (f.e. googlechrome).
  START_ACTION_OPEN_HTTP = 0,
  // Logged when an application passes an https URL to Chrome using the custom
  // registered scheme (f.e. googlechromes).
  START_ACTION_OPEN_HTTPS = 1,
  START_ACTION_OPEN_FILE = 2,
  START_ACTION_XCALLBACK_OPEN = 3,
  START_ACTION_XCALLBACK_OTHER = 4,
  START_ACTION_OTHER = 5,
  START_ACTION_XCALLBACK_APPGROUP_COMMAND = 6,
  // Logged when any application passes an http URL to Chrome using the standard
  // "http" scheme. This can happen when Chrome is set as the default browser
  // on iOS 14+ as http openURL calls will be directed to Chrome by the system
  // from all other apps.
  START_ACTION_OPEN_HTTP_FROM_OS = 7,
  // Logged when any application passes an https URL to Chrome using the
  // standard "https" scheme. This can happen when Chrome is set as the default
  // browser on iOS 14+ as http openURL calls will be directed to Chrome by the
  // system from all other apps.
  START_ACTION_OPEN_HTTPS_FROM_OS = 8,
  START_ACTION_WIDGET_KIT_COMMAND = 9,
  MOBILE_SESSION_START_ACTION_COUNT
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
  ACTION_LENS,
  SEARCH_EXTENSION_ACTION_COUNT,
};

// Values of the UMA IOS.WidgetKit.Action histogram.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class WidgetKitExtensionAction {
  ACTION_DINO_WIDGET_GAME = 0,
  ACTION_SEARCH_WIDGET_SEARCH = 1,
  ACTION_QUICK_ACTIONS_SEARCH = 2,
  ACTION_QUICK_ACTIONS_INCOGNITO = 3,
  ACTION_QUICK_ACTIONS_VOICE_SEARCH = 4,
  ACTION_QUICK_ACTIONS_QR_READER = 5,
  ACTION_LOCKSCREEN_LAUNCHER_SEARCH = 6,
  ACTION_LOCKSCREEN_LAUNCHER_INCOGNITO = 7,
  ACTION_LOCKSCREEN_LAUNCHER_VOICE_SEARCH = 8,
  ACTION_LOCKSCREEN_LAUNCHER_GAME = 9,
  ACTION_QUICK_ACTIONS_LENS = 10,
  ACTION_SHORTCUTS_SEARCH = 11,
  ACTION_SHORTCUTS_OPEN = 12,
  kMaxValue = ACTION_SHORTCUTS_OPEN,
};

// Histogram helper to log the UMA IOS.WidgetKit.Action histogram.
void LogWidgetKitAction(WidgetKitExtensionAction action) {
  UmaHistogramEnumeration("IOS.WidgetKit.Action", action);
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeMadeForIOS);
}

bool CallerAppIsFirstParty(MobileSessionCallerApp callerApp) {
  switch (callerApp) {
    case CALLER_APP_GOOGLE_SEARCH:
    case CALLER_APP_GOOGLE_GMAIL:
    case CALLER_APP_GOOGLE_PLUS:
    case CALLER_APP_GOOGLE_DRIVE:
    case CALLER_APP_GOOGLE_EARTH:
    case CALLER_APP_GOOGLE_OTHER:
    case CALLER_APP_GOOGLE_YOUTUBE:
    case CALLER_APP_GOOGLE_MAPS:
    case CALLER_APP_GOOGLE_CHROME_TODAY_EXTENSION:
    case CALLER_APP_GOOGLE_CHROME_SEARCH_EXTENSION:
    case CALLER_APP_GOOGLE_CHROME_CONTENT_EXTENSION:
    case CALLER_APP_GOOGLE_CHROME_SHARE_EXTENSION:
    case CALLER_APP_GOOGLE_CHROME:
      return true;
    case CALLER_APP_OTHER:
    case CALLER_APP_APPLE_MOBILESAFARI:
    case CALLER_APP_APPLE_OTHER:
    case CALLER_APP_THIRD_PARTY:
    case CALLER_APP_NOT_AVAILABLE:
    case MOBILE_SESSION_CALLER_APP_COUNT:
      return false;
  }
}

TabOpeningPostOpeningAction XCallbackPoaToPostOpeningAction(
    const std::string& poa_param) {
  if (poa_param == "default-browser-settings") {
    return SHOW_DEFAULT_BROWSER_SETTINGS;
  }
  return NO_ACTION;
}

}  // namespace

@implementation ChromeAppStartupParameters {
  NSString* _secureSourceApp;
  NSString* _declaredSourceApp;
}

- (instancetype)initWithExternalURL:(const GURL&)externalURL
                  declaredSourceApp:(NSString*)declaredSourceApp
                    secureSourceApp:(NSString*)secureSourceApp
                        completeURL:(NSURL*)completeURL
                    applicationMode:(ApplicationModeForTabOpening)mode {
  self = [super initWithExternalURL:externalURL
                        completeURL:net::GURLWithNSURL(completeURL)
                    applicationMode:mode];
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

  if ([completeURL.scheme isEqualToString:kWidgetKitSchemeChrome]) {
    UMA_HISTOGRAM_ENUMERATION(kUMAMobileSessionStartActionHistogram,
                              START_ACTION_WIDGET_KIT_COMMAND,
                              MOBILE_SESSION_START_ACTION_COUNT);

    const char* command = "";
    NSString* sourceWidget = completeURL.host;

    if ([completeURL.path isEqualToString:kWidgetKitActionSearch]) {
      command = app_group::kChromeAppGroupFocusOmniboxCommand;
    } else if ([completeURL.path isEqualToString:kWidgetKitActionIncognito]) {
      command = app_group::kChromeAppGroupIncognitoSearchCommand;
    } else if ([completeURL.path isEqualToString:kWidgetKitActionVoiceSearch]) {
      command = app_group::kChromeAppGroupVoiceSearchCommand;
    } else if ([completeURL.path isEqualToString:kWidgetKitActionQRReader]) {
      command = app_group::kChromeAppGroupQRScannerCommand;
    } else if ([completeURL.path isEqualToString:kWidgetKitActionLens]) {
      command = app_group::kChromeAppGroupLensCommand;
    } else if ([completeURL.path isEqual:kWidgetKitActionOpenURL]) {
      command = app_group::kChromeAppGroupOpenURLCommand;
    } else if ([completeURL.path isEqualToString:kWidgetKitActionGame]) {
      if ([sourceWidget isEqualToString:kWidgetKitHostDinoGameWidget]) {
        LogWidgetKitAction(WidgetKitExtensionAction::ACTION_DINO_WIDGET_GAME);
      } else if ([sourceWidget
                     isEqualToString:kWidgetKitHostLockscreenLauncherWidget]) {
        LogWidgetKitAction(
            WidgetKitExtensionAction::ACTION_LOCKSCREEN_LAUNCHER_GAME);
      }

      LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeGeneral);

      GURL URL(
          base::StringPrintf("%s://%s", kChromeUIScheme, kChromeUIDinoHost));
      ChromeAppStartupParameters* appStartupParameters =
          [[ChromeAppStartupParameters alloc]
              initWithExternalURL:URL
                declaredSourceApp:appId
                  secureSourceApp:sourceWidget
                      completeURL:completeURL
                  applicationMode:ApplicationModeForTabOpening::NORMAL];
      return appStartupParameters;
    }

    NSString* commandString = base::SysUTF8ToNSString(command);
    return [self newAppStartupParametersForCommand:commandString
                                  withExternalText:nil
                                  withExternalData:nil
                                         withIndex:0
                                           withURL:nil
                             fromSourceApplication:appId
                       fromSecureSourceApplication:sourceWidget];

  } else if (IsXCallbackURL(gurl)) {
    // TODO(crbug.com/228098): Temporary fix.
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
    TabOpeningPostOpeningAction postOpeningAction =
        XCallbackPoaToPostOpeningAction(parameters["poa"]);

    ChromeAppStartupParameters* startupParameters =
        [[ChromeAppStartupParameters alloc]
            initWithExternalURL:url
              declaredSourceApp:appId
                secureSourceApp:nil
                    completeURL:completeURL
                applicationMode:ApplicationModeForTabOpening::UNDETERMINED];
    // postOpeningAction can only be NO_ACTION or SHOW_DEFAULT_BROWSER_SETTINGS
    // (these are the only values returned by `XCallbackPoaToPostOpeningAction`)
    // so this assignment should not DCHECK, no matter what the URL is.
    startupParameters.postOpeningAction = postOpeningAction;
    return startupParameters;
  } else if (gurl.SchemeIsFile()) {
    UMA_HISTOGRAM_ENUMERATION(kUMAMobileSessionStartActionHistogram,
                              START_ACTION_OPEN_FILE,
                              MOBILE_SESSION_START_ACTION_COUNT);
    // `url` is the path to a file received from another application.
    GURL::Replacements replacements;
    const std::string host(kChromeUIExternalFileHost);
    std::string filename = gurl.ExtractFileName();
    replacements.SetPathStr(filename);
    replacements.SetSchemeStr(kChromeUIScheme);
    replacements.SetHostStr(host);
    GURL externalURL = gurl.ReplaceComponents(replacements);
    if (!externalURL.is_valid())
      return nil;
    return [[ChromeAppStartupParameters alloc]
        initWithExternalURL:externalURL
          declaredSourceApp:appId
            secureSourceApp:nil
                completeURL:completeURL
            applicationMode:ApplicationModeForTabOpening::NORMAL];
  } else {
    GURL externalURL = gurl;
    BOOL openedViaSpecificScheme = NO;
    MobileSessionStartAction action = START_ACTION_OTHER;
    if (gurl.SchemeIs(url::kHttpScheme)) {
      action = START_ACTION_OPEN_HTTP_FROM_OS;
      base::RecordAction(
          base::UserMetricsAction("MobileDefaultBrowserViewIntent"));
    } else if (gurl.SchemeIs(url::kHttpsScheme)) {
      action = START_ACTION_OPEN_HTTPS_FROM_OS;
      base::RecordAction(
          base::UserMetricsAction("MobileDefaultBrowserViewIntent"));
    } else {
      // Replace the scheme with https or http depending on whether the input
      // `url` scheme ends with an 's'.
      BOOL useHttps = gurl.scheme()[gurl.scheme().length() - 1] == 's';
      action = useHttps ? START_ACTION_OPEN_HTTPS : START_ACTION_OPEN_HTTP;
      base::RecordAction(base::UserMetricsAction("MobileFirstPartyViewIntent"));

      GURL::Replacements replace_scheme;
      if (useHttps)
        replace_scheme.SetSchemeStr(url::kHttpsScheme);
      else
        replace_scheme.SetSchemeStr(url::kHttpScheme);
      externalURL = gurl.ReplaceComponents(replace_scheme);
      openedViaSpecificScheme = YES;
    }
    UMA_HISTOGRAM_ENUMERATION(kUMAMobileSessionStartActionHistogram, action,
                              MOBILE_SESSION_START_ACTION_COUNT);
    // An HTTP(S) URL open that opened Chrome (e.g. default browser open) should
    // be logged as siginficnat activity for a potential user that would want
    // Chrome as their default browser in case the user changes away from
    // Chrome. This will leave a trace of this activity for re-prompting.
    LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeGeneral);

    if (action == START_ACTION_OPEN_HTTP_FROM_OS ||
        action == START_ACTION_OPEN_HTTPS_FROM_OS) {
      LogOpenHTTPURLFromExternalURL();
    }

    if (!externalURL.is_valid())
      return nil;
    ChromeAppStartupParameters* params = [[ChromeAppStartupParameters alloc]
        initWithExternalURL:externalURL
          declaredSourceApp:appId
            secureSourceApp:nil
                completeURL:completeURL
            applicationMode:ApplicationModeForTabOpening::UNDETERMINED];
    params.openedViaFirstPartyScheme =
        openedViaSpecificScheme && CallerAppIsFirstParty(params.callerApp);
    return params;
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

  // `sharedDefaults` is used for communication between apps. Synchronize to
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
                completeURL:url
            applicationMode:ApplicationModeForTabOpening::NORMAL];
    [params setPostOpeningAction:START_VOICE_SEARCH];
    action = ACTION_NEW_VOICE_SEARCH;
  }

  if ([command isEqualToString:base::SysUTF8ToNSString(
                                   app_group::kChromeAppGroupNewTabCommand)]) {
    params = [[ChromeAppStartupParameters alloc]
        initWithExternalURL:GURL(kChromeUINewTabURL)
          declaredSourceApp:appId
            secureSourceApp:secureSourceApp
                completeURL:url
            applicationMode:ApplicationModeForTabOpening::NORMAL];
    action = ACTION_NO_ACTION;
  }

  if ([command
          isEqualToString:base::SysUTF8ToNSString(
                              app_group::kChromeAppGroupFocusOmniboxCommand)]) {
    params = [[ChromeAppStartupParameters alloc]
        initWithExternalURL:GURL(kChromeUINewTabURL)
          declaredSourceApp:appId
            secureSourceApp:secureSourceApp
                completeURL:url
            applicationMode:ApplicationModeForTabOpening::NORMAL];
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
    params = [[ChromeAppStartupParameters alloc]
        initWithExternalURL:externalGURL
          declaredSourceApp:appId
            secureSourceApp:secureSourceApp
                completeURL:url
            applicationMode:ApplicationModeForTabOpening::UNDETERMINED];
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
                completeURL:url
            applicationMode:ApplicationModeForTabOpening::UNDETERMINED];

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
                completeURL:url
            applicationMode:ApplicationModeForTabOpening::UNDETERMINED];

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
                completeURL:url
            applicationMode:ApplicationModeForTabOpening::NORMAL];
    [params setPostOpeningAction:START_QR_CODE_SCANNER];

    action = ACTION_NEW_QR_CODE_SEARCH;
  }

  if ([command isEqualToString:base::SysUTF8ToNSString(
                                   app_group::kChromeAppGroupLensCommand)]) {
    params = [[ChromeAppStartupParameters alloc]
        initWithExternalURL:GURL(kChromeUINewTabURL)
          declaredSourceApp:appId
            secureSourceApp:secureSourceApp
                completeURL:url
            applicationMode:ApplicationModeForTabOpening::NORMAL];
    [params setPostOpeningAction:START_LENS];
    action = ACTION_LENS;
  }

  if ([command isEqualToString:
                   base::SysUTF8ToNSString(
                       app_group::kChromeAppGroupIncognitoSearchCommand)]) {
    params = [[ChromeAppStartupParameters alloc]
        initWithExternalURL:GURL(kChromeUINewTabURL)
          declaredSourceApp:appId
            secureSourceApp:secureSourceApp
                completeURL:url
            applicationMode:ApplicationModeForTabOpening::INCOGNITO];
    [params setPostOpeningAction:FOCUS_OMNIBOX];
    action = ACTION_NEW_INCOGNITO_SEARCH;
  }

  if (action != ACTION_NO_ACTION) {
    // An external action that opened Chrome (i.e. GrowthKit link open, open
    // Search, search clipboard content) is activity that should indicate a user
    // that would be interested in setting Chrome as the default browser.
    LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeGeneral);
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
  if ([secureSourceApp isEqualToString:kWidgetKitHostSearchWidget]) {
    LogWidgetKitAction(WidgetKitExtensionAction::ACTION_SEARCH_WIDGET_SEARCH);
  }
  if ([secureSourceApp isEqualToString:kWidgetKitHostQuickActionsWidget]) {
    switch (action) {
      case ACTION_NEW_VOICE_SEARCH:
        LogWidgetKitAction(
            WidgetKitExtensionAction::ACTION_QUICK_ACTIONS_VOICE_SEARCH);
        break;

      case ACTION_NEW_SEARCH:
        LogWidgetKitAction(
            WidgetKitExtensionAction::ACTION_QUICK_ACTIONS_SEARCH);
        break;

      case ACTION_NEW_QR_CODE_SEARCH:
        LogWidgetKitAction(
            WidgetKitExtensionAction::ACTION_QUICK_ACTIONS_QR_READER);
        break;

      case ACTION_LENS:
        LogWidgetKitAction(WidgetKitExtensionAction::ACTION_QUICK_ACTIONS_LENS);
        break;

      case ACTION_NEW_INCOGNITO_SEARCH:
        LogWidgetKitAction(
            WidgetKitExtensionAction::ACTION_QUICK_ACTIONS_INCOGNITO);
        break;

      default:
        NOTREACHED();
        break;
    }
  }
  if ([secureSourceApp
          isEqualToString:kWidgetKitHostLockscreenLauncherWidget]) {
    switch (action) {
      case ACTION_NEW_SEARCH:
        LogWidgetKitAction(
            WidgetKitExtensionAction::ACTION_LOCKSCREEN_LAUNCHER_SEARCH);
        break;

      case ACTION_NEW_INCOGNITO_SEARCH:
        LogWidgetKitAction(
            WidgetKitExtensionAction::ACTION_LOCKSCREEN_LAUNCHER_INCOGNITO);
        break;

      case ACTION_NEW_VOICE_SEARCH:
        LogWidgetKitAction(
            WidgetKitExtensionAction::ACTION_LOCKSCREEN_LAUNCHER_VOICE_SEARCH);
        break;

      default:
        NOTREACHED();
        break;
    }
  }
  if ([secureSourceApp isEqualToString:kWidgetKitHostShortcutsWidget]) {
    switch (action) {
      case ACTION_NEW_SEARCH:
        LogWidgetKitAction(WidgetKitExtensionAction::ACTION_SHORTCUTS_SEARCH);
        break;
      case ACTION_OPEN_URL:
        LogWidgetKitAction(WidgetKitExtensionAction::ACTION_SHORTCUTS_OPEN);
        break;
      default:
        NOTREACHED();
        break;
    }
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

  if (![_declaredSourceApp length]) {
    if (self.completeURL.SchemeIs(url::kHttpScheme) ||
        self.completeURL.SchemeIs(url::kHttpsScheme)) {
      // If Chrome is opened via the system default browser mechanism, the
      // action should be differentiated from the case where the source is
      // unknown.
      return CALLER_APP_THIRD_PARTY;
    }
    return CALLER_APP_NOT_AVAILABLE;
  }

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
  // Look for `kSmartAppBannerKey` anywhere within the query string.
  NSRange found = [query rangeOfString:kSmartAppBannerKey];
  if (found.location == NSNotFound)
    return first_run::LAUNCH_BY_MOBILESAFARI;
  // `kSmartAppBannerKey` can be at the beginning or end of the query
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
