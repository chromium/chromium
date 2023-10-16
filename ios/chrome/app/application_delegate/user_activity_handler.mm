// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/application_delegate/user_activity_handler.h"

#import <CoreSpotlight/CoreSpotlight.h>
#import <Intents/Intents.h>
#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/ios/block_types.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/crash/core/common/crash_key.h"
#import "components/handoff/handoff_utility.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/app/app_startup_parameters.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/application_delegate/tab_opening.h"
#import "ios/chrome/app/application_mode.h"
#import "ios/chrome/app/spotlight/actions_spotlight_manager.h"
#import "ios/chrome/app/spotlight/spotlight_util.h"
#import "ios/chrome/app/startup/app_launch_metrics.h"
#import "ios/chrome/app/startup/chrome_app_startup_parameters.h"
#import "ios/chrome/browser/intents/intent_type.h"
#import "ios/chrome/browser/metrics/first_user_action_recorder.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/connection_information.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/url_loading/image_search_param_generator.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/common/intents/OpenInChromeIncognitoIntent.h"
#import "ios/chrome/common/intents/OpenInChromeIntent.h"
#import "ios/chrome/common/intents/SearchInChromeIntent.h"
#import "net/base/mac/url_conversions.h"
#import "ui/base/page_transition_types.h"
#import "url/gurl.h"

using base::UserMetricsAction;

namespace {
// Constants for 3D touch application static shortcuts.
NSString* const kShortcutNewSearch = @"OpenNewSearch";
NSString* const kShortcutNewIncognitoSearch = @"OpenIncognitoSearch";
NSString* const kShortcutVoiceSearch = @"OpenVoiceSearch";
NSString* const kShortcutQRScanner = @"OpenQRScanner";
NSString* const kShortcutLensFromAppIconLongPress =
    @"OpenLensFromAppIconLongPress";
NSString* const kShortcutLensFromSpotlight = @"OpenLensFromSpotlight";

// Constants for Siri shortcut.
NSString* const kSiriShortcutOpenInChrome = @"OpenInChromeIntent";
NSString* const kSiriShortcutSearchInChrome = @"SearchInChromeIntent";
NSString* const kSiriShortcutOpenInIncognito = @"OpenInChromeIncognitoIntent";
NSString* const kSiriOpenReadingList = @"OpenReadingListIntent";
NSString* const kSiriOpenBookmarks = @"OpenBookmarksIntent";
NSString* const kSiriOpenRecentTabs = @"OpenRecentTabsIntent";
NSString* const kSiriOpenTabGrid = @"OpenTabGridIntent";
NSString* const kSiriVoiceSearch = @"SearchWithVoiceIntent";
NSString* const kSiriOpenNewTab = @"OpenNewTabIntent";
NSString* const kSiriPlayDinoGame = @"PlayDinoGameIntent";
NSString* const kSiriSetChromeDefaultBrowser = @"SetChromeDefaultBrowserIntent";
NSString* const kSiriViewHistory = @"ViewHistoryIntent";
NSString* const kSiriOpenNewIncognitoTab = @"OpenNewIncognitoTabIntent";
NSString* const kSiriManagePaymentMethods = @"ManagePaymentMethodsIntent";
NSString* const kSiriRunSafetyCheck = @"RunSafetyCheckIntent";
NSString* const kSiriManagePasswords = @"ManagePasswordsIntent";
NSString* const kSiriManageSettings = @"ManageSettingsIntent";
NSString* const kSiriOpenLatestTab = @"OpenLatestTabIntent";
NSString* const kSiriOpenLensFromIntents = @"OpenLensIntent";
NSString* const kSiriClearBrowsingData = @"ClearBrowsingDataIntent";

// Constants for compatible mode for user activities.
NSString* const kRegularMode = @"RegularMode";
NSString* const kIncognitoMode = @"IncognitoMode";

std::vector<GURL> createGURLVectorFromIntentURLs(NSArray<NSURL*>* intentURLs) {
  std::vector<GURL> URLs;
  for (NSURL* URL in intentURLs) {
    URLs.push_back(net::GURLWithNSURL(URL));
  }
  return URLs;
}

// Returns the compatible mode array for an user activity.
NSArray* CompatibleModeForActivityType(NSString* activityType) {
  if ([activityType isEqualToString:CSSearchableItemActionType] ||
      [activityType isEqualToString:kShortcutNewSearch] ||
      [activityType isEqualToString:kShortcutVoiceSearch] ||
      [activityType isEqualToString:kShortcutQRScanner] ||
      [activityType isEqualToString:kShortcutLensFromAppIconLongPress] ||
      [activityType isEqualToString:kShortcutLensFromSpotlight] ||
      [activityType isEqualToString:kSiriShortcutSearchInChrome] ||
      [activityType isEqualToString:NSUserActivityTypeBrowsingWeb]) {
    return @[ kRegularMode, kIncognitoMode ];
  } else if ([activityType isEqualToString:kSiriShortcutOpenInChrome]) {
    return @[ kRegularMode ];
  } else if ([activityType isEqualToString:kShortcutNewIncognitoSearch] ||
             [activityType isEqualToString:kSiriShortcutOpenInIncognito]) {
    return @[ kIncognitoMode ];
  } else {
    // Use 32 as the maximum length of the reported value for this key (31
    // characters + '\0'). See NSUserActivityTypes in Info.plist for the list of
    // expected values.
    static crash_reporter::CrashKeyString<32> key("activity");
    crash_reporter::ScopedCrashKeyString crash_key(
        &key, base::SysNSStringToUTF8(activityType));
    base::debug::DumpWithoutCrashing();
  }
  return nil;
}

}  // namespace

