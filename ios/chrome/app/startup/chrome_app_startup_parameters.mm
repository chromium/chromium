// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/startup/chrome_app_startup_parameters.h"

#import "base/apple/bundle_locations.h"
#import "base/apple/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/manage_passwords_referrer.h"
#import "ios/chrome/app/startup/app_launch_metrics.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/x_callback_url.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "net/base/apple/url_conversions.h"
#import "net/base/url_util.h"
#import "url/gurl.h"

using base::UmaHistogramEnumeration;

namespace {

// Key of the UMA Startup.MobileSessionStartAction histogram.
const char kUMAMobileSessionStartActionHistogram[] =
    "Startup.MobileSessionStartAction";

const char kApplicationGroupCommandDelay[] =
    "Startup.ApplicationGroupCommandDelay";

// UMA histogram key for IOS.ExternalAction.
const char kExternalActionHistogram[] = "IOS.ExternalAction";

// Host string used to detect an "external action" scheme URL.
NSString* const kExternalActionURLHost = @"ChromeExternalAction";

// Action path string for launching the default browser settings using external
// actions.
NSString* const kExternalActionDefaultBrowserSettings =
    @"DefaultBrowserSettings";

// Action path string for Opening an NTP using external actions.
NSString* const kExternalActionOpenNTP = @"OpenNTP";

// URL Query String parameter to indicate that this openURL: request arrived
// here due to a Smart App Banner presentation on a Google.com page.
NSString* const kSmartAppBannerKey = @"safarisab";

// TODO(crbug.com/40725595): When swift is supported move WidgetKit constants to
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
// Host used to identify the Search Passwords widget.
NSString* const kWidgetKitHostSearchPasswordsWidget =
    @"search-passwords-widget";
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
// Path for search passwords action.
NSString* const kWidgetKitActionSearchPasswords = @"/search-passwords";

const CGFloat kAppGroupTriggersVoiceSearchTimeout = 15.0;

// Values of the UMA Startup.MobileSessionStartAction histogram.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange
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
  // Logged when Chrome is opened via the external action scheme.
  START_EXTERNAL_ACTION = 10,
  MOBILE_SESSION_START_ACTION_COUNT
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml)

// Values of the UMA iOS.SearchExtension.Action histogram.
// LINT.IfChange
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
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml)

// Values of the UMA IOS.WidgetKit.Action histogram.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange
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
  ACTION_SEARCH_PASSWORDS_WIDGET_SEARCH_PASSWORDS = 13,
  kMaxValue = ACTION_SEARCH_PASSWORDS_WIDGET_SEARCH_PASSWORDS,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml)

// Values of the UMA IOS.ExternalAction histogram.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange
enum class IOSExternalAction {
  // Logged when Chrome is passed an invalid action.
  ACTION_INVALID = 0,
  // Logged when Chrome is passed a "OpenNTP" action.
  ACTION_OPEN_NTP = 1,
  // Logged when Chrome is passed a "DefaultBrowserSettings" action.
  ACTION_DEFAULT_BROWSER_SETTINGS = 2,
  // Logged when Chrome is passed a "DefaultBrowserSettings" action, but instead
  // will show the NTP, since Chrome is already set as default browser.
  ACTION_SKIPPED_DEFAULT_BROWSER_SETTINGS_FOR_NTP = 3,
  kMaxValue = ACTION_SKIPPED_DEFAULT_BROWSER_SETTINGS_FOR_NTP,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml)

// Histogram helper to log the UMA IOS.WidgetKit.Action histogram.
void LogWidgetKitAction(WidgetKitExtensionAction action) {
  UmaHistogramEnumeration("IOS.WidgetKit.Action", action);
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
    case CALLER_APP_GOOGLE_CHROME_OPEN_EXTENSION:
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

+ (instancetype)startupParametersWithURL:(NSURL*)completeURL
                       sourceApplication:(NSString*)appID {
  GURL parsedURL = net::GURLWithNSURL(completeURL);

  if (!parsedURL.is_valid() || parsedURL.scheme().length() == 0) {
    return nil;
  }

  // Log browser started indirectly for default browser promo experiment stats.
  LogBrowserIndirectlylaunched();

  if ([completeURL.scheme isEqualToString:kWidgetKitSchemeChrome]) {
    UMA_HISTOGRAM_ENUMERATION(kUMAMobileSessionStartActionHistogram,
                              START_ACTION_WIDGET_KIT_COMMAND,
                              MOBILE_SESSION_START_ACTION_COUNT);

    base::UmaHistogramEnumeration(kAppLaunchSource, AppLaunchSource::WIDGET);

    const char* command = "";
    NSString* sourceWidget = completeURL.host;
    NSString* externalText = nil;

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
      std::string URLQueryParam;
      if (net::GetValueForKeyInQuery(net::GURLWithNSURL(completeURL), "url",
                                     &URLQueryParam)) {
        externalText = base::SysUTF8ToNSString(URLQueryParam);
      }
    } else if ([completeURL.path isEqual:kWidgetKitActionSearchPasswords]) {
      command = app_group::kChromeAppGroupSearchPasswordsCommand;
    } else if ([completeURL.path isEqualToString:kWidgetKitActionGame]) {
      if ([sourceWidget isEqualToString:kWidgetKitHostDinoGameWidget]) {
        LogWidgetKitAction(WidgetKitExtensionAction::ACTION_DINO_WIDGET_GAME);
      } else if ([sourceWidget
                     isEqualToString:kWidgetKitHostLockscreenLauncherWidget]) {
        LogWidgetKitAction(
            WidgetKitExtensionAction::ACTION_LOCKSCREEN_LAUNCHER_GAME);
      }

      GURL URL(
          base::StringPrintf("%s://%s", kChromeUIScheme, kChromeUIDinoHost));
      ChromeAppStartupParameters* appStartupParameters =
          [[ChromeAppStartupParameters alloc]
              initWithExternalURL:URL
                declaredSourceApp:appID
                  secureSourceApp:sourceWidget
                      completeURL:completeURL
                  applicationMode:ApplicationModeForTabOpening::NORMAL];
      appStartupParameters.openedViaWidgetScheme = YES;
      return appStartupParameters;
    }

    NSString* commandString = base::SysUTF8ToNSString(command);
    ChromeAppStartupParameters* appStartupParameters =
        [self startupParametersForCommand:commandString
                         withExternalText:externalText
                             externalData:nil
                                    index:0
                                      URL:nil
                        sourceApplication:appID
                  secureSourceApplication:sourceWidget];
    appStartupParameters.openedViaWidgetScheme = YES;
    return appStartupParameters;

  } else if (IsXCallbackURL(parsedURL)) {
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::X_CALLBACK);
    // TODO(crbug.com/41004788): Temporary fix.
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
          startupParametersForExtensionCommandWithURL:completeURL
                                    sourceApplication:appID];
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
        ExtractQueryParametersFromXCallbackURL(parsedURL);
    GURL URLQueryParam = GURL(parameters["url"]);
    if (!URLQueryParam.is_valid() ||
        (!URLQueryParam.SchemeIs(url::kHttpScheme) &&
         !URLQueryParam.SchemeIs(url::kHttpsScheme))) {
      return nil;
    }
    TabOpeningPostOpeningAction postOpeningAction =
        XCallbackPoaToPostOpeningAction(parameters["poa"]);

    ChromeAppStartupParameters* startupParameters =
        [[ChromeAppStartupParameters alloc]
            initWithExternalURL:URLQueryParam
              declaredSourceApp:appID
                secureSourceApp:nil
                    completeURL:completeURL
                applicationMode:ApplicationModeForTabOpening::UNDETERMINED];
    // postOpeningAction can only be NO_ACTION or SHOW_DEFAULT_BROWSER_SETTINGS
    // (these are the only values returned by `XCallbackPoaToPostOpeningAction`)
    // so this assignment should not DCHECK, no matter what the URL is.
    startupParameters.postOpeningAction = postOpeningAction;
    return startupParameters;
  } else if ([self isChromeExternalActionURL:completeURL]) {
    base::RecordAction(
        base::UserMetricsAction("MobileExternalActionURLOpened"));
    UMA_HISTOGRAM_ENUMERATION(kUMAMobileSessionStartActionHistogram,
                              START_EXTERNAL_ACTION,
                              MOBILE_SESSION_START_ACTION_COUNT);
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::EXTERNAL_ACTION);

    return [self startupParametersForExternalActionWithAppID:appID
                                                 completeURL:completeURL];
  } else if (parsedURL.SchemeIsFile()) {
    UMA_HISTOGRAM_ENUMERATION(kUMAMobileSessionStartActionHistogram,
                              START_ACTION_OPEN_FILE,
                              MOBILE_SESSION_START_ACTION_COUNT);
    // `url` is the path to a file received from another application.
    GURL::Replacements replacements;
    const std::string host(kChromeUIExternalFileHost);
    std::string filename = parsedURL.ExtractFileName();
    replacements.SetPathStr(filename);
    replacements.SetSchemeStr(kChromeUIScheme);
    replacements.SetHostStr(host);
    GURL externalURL = parsedURL.ReplaceComponents(replacements);
    if (!externalURL.is_valid())
      return nil;
    return [[ChromeAppStartupParameters alloc]
        initWithExternalURL:externalURL
          declaredSourceApp:appID
            secureSourceApp:nil
                completeURL:completeURL
            applicationMode:ApplicationModeForTabOpening::NORMAL];
  } else {
    GURL externalURL = parsedURL;
    BOOL openedViaSpecificScheme = NO;
    MobileSessionStartAction action = START_ACTION_OTHER;
    if (parsedURL.SchemeIs(url::kHttpScheme)) {
      action = START_ACTION_OPEN_HTTP_FROM_OS;
      base::RecordAction(
          base::UserMetricsAction("MobileDefaultBrowserViewIntent"));
    } else if (parsedURL.SchemeIs(url::kHttpsScheme)) {
      action = START_ACTION_OPEN_HTTPS_FROM_OS;
      base::RecordAction(
          base::UserMetricsAction("MobileDefaultBrowserViewIntent"));
    } else {
      // Replace the scheme with https or http depending on whether the input
      // `url` scheme ends with an 's'.
      BOOL useHttps =
          parsedURL.scheme()[parsedURL.scheme().length() - 1] == 's';
      action = useHttps ? START_ACTION_OPEN_HTTPS : START_ACTION_OPEN_HTTP;
      base::UmaHistogramEnumeration(kAppLaunchSource,
                                    AppLaunchSource::LINK_OPENED_FROM_APP);
      base::RecordAction(base::UserMetricsAction("MobileFirstPartyViewIntent"));

      GURL::Replacements replaceScheme;
      if (useHttps)
        replaceScheme.SetSchemeStr(url::kHttpsScheme);
      else
        replaceScheme.SetSchemeStr(url::kHttpScheme);
      externalURL = parsedURL.ReplaceComponents(replaceScheme);
      openedViaSpecificScheme = YES;
    }
    UMA_HISTOGRAM_ENUMERATION(kUMAMobileSessionStartActionHistogram, action,
                              MOBILE_SESSION_START_ACTION_COUNT);

    if (action == START_ACTION_OPEN_HTTP_FROM_OS ||
        action == START_ACTION_OPEN_HTTPS_FROM_OS) {
      base::UmaHistogramEnumeration(kAppLaunchSource,
                                    AppLaunchSource::LINK_OPENED_FROM_OS);
      LogOpenHTTPURLFromExternalURL();
    }

    if (!externalURL.is_valid())
      return nil;
    ChromeAppStartupParameters* params = [[ChromeAppStartupParameters alloc]
        initWithExternalURL:externalURL
          declaredSourceApp:appID
            secureSourceApp:nil
                completeURL:completeURL
            applicationMode:ApplicationModeForTabOpening::UNDETERMINED];
    params.openedWithURL = YES;
    params.openedViaFirstPartyScheme =
        openedViaSpecificScheme && CallerAppIsFirstParty(params.callerApp);
    return params;
  }
}