@interface UserActivityHandler ()
// Handles the 3D touch application static items. Does nothing if in first run.
+ (BOOL)handleShortcutItem:(UIApplicationShortcutItem*)shortcutItem
     connectionInformation:(id<ConnectionInformation>)connectionInformation
                 initStage:(InitStage)initStage;
@end

@implementation UserActivityHandler

#pragma mark - Public methods.

+ (BOOL)continueUserActivity:(NSUserActivity*)userActivity
         applicationIsActive:(BOOL)applicationIsActive
                   tabOpener:(id<TabOpening>)tabOpener
       connectionInformation:(id<ConnectionInformation>)connectionInformation
          startupInformation:(id<StartupInformation>)startupInformation
                browserState:(ChromeBrowserState*)browserState
                   initStage:(InitStage)initStage {
  NSURL* webpageURL = userActivity.webpageURL;

  if ([userActivity.activityType
          isEqualToString:handoff::kChromeHandoffActivityType] ||
      [userActivity.activityType
          isEqualToString:NSUserActivityTypeBrowsingWeb]) {
    // App was launched by iOS as a result of Handoff.
    NSString* originString = base::apple::ObjCCast<NSString>(
        userActivity.userInfo[handoff::kOriginKey]);
    handoff::Origin origin = handoff::OriginFromString(originString);
    UMA_HISTOGRAM_ENUMERATION("IOS.Handoff.Origin", origin,
                              handoff::ORIGIN_COUNT);
    base::UmaHistogramEnumeration(kAppLaunchSource, AppLaunchSource::HANDOFF);
  } else if (spotlight::IsSpotlightAvailable() &&
             [userActivity.activityType
                 isEqualToString:CSSearchableItemActionType]) {
    // App was launched by iOS as the result of a tap on a Spotlight Search
    // result.
    NSString* itemID =
        [userActivity.userInfo objectForKey:CSSearchableItemActivityIdentifier];
    spotlight::Domain domain = spotlight::SpotlightDomainFromString(itemID);
    UMA_HISTOGRAM_ENUMERATION("IOS.Spotlight.Origin", domain,
                              spotlight::DOMAIN_COUNT);

    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SPOTLIGHT_CHROME);
    if (!itemID) {
      return NO;
    }
    if (domain == spotlight::DOMAIN_ACTIONS) {
      webpageURL =
          [NSURL URLWithString:base::SysUTF8ToNSString(kChromeUINewTabURL)];
      AppStartupParameters* startupParams = [[AppStartupParameters alloc]
          initWithExternalURL:GURL(kChromeUINewTabURL)
                  completeURL:GURL(kChromeUINewTabURL)
              applicationMode:ApplicationModeForTabOpening::UNDETERMINED];
      BOOL startupParamsSet = spotlight::SetStartupParametersForSpotlightAction(
          itemID, startupParams);
      if (!startupParamsSet) {
        return NO;
      }
      [connectionInformation setStartupParameters:startupParams];
    } else if (!webpageURL) {
      spotlight::GetURLForSpotlightItemID(itemID, ^(NSURL* contentURL) {
        if (!contentURL) {
          return;
        }
        dispatch_async(dispatch_get_main_queue(), ^{
          // Update the isActive flag as it may have changed during the async
          // calls.
          BOOL isActive = [[UIApplication sharedApplication]
                              applicationState] == UIApplicationStateActive;

          [self
              continueUserActivityURL:contentURL
                  applicationIsActive:isActive
                            tabOpener:tabOpener
                connectionInformation:connectionInformation
                   startupInformation:startupInformation
                         browserState:browserState
                            initStage:initStage
                      openExistingTab:(domain == spotlight::DOMAIN_OPEN_TABS)];
        });
      });
      return YES;
    }
  } else if ([userActivity.activityType
                 isEqualToString:kSiriShortcutSearchInChrome]) {
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);
    base::RecordAction(UserMetricsAction("IOSLaunchedBySearchInChromeIntent"));
    base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                  IntentType::kSearchInChrome);

    AppStartupParameters* startupParams = [[AppStartupParameters alloc]
        initWithExternalURL:GURL(kChromeUINewTabURL)
                completeURL:GURL(kChromeUINewTabURL)
            applicationMode:ApplicationModeForTabOpening::NORMAL];

    if (IsIncognitoModeForced(browserState->GetPrefs())) {
      // Set incognito mode to yes if only incognito mode is available.
      startupParams.applicationMode = ApplicationModeForTabOpening::INCOGNITO;
    }

    SearchInChromeIntent* intent =
        base::apple::ObjCCastStrict<SearchInChromeIntent>(
            userActivity.interaction.intent);

    if (!intent) {
      return NO;
    }

    id searchPhrase = [intent valueForKey:@"searchPhrase"];

    if ([searchPhrase isKindOfClass:[NSString class]] &&
        [searchPhrase
            stringByTrimmingCharactersInSet:[NSCharacterSet
                                                whitespaceCharacterSet]]
                .length > 0) {
      startupParams.textQuery = searchPhrase;
    } else {
      startupParams.postOpeningAction = FOCUS_OMNIBOX;
    }

    [connectionInformation setStartupParameters:startupParams];
    webpageURL =
        [NSURL URLWithString:base::SysUTF8ToNSString(kChromeUINewTabURL)];

  } else if ([userActivity.activityType
                 isEqualToString:kSiriShortcutOpenInChrome]) {
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);
    base::RecordAction(UserMetricsAction("IOSLaunchedByOpenInChromeIntent"));
    base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                  IntentType::kOpenInChrome);

    OpenInChromeIntent* intent =
        base::apple::ObjCCastStrict<OpenInChromeIntent>(
            userActivity.interaction.intent);

    if (!intent.url) {
      return NO;
    }

    std::vector<GURL> URLs;

    if ([intent.url isKindOfClass:[NSURL class]]) {
      // Old intent version where `url` is of type NSURL rather than an array.
      GURL webpageGURL(
          net::GURLWithNSURL(base::apple::ObjCCastStrict<NSURL>(intent.url)));
      if (!webpageGURL.is_valid())
        return NO;
      URLs.push_back(webpageGURL);
    } else if ([intent.url isKindOfClass:[NSArray class]] &&
               intent.url.count > 0) {
      URLs = createGURLVectorFromIntentURLs(intent.url);
    } else {
      // Unknown or invalid intent object.
      return NO;
    }

    AppStartupParameters* startupParams = [[AppStartupParameters alloc]
           initWithURLs:URLs
        applicationMode:ApplicationModeForTabOpening::NORMAL];

    [connectionInformation setStartupParameters:startupParams];
    return [self continueUserActivityURLs:URLs
                      applicationIsActive:applicationIsActive
                                tabOpener:tabOpener
                    connectionInformation:connectionInformation
                       startupInformation:startupInformation
                                Incognito:NO
                                initStage:initStage];

  } else if ([userActivity.activityType
                 isEqualToString:kSiriShortcutOpenInIncognito]) {
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);
    base::RecordAction(UserMetricsAction("IOSLaunchedByOpenInIncognitoIntent"));
    base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                  IntentType::kOpenInIncognito);

    OpenInChromeIncognitoIntent* intent =
        base::apple::ObjCCastStrict<OpenInChromeIncognitoIntent>(
            userActivity.interaction.intent);

    if (!intent.url || intent.url.count == 0) {
      return NO;
    }

    std::vector<GURL> URLs = createGURLVectorFromIntentURLs(intent.url);

    AppStartupParameters* startupParams = [[AppStartupParameters alloc]
           initWithURLs:URLs
        applicationMode:ApplicationModeForTabOpening::INCOGNITO];

    [connectionInformation setStartupParameters:startupParams];
    return [self continueUserActivityURLs:URLs
                      applicationIsActive:applicationIsActive
                                tabOpener:tabOpener
                    connectionInformation:connectionInformation
                       startupInformation:startupInformation
                                Incognito:YES
                                initStage:initStage];

  } else if ([userActivity.activityType isEqualToString:kSiriOpenLatestTab]) {
    base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                  IntentType::kOpenLatestTab);
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);

    AppStartupParameters* startupParams = [[AppStartupParameters alloc]
        initWithExternalURL:GURL()
                completeURL:GURL()
            applicationMode:ApplicationModeForTabOpening::NORMAL];

    startupParams.postOpeningAction = OPEN_LATEST_TAB;
    connectionInformation.startupParameters = startupParams;
  } else if ([userActivity.activityType isEqualToString:kSiriOpenReadingList]) {
    base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                  IntentType::kOpenReadingList);
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);

    [connectionInformation
        setStartupParameters:[self startupParametersForOpeningNewTabWithAction:
                                       OPEN_READING_LIST]];
  } else if ([userActivity.activityType isEqualToString:kSiriOpenBookmarks]) {
    base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                  IntentType::kOpenBookmarks);
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);

    [connectionInformation
        setStartupParameters:
            [self startupParametersForOpeningNewTabWithAction:OPEN_BOOKMARKS]];
  } else if ([userActivity.activityType isEqualToString:kSiriOpenRecentTabs]) {
    base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                  IntentType::kOpenRecentTabs);
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);

    [connectionInformation
        setStartupParameters:[self startupParametersForOpeningNewTabWithAction:
                                       OPEN_RECENT_TABS]];
  } else if ([userActivity.activityType isEqualToString:kSiriOpenTabGrid]) {
    base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                  IntentType::kOpenTabGrid);
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);

    [connectionInformation
        setStartupParameters:
            [self startupParametersForOpeningNewTabWithAction:OPEN_TAB_GRID]];
  } else if ([userActivity.activityType isEqualToString:kSiriVoiceSearch]) {
    base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                  IntentType::kOpenVoiceSearch);
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);

    [connectionInformation
        setStartupParameters:[self startupParametersForOpeningNewTabWithAction:
                                       START_VOICE_SEARCH]];
  } else if ([userActivity.activityType isEqualToString:kSiriOpenNewTab]) {
    base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                  IntentType::kOpenNewTab);
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);

    [connectionInformation
        setStartupParameters:
            [self startupParametersForOpeningNewTabWithAction:NO_ACTION]];
  } else if ([userActivity.activityType isEqualToString:kSiriPlayDinoGame]) {
    base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                  IntentType::kPlayDinoGame);
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);

    webpageURL =
        [NSURL URLWithString:base::SysUTF8ToNSString(kChromeDinoGameURL)];
  } else if ([userActivity.activityType
                 isEqualToString:kSiriSetChromeDefaultBrowser]) {
    base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                  IntentType::kSetDefaultBrowser);
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);

    [connectionInformation
        setStartupParameters:[self startupParametersForOpeningNewTabWithAction:
                                       SET_CHROME_DEFAULT_BROWSER]];
  } else if ([userActivity.activityType isEqualToString:kSiriViewHistory]) {
    base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                  IntentType::kViewHistory);
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);

    [connectionInformation
        setStartupParameters:
            [self startupParametersForOpeningNewTabWithAction:VIEW_HISTORY]];
  } else if ([userActivity.activityType
                 isEqualToString:kSiriOpenNewIncognitoTab]) {
    base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                  IntentType::kOpenNewIncognitoTab);
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);

    AppStartupParameters* startupParams = [[AppStartupParameters alloc]
        initWithExternalURL:GURL(kChromeUINewTabURL)
                completeURL:GURL(kChromeUINewTabURL)
            applicationMode:ApplicationModeForTabOpening::INCOGNITO];
    [connectionInformation setStartupParameters:startupParams];
  } else if ([userActivity.activityType
                 isEqualToString:kSiriManagePaymentMethods]) {
    base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                  IntentType::kManagePaymentMethods);
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);

    [connectionInformation
        setStartupParameters:[self startupParametersForOpeningNewTabWithAction:
                                       OPEN_PAYMENT_METHODS]];
  } else if ([userActivity.activityType isEqualToString:kSiriRunSafetyCheck]) {
    base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                  IntentType::kRunSafetyCheck);
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);

    [connectionInformation
        setStartupParameters:[self startupParametersForOpeningNewTabWithAction:
                                       RUN_SAFETY_CHECK]];
  } else if ([userActivity.activityType isEqualToString:kSiriManagePasswords]) {
    base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                  IntentType::kManagePasswords);
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);

    [connectionInformation
        setStartupParameters:[self startupParametersForOpeningNewTabWithAction:
                                       MANAGE_PASSWORDS]];
  } else if ([userActivity.activityType isEqualToString:kSiriManageSettings]) {
    base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                  IntentType::kManageSettings);
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);

    [connectionInformation
        setStartupParameters:
            [self startupParametersForOpeningNewTabWithAction:MANAGE_SETTINGS]];
  } else if ([userActivity.activityType
                 isEqualToString:kSiriOpenLensFromIntents]) {
    base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                  IntentType::kStartLens);
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);

    [connectionInformation
        setStartupParameters:[self startupParametersForOpeningNewTabWithAction:
                                       START_LENS_FROM_INTENTS]];
  } else if ([userActivity.activityType
                 isEqualToString:kSiriClearBrowsingData]) {
    base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                  IntentType::kClearBrowsingData);
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);

    [connectionInformation
        setStartupParameters:[self startupParametersForOpeningNewTabWithAction:
                                       OPEN_CLEAR_BROWSING_DATA_DIALOG]];
  }

  else {
    // Do nothing for unknown activity type.
    return NO;
  }

  return [self continueUserActivityURL:webpageURL
                   applicationIsActive:applicationIsActive
                             tabOpener:tabOpener
                 connectionInformation:connectionInformation
                    startupInformation:startupInformation
                          browserState:browserState
                             initStage:initStage
                       openExistingTab:NO];
}

+ (BOOL)continueUserActivityURL:(NSURL*)webpageURL
            applicationIsActive:(BOOL)applicationIsActive
                      tabOpener:(id<TabOpening>)tabOpener
          connectionInformation:(id<ConnectionInformation>)connectionInformation
             startupInformation:(id<StartupInformation>)startupInformation
                   browserState:(ChromeBrowserState*)browserState
                      initStage:(InitStage)initStage
                openExistingTab:(BOOL)openExistingTab {
  if (!webpageURL)
    return NO;

  GURL webpageGURL(net::GURLWithNSURL(webpageURL));
  if (!webpageGURL.is_valid())
    return NO;

  if (applicationIsActive && initStage > InitStageFirstRun) {
    // The app is already active so the applicationDidBecomeActive: method will
    // never be called. Open the requested URL immediately.
    ApplicationModeForTabOpening targetMode =
        [[connectionInformation startupParameters] applicationMode];
    UrlLoadParams params = UrlLoadParams::InNewTab(webpageGURL);

    if (connectionInformation.startupParameters.textQuery) {
      NSString* query = connectionInformation.startupParameters.textQuery;

      GURL result = [self generateResultGURLFromSearchQuery:query
                                               browserState:browserState];
      params.web_params.url = result;
    }

    if ([[connectionInformation startupParameters] applicationMode] !=
            ApplicationModeForTabOpening::INCOGNITO &&
        [tabOpener URLIsOpenedInRegularMode:webpageGURL]) {
      // Record metric.
    }
    [tabOpener dismissModalsAndMaybeOpenSelectedTabInMode:targetMode
                                        withUrlLoadParams:params
                                           dismissOmnibox:YES
                                               completion:^{
                                                 [connectionInformation
                                                     setStartupParameters:nil];
                                               }];
    return YES;
  }

  // Don't record the first action as a user action, since it will not be
  // initiated by the user.
  [startupInformation resetFirstUserActionRecorder];

  if (![connectionInformation startupParameters]) {
    AppStartupParameters* startupParams = [[AppStartupParameters alloc]
        initWithExternalURL:webpageGURL
                completeURL:webpageGURL
            applicationMode:ApplicationModeForTabOpening::NORMAL];
    startupParams.openExistingTab = openExistingTab;
    [connectionInformation setStartupParameters:startupParams];
  }
  return YES;
}