// Returns true if the URL passed is an external action URL, defined by having
// the `kExternalActionURLHost` as host.
+ (BOOL)isChromeExternalActionURL:(NSURL*)URL {
  return [URL.host isEqualToString:kExternalActionURLHost];
}

// Returns a new `ChromeAppStartupParameters` for a given `appID`, `completeURL`
// and `externalURL`.
+ (ChromeAppStartupParameters*)
    startupParametersForExternalActionWithAppID:(NSString*)appID
                                    completeURL:(NSURL*)completeURL
                                    externalURL:(const GURL&)externalURL {
  return [[ChromeAppStartupParameters alloc]
      initWithExternalURL:externalURL
        declaredSourceApp:appID
          secureSourceApp:nil
              completeURL:completeURL
          applicationMode:ApplicationModeForTabOpening::UNDETERMINED];
}

// Returns the correct startup parameters for a given external action passed as
// path to the external action "scheme". Returns nil (no-op) if the action is
// not recognized.
+ (instancetype)startupParametersForExternalActionWithAppID:(NSString*)appID
                                                completeURL:
                                                    (NSURL*)completeURL {
  ChromeAppStartupParameters* params;
  IOSExternalAction action;
  NSString* path;

  // Separate the path into its components and ensure there is only one
  // component after the first "/".
  NSArray<NSString*>* pathComponents = completeURL.pathComponents;
  if ([pathComponents count] == 2 && [pathComponents[0] isEqualToString:@"/"]) {
    path = pathComponents[1];
  }

  if ([path isEqualToString:kExternalActionOpenNTP]) {
    base::RecordAction(
        base::UserMetricsAction("MobileExternalActionURLOpenedWithOpenNTP"));
    action = IOSExternalAction::ACTION_OPEN_NTP;
    params = [self
        startupParametersForExternalActionWithAppID:appID
                                        completeURL:completeURL
                                        externalURL:GURL(kChromeUINewTabURL)];
  } else if ([path isEqualToString:kExternalActionDefaultBrowserSettings]) {
    base::RecordAction(base::UserMetricsAction(
        "MobileExternalActionURLOpenedWithDefaultBrowserSettings"));

    // If Chrome is already set as default browser, just open the NTP.
    if (IsChromeLikelyDefaultBrowser()) {
      action =
          IOSExternalAction::ACTION_SKIPPED_DEFAULT_BROWSER_SETTINGS_FOR_NTP;
      params = [self
          startupParametersForExternalActionWithAppID:appID
                                          completeURL:completeURL
                                          externalURL:GURL(kChromeUINewTabURL)];
    } else {
      action = IOSExternalAction::ACTION_DEFAULT_BROWSER_SETTINGS;
      params = [self startupParametersForExternalActionWithAppID:appID
                                                     completeURL:completeURL
                                                     externalURL:GURL()];
      params.postOpeningAction = EXTERNAL_ACTION_SHOW_BROWSER_SETTINGS;
    }
  } else {
    action = IOSExternalAction::ACTION_INVALID;
    params = nil;
  }

  base::UmaHistogramEnumeration(kExternalActionHistogram, action);
  params.openedViaFirstPartyScheme = CallerAppIsFirstParty(params.callerApp);
  return params;
}