+ (void)openMultipleTabsWithConnectionInformation:
            (id<ConnectionInformation>)connectionInformation
                                        tabOpener:(id<TabOpening>)tabOpener {
  BOOL incognitoMode =
      connectionInformation.startupParameters.applicationMode ==
      ApplicationModeForTabOpening::INCOGNITO;
  BOOL dismissOmnibox = [[connectionInformation startupParameters]
                            postOpeningAction] != FOCUS_OMNIBOX;

  // Using a weak reference to `connectionInformation` to solve a memory leak
  // issue. `tabOpener` and `connectionInformation` are the same object in
  // some cases (SceneController). This retains the object while the block
  // exists. Then this block is passed around and in some cases it ends up
  // stored in BrowserViewController. This results in a memory leak that looks
  // like this: SceneController -> BrowserViewWrangler -> BrowserCoordinator
  // -> BrowserViewController -> SceneController
  __weak id<ConnectionInformation> weakConnectionInfo = connectionInformation;

  [tabOpener
      dismissModalsAndOpenMultipleTabsWithURLs:weakConnectionInfo
                                                   .startupParameters.URLs
                               inIncognitoMode:incognitoMode
                                dismissOmnibox:dismissOmnibox
                                    completion:^{
                                      weakConnectionInfo.startupParameters =
                                          nil;
                                    }];
}

+ (BOOL)continueUserActivityURLs:(const std::vector<GURL>&)webpageURLs
             applicationIsActive:(BOOL)applicationIsActive
                       tabOpener:(id<TabOpening>)tabOpener
           connectionInformation:
               (id<ConnectionInformation>)connectionInformation
              startupInformation:(id<StartupInformation>)startupInformation
                       Incognito:(BOOL)Incognito
                       initStage:(InitStage)initStage

{
  if (applicationIsActive && initStage > InitStageFirstRun) {
    // The app is already active so the applicationDidBecomeActive: method will
    // never be called. Open the requested URLs immediately.
    [self openMultipleTabsWithConnectionInformation:connectionInformation
                                          tabOpener:tabOpener];
    return YES;
  }

  // Don't record the first action as a user action, since it will not be
  // initiated by the user.
  [startupInformation resetFirstUserActionRecorder];

  if (![connectionInformation startupParameters]) {
    AppStartupParameters* startupParams = [[AppStartupParameters alloc]
           initWithURLs:webpageURLs
        applicationMode:ApplicationModeForTabOpening::UNDETERMINED];
    if (Incognito) {
      startupParams.applicationMode = ApplicationModeForTabOpening::INCOGNITO;
    }
    [connectionInformation setStartupParameters:startupParams];
  }
  return YES;
}