+ (instancetype)startupParametersForExtensionCommandWithURL:(NSURL*)URL
                                          sourceApplication:(NSString*)appID {
  NSUserDefaults* sharedDefaults = app_group::GetGroupUserDefaults();

  NSString* commandDictionaryPreference =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandPreference);
  NSDictionary* commandDictionary = base::apple::ObjCCast<NSDictionary>(
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
  NSString* commandCaller = base::apple::ObjCCast<NSString>(
      [commandDictionary objectForKey:commandCallerPreference]);

  NSString* commandPreference = base::SysUTF8ToNSString(
      app_group::kChromeAppGroupCommandCommandPreference);
  NSString* command = base::apple::ObjCCast<NSString>(
      [commandDictionary objectForKey:commandPreference]);

  NSString* commandTimePreference =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandTimePreference);
  id commandTime = base::apple::ObjCCast<NSDate>(
      [commandDictionary objectForKey:commandTimePreference]);

  NSString* commandTextPreference =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandTextPreference);
  NSString* externalText = base::apple::ObjCCast<NSString>(
      [commandDictionary objectForKey:commandTextPreference]);

  NSString* commandDataPreference =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandDataPreference);
  NSData* externalData = base::apple::ObjCCast<NSData>(
      [commandDictionary objectForKey:commandDataPreference]);

  NSString* commandIndexPreference =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandIndexPreference);
  NSNumber* index = base::apple::ObjCCast<NSNumber>(
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
  return [ChromeAppStartupParameters startupParametersForCommand:command
                                                withExternalText:externalText
                                                    externalData:externalData
                                                           index:index
                                                             URL:URL
                                               sourceApplication:appID
                                         secureSourceApplication:commandCaller];
}

+ (instancetype)startupParametersForCommand:(NSString*)command
                           withExternalText:(NSString*)externalText
                               externalData:(NSData*)externalData
                                      index:(NSNumber*)index
                                        URL:(NSURL*)URL
                          sourceApplication:(NSString*)appID
                    secureSourceApplication:(NSString*)secureAppID {
  SearchExtensionAction action = ACTION_NO_ACTION;
  ChromeAppStartupParameters* params = nil;

  if ([command
          isEqualToString:base::SysUTF8ToNSString(
                              app_group::kChromeAppGroupVoiceSearchCommand)]) {
    params = [[ChromeAppStartupParameters alloc]
        initWithExternalURL:GURL(kChromeUINewTabURL)
          declaredSourceApp:appID
            secureSourceApp:secureAppID
                completeURL:URL
            applicationMode:ApplicationModeForTabOpening::NORMAL];
    [params setPostOpeningAction:START_VOICE_SEARCH];
    action = ACTION_NEW_VOICE_SEARCH;
  }

  if ([command isEqualToString:base::SysUTF8ToNSString(
                                   app_group::kChromeAppGroupNewTabCommand)]) {
    params = [[ChromeAppStartupParameters alloc]
        initWithExternalURL:GURL(kChromeUINewTabURL)
          declaredSourceApp:appID
            secureSourceApp:secureAppID
                completeURL:URL
            applicationMode:ApplicationModeForTabOpening::NORMAL];
    action = ACTION_NO_ACTION;
  }

  if ([command
          isEqualToString:base::SysUTF8ToNSString(
                              app_group::kChromeAppGroupFocusOmniboxCommand)]) {
    params = [[ChromeAppStartupParameters alloc]
        initWithExternalURL:GURL(kChromeUINewTabURL)
          declaredSourceApp:appID
            secureSourceApp:secureAppID
                completeURL:URL
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
          declaredSourceApp:appID
            secureSourceApp:secureAppID
                completeURL:URL
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
          declaredSourceApp:appID
            secureSourceApp:secureAppID
                completeURL:URL
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
          declaredSourceApp:appID
            secureSourceApp:secureAppID
                completeURL:URL
            applicationMode:ApplicationModeForTabOpening::UNDETERMINED];

    params.imageSearchData = externalData;

    action = ACTION_SEARCH_IMAGE;
  }

  if ([command
          isEqualToString:base::SysUTF8ToNSString(
                              app_group::kChromeAppGroupQRScannerCommand)]) {
    params = [[ChromeAppStartupParameters alloc]
        initWithExternalURL:GURL(kChromeUINewTabURL)
          declaredSourceApp:appID
            secureSourceApp:secureAppID
                completeURL:URL
            applicationMode:ApplicationModeForTabOpening::NORMAL];
    [params setPostOpeningAction:START_QR_CODE_SCANNER];

    action = ACTION_NEW_QR_CODE_SEARCH;
  }

  if ([command isEqualToString:base::SysUTF8ToNSString(
                                   app_group::kChromeAppGroupLensCommand)]) {
    params = [[ChromeAppStartupParameters alloc]
        initWithExternalURL:GURL()
          declaredSourceApp:appID
            secureSourceApp:secureAppID
                completeURL:URL
            applicationMode:ApplicationModeForTabOpening::NORMAL];
    [params setPostOpeningAction:START_LENS_FROM_HOME_SCREEN_WIDGET];
    action = ACTION_LENS;
  }

  if ([command isEqualToString:
                   base::SysUTF8ToNSString(
                       app_group::kChromeAppGroupIncognitoSearchCommand)]) {
    params = [[ChromeAppStartupParameters alloc]
        initWithExternalURL:GURL(kChromeUINewTabURL)
          declaredSourceApp:appID
            secureSourceApp:secureAppID
                completeURL:URL
            applicationMode:ApplicationModeForTabOpening::INCOGNITO];
    [params setPostOpeningAction:FOCUS_OMNIBOX];
    action = ACTION_NEW_INCOGNITO_SEARCH;
  }

  if ([command isEqualToString:
                   base::SysUTF8ToNSString(
                       app_group::kChromeAppGroupSearchPasswordsCommand)]) {
    params = [[ChromeAppStartupParameters alloc]
        initWithExternalURL:GURL()
          declaredSourceApp:appID
            secureSourceApp:secureAppID
                completeURL:URL
            applicationMode:ApplicationModeForTabOpening::NORMAL];
    [params setPostOpeningAction:SEARCH_PASSWORDS];
    action = ACTION_NO_ACTION;
  }

  if ([secureAppID
          isEqualToString:app_group::kOpenCommandSourceSearchExtension]) {
    UMA_HISTOGRAM_ENUMERATION("IOS.SearchExtension.Action", action,
                              SEARCH_EXTENSION_ACTION_COUNT);
  }
  if ([secureAppID
          isEqualToString:app_group::kOpenCommandSourceContentExtension] &&
      index) {
    UMA_HISTOGRAM_COUNTS_100("IOS.ContentExtension.Index",
                             [index integerValue]);
  }
  if ([secureAppID isEqualToString:kWidgetKitHostSearchWidget]) {
    LogWidgetKitAction(WidgetKitExtensionAction::ACTION_SEARCH_WIDGET_SEARCH);
  }
  if ([secureAppID isEqualToString:kWidgetKitHostQuickActionsWidget]) {
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
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }
  if ([secureAppID isEqualToString:kWidgetKitHostLockscreenLauncherWidget]) {
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
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }
  if ([secureAppID isEqualToString:kWidgetKitHostShortcutsWidget]) {
    switch (action) {
      case ACTION_NEW_SEARCH:
        LogWidgetKitAction(WidgetKitExtensionAction::ACTION_SHORTCUTS_SEARCH);
        break;
      case ACTION_OPEN_URL:
        LogWidgetKitAction(WidgetKitExtensionAction::ACTION_SHORTCUTS_OPEN);
        break;
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }
  if ([secureAppID isEqualToString:kWidgetKitHostSearchPasswordsWidget]) {
    LogWidgetKitAction(WidgetKitExtensionAction::
                           ACTION_SEARCH_PASSWORDS_WIDGET_SEARCH_PASSWORDS);
    UMA_HISTOGRAM_ENUMERATION(
        "PasswordManager.ManagePasswordsReferrer",
        password_manager::ManagePasswordsReferrer::kSearchPasswordsWidget);
    base::RecordAction(base::UserMetricsAction(
        "MobileSearchPasswordsWidgetOpenPasswordManager"));
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
  if ([_secureSourceApp
          isEqualToString:app_group::kOpenCommandSourceOpenExtension]) {
    return CALLER_APP_GOOGLE_CHROME_OPEN_EXTENSION;
  }

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
          isEqualToString:[base::apple::FrameworkBundle() bundleIdentifier]]) {
    return CALLER_APP_GOOGLE_CHROME;
  }
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