+ (void)performActionForShortcutItem:(UIApplicationShortcutItem*)shortcutItem
                   completionHandler:(void (^)(BOOL succeeded))completionHandler
                           tabOpener:(id<TabOpening>)tabOpener
               connectionInformation:
                   (id<ConnectionInformation>)connectionInformation
                  startupInformation:(id<StartupInformation>)startupInformation
                        browserState:(ChromeBrowserState*)browserState
                           initStage:(InitStage)initStage {
  BOOL handledShortcutItem =
      [UserActivityHandler handleShortcutItem:shortcutItem
                        connectionInformation:connectionInformation
                                    initStage:initStage];
  BOOL isActive = [[UIApplication sharedApplication] applicationState] ==
                  UIApplicationStateActive;
  if (handledShortcutItem && isActive) {
    [UserActivityHandler
        handleStartupParametersWithTabOpener:tabOpener
                       connectionInformation:connectionInformation
                          startupInformation:startupInformation
                                browserState:browserState
                                   initStage:initStage];
  }
  if (completionHandler) {
    completionHandler(handledShortcutItem);
  }
}

+ (BOOL)willContinueUserActivityWithType:(NSString*)userActivityType {
  return
      [userActivityType isEqualToString:handoff::kChromeHandoffActivityType] ||
      (spotlight::IsSpotlightAvailable() &&
       [userActivityType isEqualToString:CSSearchableItemActionType]);
}

+ (GURL)generateResultGURLFromSearchQuery:(NSString*)searchQuery
                             browserState:(ChromeBrowserState*)browserState {
  TemplateURLService* templateURLService =
      ios::TemplateURLServiceFactory::GetForBrowserState(browserState);

  const TemplateURL* defaultURL =
      templateURLService->GetDefaultSearchProvider();
  DCHECK(defaultURL);
  DCHECK(!defaultURL->url().empty());
  DCHECK(
      defaultURL->url_ref().IsValid(templateURLService->search_terms_data()));
  std::u16string queryString = base::SysNSStringToUTF16(searchQuery);
  TemplateURLRef::SearchTermsArgs search_args(queryString);

  GURL result(defaultURL->url_ref().ReplaceSearchTerms(
      search_args, templateURLService->search_terms_data()));

  return result;
}

+ (void)handleStartupParametersWithTabOpener:(id<TabOpening>)tabOpener
                       connectionInformation:
                           (id<ConnectionInformation>)connectionInformation
                          startupInformation:
                              (id<StartupInformation>)startupInformation
                                browserState:(ChromeBrowserState*)browserState
                                   initStage:(InitStage)initStage {
  // Do not load the external URL if the user has not accepted the terms of
  // service. This corresponds to the case when the user installed Chrome,
  // has never launched it and attempts to open an external URL in Chrome.
  if (initStage <= InitStageFirstRun) {
    return;
  }

  // Do not handle the parameters that are/were already handled.
  if (connectionInformation.startupParametersAreBeingHandled) {
    return;
  }

  connectionInformation.startupParametersAreBeingHandled = YES;

  if (!connectionInformation.startupParameters.URLs.empty() &&
      !connectionInformation.startupParameters.isUnexpectedMode) {
    [self openMultipleTabsWithConnectionInformation:connectionInformation
                                          tabOpener:tabOpener];
    return;
  }

  GURL externalURL = connectionInformation.startupParameters.externalURL;

  // If the user intent to open a url in a unavailable mode, don't fulfill the
  // request.
  if (externalURL != kChromeUINewTabURL &&
      connectionInformation.startupParameters.isUnexpectedMode) {
    return;
  }

  // TODO(crbug.com/935019): Exacly the same copy of this code is present in
  // +[URLOpener
  // openURL:applicationActive:options:tabOpener:startupInformation:]

  // The app is already active so the applicationDidBecomeActive: method
  // will never be called. Open the requested URL after all modal UIs have
  // been dismissed. `_startupParameters` must be retained until all deferred
  // modal UIs are dismissed and tab opened (or Incognito interstitial shown)
  // with requested URL.
  ApplicationModeForTabOpening targetMode =
      [[connectionInformation startupParameters] applicationMode];
  GURL URL;
  GURL virtualURL;
  GURL completeURL = connectionInformation.startupParameters.completeURL;
  if (completeURL.SchemeIsFile()) {
    // External URL will be loaded by WebState, which expects `completeURL`.
    // Omnibox however suppose to display `externalURL`, which is used as
    // virtual URL.
    URL = completeURL;
    virtualURL = externalURL;
  } else {
    URL = externalURL;
  }
  UrlLoadParams params;
  if (connectionInformation.startupParameters.openExistingTab) {
    web::NavigationManager::WebLoadParams webLoadParams =
        web::NavigationManager::WebLoadParams(URL);
    params = UrlLoadParams::SwitchToTab(webLoadParams);
  } else {
    params = UrlLoadParams::InNewTab(URL, virtualURL);
  }

  if (connectionInformation.startupParameters.imageSearchData) {
    TemplateURLService* templateURLService =
        ios::TemplateURLServiceFactory::GetForBrowserState(browserState);

    NSData* imageData = connectionInformation.startupParameters.imageSearchData;
    web::NavigationManager::WebLoadParams webLoadParams =
        ImageSearchParamGenerator::LoadParamsForImageData(imageData, GURL(),
                                                          templateURLService);

    params.web_params = webLoadParams;
  } else if (connectionInformation.startupParameters.textQuery) {
    NSString* query = connectionInformation.startupParameters.textQuery;

    GURL result = [self generateResultGURLFromSearchQuery:query
                                             browserState:browserState];
    params.web_params.url = result;
  }

  params.from_external = true;

  if ([[connectionInformation startupParameters] applicationMode] !=
          ApplicationModeForTabOpening::INCOGNITO &&
      [tabOpener URLIsOpenedInRegularMode:params.web_params.url]) {
    // Record metric.
  }

  [tabOpener
      dismissModalsAndMaybeOpenSelectedTabInMode:targetMode
                               withUrlLoadParams:params
                                  dismissOmnibox:[[connectionInformation
                                                     startupParameters]
                                                     postOpeningAction] !=
                                                 FOCUS_OMNIBOX
                                      completion:^{
                                        [connectionInformation
                                            setStartupParameters:nil];
                                      }];
}

+ (BOOL)canProceedWithUserActivity:(NSUserActivity*)userActivity
                       prefService:(PrefService*)prefService {
  NSArray* array = CompatibleModeForActivityType(userActivity.activityType);

  if (IsIncognitoModeDisabled(prefService)) {
    return [array containsObject:kRegularMode];
  } else if (IsIncognitoModeForced(prefService)) {
    return [array containsObject:kIncognitoMode];
  }

  // Return YES if the compatible mode array is not nil.
  return array != nil;
}

#pragma mark - Internal methods.

// Returns an app startup parameter for opening a new tab with a post action.
+ (AppStartupParameters*)startupParametersForOpeningNewTabWithAction:
    (TabOpeningPostOpeningAction)action {
  AppStartupParameters* startupParams = [[AppStartupParameters alloc]
      initWithExternalURL:GURL(kChromeUINewTabURL)
              completeURL:GURL(kChromeUINewTabURL)
          applicationMode:ApplicationModeForTabOpening::NORMAL];

  startupParams.postOpeningAction = action;
  return startupParams;
}

+ (BOOL)handleShortcutItem:(UIApplicationShortcutItem*)shortcutItem
     connectionInformation:(id<ConnectionInformation>)connectionInformation
                 initStage:(InitStage)initStage {
  if (initStage <= InitStageFirstRun)
    return NO;

  base::UmaHistogramEnumeration(kAppLaunchSource,
                                AppLaunchSource::LONG_PRESS_ON_APP_ICON);

  // Lens entry points should not open an extra new tab page.
  GURL startupURL =
      ([shortcutItem.type isEqualToString:kShortcutLensFromAppIconLongPress] ||
       [shortcutItem.type isEqualToString:kShortcutLensFromSpotlight])
          ? GURL()
          : GURL(kChromeUINewTabURL);

  AppStartupParameters* startupParams = [[AppStartupParameters alloc]
      initWithExternalURL:startupURL
              completeURL:startupURL
          applicationMode:ApplicationModeForTabOpening::NORMAL];

  if ([shortcutItem.type isEqualToString:kShortcutNewSearch]) {
    base::RecordAction(
        UserMetricsAction("ApplicationShortcut.NewSearchPressed"));
    startupParams.postOpeningAction = FOCUS_OMNIBOX;
    connectionInformation.startupParameters = startupParams;
    return YES;

  } else if ([shortcutItem.type isEqualToString:kShortcutNewIncognitoSearch]) {
    base::RecordAction(
        UserMetricsAction("ApplicationShortcut.NewIncognitoSearchPressed"));
    startupParams.applicationMode = ApplicationModeForTabOpening::INCOGNITO;
    startupParams.postOpeningAction = FOCUS_OMNIBOX;
    connectionInformation.startupParameters = startupParams;
    return YES;

  } else if ([shortcutItem.type isEqualToString:kShortcutVoiceSearch]) {
    base::RecordAction(
        UserMetricsAction("ApplicationShortcut.VoiceSearchPressed"));
    startupParams.postOpeningAction = START_VOICE_SEARCH;
    connectionInformation.startupParameters = startupParams;
    return YES;

  } else if ([shortcutItem.type isEqualToString:kShortcutQRScanner]) {
    base::RecordAction(
        UserMetricsAction("ApplicationShortcut.ScanQRCodePressed"));
    startupParams.postOpeningAction = START_QR_CODE_SCANNER;
    connectionInformation.startupParameters = startupParams;
    return YES;
  } else if ([shortcutItem.type
                 isEqualToString:kShortcutLensFromAppIconLongPress]) {
    base::RecordAction(UserMetricsAction(
        "ApplicationShortcut.LensPressedFromAppIconLongPress"));
    startupParams.postOpeningAction = START_LENS_FROM_APP_ICON_LONG_PRESS;
    connectionInformation.startupParameters = startupParams;
    return YES;
  } else if ([shortcutItem.type isEqualToString:kShortcutLensFromSpotlight]) {
    base::RecordAction(
        UserMetricsAction("ApplicationShortcut.LensPressedFromSpotlight"));
    startupParams.postOpeningAction = START_LENS_FROM_SPOTLIGHT;
    connectionInformation.startupParameters = startupParams;
    return YES;
  }

  // Use 16 as the maximum length of the reported value for this key (15
  // characters + '\0'). Expected values are UIApplicationShortcutItemType
  // entries in Info.plist.
  static crash_reporter::CrashKeyString<16> key("shortcut-item");
  crash_reporter::ScopedCrashKeyString crash_key(
      &key, base::SysNSStringToUTF8(shortcutItem.type));
  base::debug::DumpWithoutCrashing();
  return NO;
}

@end
