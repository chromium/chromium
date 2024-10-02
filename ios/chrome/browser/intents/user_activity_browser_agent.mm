// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intents/user_activity_browser_agent.h"

#import <CoreSpotlight/CoreSpotlight.h>
#import <Intents/Intents.h>
#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/ios/block_types.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/bind_post_task.h"
#import "components/crash/core/common/crash_key.h"
#import "components/handoff/handoff_utility.h"
#import "components/prefs/pref_service.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#import "ios/chrome/app/application_mode.h"
#import "ios/chrome/app/spotlight/actions_spotlight_manager.h"
#import "ios/chrome/app/spotlight/spotlight_util.h"
#import "ios/chrome/app/startup/app_launch_metrics.h"
#import "ios/chrome/browser/intents/intent_type.h"
#import "ios/chrome/browser/intents/intents_constants.h"
#import "ios/chrome/browser/metrics/model/first_user_action_recorder.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/connection_information.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/url_loading/model/image_search_param_generator.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/common/intents/AddBookmarkToChromeIntent.h"
#import "ios/chrome/common/intents/AddReadingListItemToChromeIntent.h"
#import "ios/chrome/common/intents/OpenInChromeIncognitoIntent.h"
#import "ios/chrome/common/intents/OpenInChromeIntent.h"
#import "ios/chrome/common/intents/SearchInChromeIntent.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/page_transition_types.h"

using base::UserMetricsAction;

namespace {

// Constants for compatible mode for user activities.
NSString* const kRegularMode = @"RegularMode";
NSString* const kIncognitoMode = @"IncognitoMode";

std::vector<GURL> CreateGURLVectorFromIntentURLs(NSArray<NSURL*>* intent_urls) {
  std::vector<GURL> urls;
  for (NSURL* url in intent_urls) {
    urls.push_back(net::GURLWithNSURL(url));
  }
  return urls;
}

// Returns the compatible mode array for an user activity.
NSArray* CompatibleModeForActivityType(NSString* activity_type) {
  if ([activity_type isEqualToString:CSSearchableItemActionType] ||
      [activity_type isEqualToString:kShortcutNewSearch] ||
      [activity_type isEqualToString:kShortcutVoiceSearch] ||
      [activity_type isEqualToString:kShortcutQRScanner] ||
      [activity_type isEqualToString:kShortcutLensFromAppIconLongPress] ||
      [activity_type isEqualToString:kShortcutLensFromSpotlight] ||
      [activity_type isEqualToString:kSiriShortcutAddBookmarkToChrome] ||
      [activity_type isEqualToString:kSiriShortcutAddReadingListItemToChrome] ||
      [activity_type isEqualToString:kSiriShortcutSearchInChrome] ||
      [activity_type isEqualToString:NSUserActivityTypeBrowsingWeb]) {
    return @[ kRegularMode, kIncognitoMode ];
  } else if ([activity_type isEqualToString:kSiriShortcutOpenInChrome]) {
    return @[ kRegularMode ];
  } else if ([activity_type isEqualToString:kShortcutNewIncognitoSearch] ||
             [activity_type isEqualToString:kSiriShortcutOpenInIncognito]) {
    return @[ kIncognitoMode ];
  } else {
    // Use 32 as the maximum length of the reported value for this key (31
    // characters + '\0'). See NSUserActivityTypes in Info.plist for the list of
    // expected values.
    static crash_reporter::CrashKeyString<32> key("activity");
    crash_reporter::ScopedCrashKeyString crash_key(
        &key, base::SysNSStringToUTF8(activity_type));
    base::debug::DumpWithoutCrashing();
  }
  return nil;
}

}  // namespace

BROWSER_USER_DATA_KEY_IMPL(UserActivityBrowserAgent)

UserActivityBrowserAgent::UserActivityBrowserAgent(Browser* browser)
    : browser_(browser), profile_(browser->GetProfile()) {
  SceneState* scene_state = browser_->GetSceneState();
  connection_information_ = scene_state.controller;
  tab_opener_ = scene_state.controller;
  startup_information_ = scene_state.appState.startupInformation;
}

UserActivityBrowserAgent::~UserActivityBrowserAgent() {}

#pragma mark - Public methods.

BOOL UserActivityBrowserAgent::ContinueUserActivity(
    NSUserActivity* user_activity,
    BOOL application_is_active) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NSURL* webpage_url = user_activity.webpageURL;

  if ([user_activity.activityType
          isEqualToString:handoff::kChromeHandoffActivityType] ||
      [user_activity.activityType
          isEqualToString:NSUserActivityTypeBrowsingWeb]) {
    // App was launched by iOS as a result of Handoff.
    base::UmaHistogramEnumeration(kAppLaunchSource, AppLaunchSource::HANDOFF);
  } else if (spotlight::IsSpotlightAvailable() &&
             [user_activity.activityType
                 isEqualToString:CSSearchableItemActionType]) {
    // App was launched by iOS as the result of a tap on a Spotlight Search
    // result.
    NSString* item_id = [user_activity.userInfo
        objectForKey:CSSearchableItemActivityIdentifier];
    spotlight::Domain domain = spotlight::SpotlightDomainFromString(item_id);
    base::UmaHistogramEnumeration("IOS.Spotlight.Origin", domain);

    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SPOTLIGHT_CHROME);
    if (!item_id || domain == spotlight::DOMAIN_UNKNOWN) {
      return NO;
    }
    if (domain == spotlight::DOMAIN_ACTIONS) {
      webpage_url =
          [NSURL URLWithString:base::SysUTF8ToNSString(kChromeUINewTabURL)];
      AppStartupParameters* startup_params = [[AppStartupParameters alloc]
          initWithExternalURL:GURL(kChromeUINewTabURL)
                  completeURL:GURL(kChromeUINewTabURL)
              applicationMode:ApplicationModeForTabOpening::UNDETERMINED];
      BOOL startup_params_set =
          spotlight::SetStartupParametersForSpotlightAction(item_id,
                                                            startup_params);
      if (!startup_params_set) {
        return NO;
      }
      [connection_information_ setStartupParameters:startup_params];
    } else if (!webpage_url) {
      spotlight::GetURLForSpotlightItemID(
          item_id,
          base::CallbackToBlock(base::BindPostTask(
              base::SequencedTaskRunner::GetCurrentDefault(),
              base::BindOnce(
                  &UserActivityBrowserAgent::OverloadContinueUserActivityURL,
                  weak_ptr_factory_.GetWeakPtr(),
                  domain == spotlight::DOMAIN_OPEN_TABS))));
      return YES;
    }
  } else if ([user_activity.activityType
                 isEqualToString:kSiriShortcutSearchInChrome]) {
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);
    base::RecordAction(UserMetricsAction("IOSLaunchedBySearchInChromeIntent"));
    base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                  IntentType::kSearchInChrome);

    AppStartupParameters* startup_params = [[AppStartupParameters alloc]
        initWithExternalURL:GURL(kChromeUINewTabURL)
                completeURL:GURL(kChromeUINewTabURL)
            applicationMode:ApplicationModeForTabOpening::NORMAL];

    if (IsIncognitoModeForced(profile_->GetPrefs())) {
      // Set incognito mode to yes if only incognito mode is available.
      startup_params.applicationMode = ApplicationModeForTabOpening::INCOGNITO;
    }

    SearchInChromeIntent* intent =
        base::apple::ObjCCastStrict<SearchInChromeIntent>(
            user_activity.interaction.intent);
    if (!intent) {
      return NO;
    }

    id search_phrase = [intent valueForKey:@"searchPhrase"];

    if ([search_phrase isKindOfClass:[NSString class]] &&
        [search_phrase
            stringByTrimmingCharactersInSet:[NSCharacterSet
                                                whitespaceCharacterSet]]
                .length > 0) {
      startup_params.textQuery = search_phrase;
    } else {
      startup_params.postOpeningAction = FOCUS_OMNIBOX;
    }

    [connection_information_ setStartupParameters:startup_params];
    webpage_url =
        [NSURL URLWithString:base::SysUTF8ToNSString(kChromeUINewTabURL)];

  } else if ([user_activity.activityType
                 isEqualToString:kSiriShortcutOpenInChrome]) {
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);
    base::RecordAction(UserMetricsAction("IOSLaunchedByOpenInChromeIntent"));
    base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                  IntentType::kOpenInChrome);

    OpenInChromeIntent* intent =
        base::apple::ObjCCastStrict<OpenInChromeIntent>(
            user_activity.interaction.intent);

    if (!intent.url) {
      return NO;
    }

    std::vector<GURL> urls;

    if ([intent.url isKindOfClass:[NSURL class]]) {
      // Old intent version where `url` is of type NSURL rather than an array.
      GURL webpage_GURL(
          net::GURLWithNSURL(base::apple::ObjCCastStrict<NSURL>(intent.url)));
      if (!webpage_GURL.is_valid()) {
        return NO;
      }
      urls.push_back(webpage_GURL);
    } else if ([intent.url isKindOfClass:[NSArray class]] &&
               intent.url.count > 0) {
      urls = CreateGURLVectorFromIntentURLs(intent.url);
    } else {
      // Unknown or invalid intent object.
      return NO;
    }

    OpenRequestedURLs(urls, application_is_active, NO);
    return YES;

  } else if ([user_activity.activityType
                 isEqualToString:kSiriShortcutOpenInIncognito]) {
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);
    base::RecordAction(UserMetricsAction("IOSLaunchedByOpenInIncognitoIntent"));
    base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                  IntentType::kOpenInIncognito);

    OpenInChromeIncognitoIntent* intent =
        base::apple::ObjCCastStrict<OpenInChromeIncognitoIntent>(
            user_activity.interaction.intent);

    if (!intent.url || intent.url.count == 0) {
      return NO;
    }

    std::vector<GURL> urls = CreateGURLVectorFromIntentURLs(intent.url);

    OpenRequestedURLs(urls, application_is_active, YES);
    return YES;

  } else if ([user_activity.activityType
                 isEqualToString:kSiriShortcutAddBookmarkToChrome]) {
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);
    base::RecordAction(
        UserMetricsAction("IOSLaunchedByAddBookmarkToChromeIntent"));

    AddBookmarkToChromeIntent* intent =
        base::apple::ObjCCastStrict<AddBookmarkToChromeIntent>(
            user_activity.interaction.intent);

    if (!intent || !intent.url || intent.url.count == 0) {
      return NO;
    }

    AppStartupParameters* startup_params =
        StartupParametersForOpeningNewTab(ADD_BOOKMARKS);
    startup_params.inputURLs = intent.url;
    [connection_information_ setStartupParameters:startup_params];
  } else if ([user_activity.activityType
                 isEqualToString:kSiriShortcutAddReadingListItemToChrome]) {
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);
    base::RecordAction(
        UserMetricsAction("IOSLaunchedByAddReadingListItemToChromeIntent"));

    AddReadingListItemToChromeIntent* intent =
        base::apple::ObjCCastStrict<AddReadingListItemToChromeIntent>(
            user_activity.interaction.intent);

    if (!intent || !intent.url || intent.url.count == 0) {
      return NO;
    }

    AppStartupParameters* startup_params =
        StartupParametersForOpeningNewTab(ADD_READING_LIST_ITEMS);
    startup_params.inputURLs = intent.url;
    [connection_information_ setStartupParameters:startup_params];
  } else if ([user_activity.activityType isEqualToString:kSiriOpenLatestTab]) {
    base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                  IntentType::kOpenLatestTab);
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);

    AppStartupParameters* startup_params = [[AppStartupParameters alloc]
        initWithExternalURL:GURL()
                completeURL:GURL()
            applicationMode:ApplicationModeForTabOpening::NORMAL];

    startup_params.postOpeningAction = OPEN_LATEST_TAB;
    [connection_information_ setStartupParameters:startup_params];
  } else if ([user_activity.activityType
                 isEqualToString:kSiriOpenReadingList]) {
    base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                  IntentType::kOpenReadingList);
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);

    [connection_information_
        setStartupParameters:StartupParametersForOpeningNewTab(
                                 OPEN_READING_LIST)];
  } else if ([user_activity.activityType isEqualToString:kSiriOpenBookmarks]) {
    base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                  IntentType::kOpenBookmarks);
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);

    [connection_information_
        setStartupParameters:StartupParametersForOpeningNewTab(OPEN_BOOKMARKS)];
  } else if ([user_activity.activityType isEqualToString:kSiriOpenRecentTabs]) {
    base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                  IntentType::kOpenRecentTabs);
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);

    [connection_information_
        setStartupParameters:StartupParametersForOpeningNewTab(
                                 OPEN_RECENT_TABS)];
  } else if ([user_activity.activityType isEqualToString:kSiriOpenTabGrid]) {
    base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                  IntentType::kOpenTabGrid);
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);

    [connection_information_
        setStartupParameters:StartupParametersForOpeningNewTab(OPEN_TAB_GRID)];
  } else if ([user_activity.activityType isEqualToString:kSiriVoiceSearch]) {
    base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                  IntentType::kOpenVoiceSearch);
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);

    [connection_information_
        setStartupParameters:StartupParametersForOpeningNewTab(
                                 START_VOICE_SEARCH)];
  } else if ([user_activity.activityType isEqualToString:kSiriOpenNewTab]) {
    base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                  IntentType::kOpenNewTab);
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);

    [connection_information_
        setStartupParameters:StartupParametersForOpeningNewTab(NO_ACTION)];
  } else if ([user_activity.activityType isEqualToString:kSiriPlayDinoGame]) {
    base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                  IntentType::kPlayDinoGame);
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);

    webpage_url =
        [NSURL URLWithString:base::SysUTF8ToNSString(kChromeDinoGameURL)];
  } else if ([user_activity.activityType
                 isEqualToString:kSiriSetChromeDefaultBrowser]) {
    base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                  IntentType::kSetDefaultBrowser);
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);

    [connection_information_
        setStartupParameters:StartupParametersForOpeningNewTab(
                                 SET_CHROME_DEFAULT_BROWSER)];
  } else if ([user_activity.activityType isEqualToString:kSiriViewHistory]) {
    base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                  IntentType::kViewHistory);
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);

    [connection_information_
        setStartupParameters:StartupParametersForOpeningNewTab(VIEW_HISTORY)];
  } else if ([user_activity.activityType
                 isEqualToString:kSiriOpenNewIncognitoTab]) {
    base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                  IntentType::kOpenNewIncognitoTab);
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);

    AppStartupParameters* startup_params = [[AppStartupParameters alloc]
        initWithExternalURL:GURL(kChromeUINewTabURL)
                completeURL:GURL(kChromeUINewTabURL)
            applicationMode:ApplicationModeForTabOpening::INCOGNITO];
    [connection_information_ setStartupParameters:startup_params];
  } else if ([user_activity.activityType
                 isEqualToString:kSiriManagePaymentMethods]) {
    base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                  IntentType::kManagePaymentMethods);
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);

    [connection_information_
        setStartupParameters:StartupParametersForOpeningNewTab(
                                 OPEN_PAYMENT_METHODS)];
  } else if ([user_activity.activityType isEqualToString:kSiriRunSafetyCheck]) {
    base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                  IntentType::kRunSafetyCheck);
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);

    [connection_information_
        setStartupParameters:StartupParametersForOpeningNewTab(
                                 RUN_SAFETY_CHECK)];
  } else if ([user_activity.activityType
                 isEqualToString:kSiriManagePasswords]) {
    base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                  IntentType::kManagePasswords);
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);

    [connection_information_
        setStartupParameters:StartupParametersForOpeningNewTab(
                                 MANAGE_PASSWORDS)];
  } else if ([user_activity.activityType isEqualToString:kSiriManageSettings]) {
    base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                  IntentType::kManageSettings);
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);

    [connection_information_
        setStartupParameters:StartupParametersForOpeningNewTab(
                                 MANAGE_SETTINGS)];
  } else if ([user_activity.activityType
                 isEqualToString:kSiriOpenLensFromIntents]) {
    base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                  IntentType::kStartLens);
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);
    [connection_information_
        setStartupParameters:StartupParametersForOpeningNewTab(
                                 START_LENS_FROM_INTENTS)];
  } else if ([user_activity.activityType
                 isEqualToString:kSiriClearBrowsingData]) {
    base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                  IntentType::kClearBrowsingData);
    base::UmaHistogramEnumeration(kAppLaunchSource,
                                  AppLaunchSource::SIRI_SHORTCUT);

    [connection_information_
        setStartupParameters:StartupParametersForOpeningNewTab(
                                 OPEN_CLEAR_BROWSING_DATA_DIALOG)];
  } else {
    // Do nothing for unknown activity type.
    return NO;
  }
  return ContinueUserActivityURL(webpage_url, application_is_active, NO);
}

BOOL UserActivityBrowserAgent::Handle3DTouchApplicationShortcuts(
    UIApplicationShortcutItem* shortcut_item) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const BOOL handled_shortcut_item = HandleShortcutItem(shortcut_item);
  const BOOL is_active = [[UIApplication sharedApplication] applicationState] ==
                         UIApplicationStateActive;
  if (handled_shortcut_item && is_active) {
    RouteToCorrectTab();
  }
  return handled_shortcut_item;
}

void UserActivityBrowserAgent::RouteToCorrectTab() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AppInitStage init_stage = browser_->GetSceneState().appState.initStage;
  // Do not load the external URL if the user has not accepted the terms of
  // service. This corresponds to the case when the user installed Chrome,
  // has never launched it and attempts to open an external URL in Chrome.
  if (init_stage <= AppInitStage::kFirstRun) {
    return;
  }
  // Do not handle the parameters that are/were already handled.
  if (connection_information_.startupParametersAreBeingHandled) {
    return;
  }

  connection_information_.startupParametersAreBeingHandled = YES;

  if (!connection_information_.startupParameters.URLs.empty() &&
      !connection_information_.startupParameters.isUnexpectedMode) {
    OpenMultipleTabs();
    return;
  }

  GURL external_url = connection_information_.startupParameters.externalURL;

  // If the user intent to open a url in a unavailable mode, don't fulfill the
  // request.
  if (external_url != kChromeUINewTabURL &&
      connection_information_.startupParameters.isUnexpectedMode) {
    return;
  }

  // TODO(crbug.com/41443029): Exacly the same copy of this code is present in
  // +[URLOpener
  // openURL:applicationActive:options:tabOpener:startupInformation:]

  // The app is already active so the applicationDidBecomeActive: method
  // will never be called. Open the requested URL after all modal UIs have
  // been dismissed. `_startupParameters` must be retained until all deferred
  // modal UIs are dismissed and tab opened (or Incognito interstitial shown)
  // with requested URL.
  ApplicationModeForTabOpening target_mode =
      [[connection_information_ startupParameters] applicationMode];
  GURL url;
  GURL virtual_url;
  GURL complete_url = connection_information_.startupParameters.completeURL;
  if (complete_url.SchemeIsFile()) {
    // External URL will be loaded by WebState, which expects `complete_url`.
    // Omnibox however suppose to display `external_url`, which is used as
    // virtual URL.
    url = complete_url;
    virtual_url = external_url;
  } else {
    url = external_url;
  }
  UrlLoadParams params;
  if (connection_information_.startupParameters.openExistingTab) {
    web::NavigationManager::WebLoadParams web_load_params =
        web::NavigationManager::WebLoadParams(url);
    params = UrlLoadParams::SwitchToTab(web_load_params);
  } else {
    params = UrlLoadParams::InNewTab(url, virtual_url);
  }

  if (connection_information_.startupParameters.imageSearchData) {
    TemplateURLService* template_url_service =
        ios::TemplateURLServiceFactory::GetForProfile(profile_);

    NSData* image_data =
        connection_information_.startupParameters.imageSearchData;
    web::NavigationManager::WebLoadParams web_load_params =
        ImageSearchParamGenerator::LoadParamsForImageData(image_data, GURL(),
                                                          template_url_service);

    params.web_params = web_load_params;
  } else if (connection_information_.startupParameters.textQuery) {
    NSString* query = connection_information_.startupParameters.textQuery;

    GURL result = GenerateResultGURLFromSearchQuery(query);
    params.web_params.url = result;
  }

  params.from_external = true;

  if ([[connection_information_ startupParameters] applicationMode] !=
          ApplicationModeForTabOpening::INCOGNITO &&
      [tab_opener_ URLIsOpenedInRegularMode:params.web_params.url]) {
    // Record metric.
  }

  base::OnceClosure closure =
      base::BindOnce(&UserActivityBrowserAgent::ClearStartupParameters,
                     weak_ptr_factory_.GetWeakPtr());
  [tab_opener_
      dismissModalsAndMaybeOpenSelectedTabInMode:target_mode
                               withUrlLoadParams:params
                                  dismissOmnibox:[[connection_information_
                                                     startupParameters]
                                                     postOpeningAction] !=
                                                 FOCUS_OMNIBOX
                                      completion:base::CallbackToBlock(
                                                     std::move(closure))];
}

BOOL UserActivityBrowserAgent::ProceedWithUserActivity(
    NSUserActivity* user_activity) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NSArray* array = CompatibleModeForActivityType(user_activity.activityType);
  PrefService* pref_service = profile_->GetPrefs();
  if (IsIncognitoModeDisabled(pref_service)) {
    return [array containsObject:kRegularMode];
  }
  if (IsIncognitoModeForced(pref_service)) {
    return [array containsObject:kIncognitoMode];
  }
  // Return YES if the compatible mode array is not nil.
  return array != nil;
}

#pragma mark - Internal methods.

AppStartupParameters*
UserActivityBrowserAgent::StartupParametersForOpeningNewTab(
    TabOpeningPostOpeningAction action) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AppStartupParameters* startup_params = [[AppStartupParameters alloc]
      initWithExternalURL:GURL(kChromeUINewTabURL)
              completeURL:GURL(kChromeUINewTabURL)
          applicationMode:ApplicationModeForTabOpening::NORMAL];

  startup_params.postOpeningAction = action;
  return startup_params;
}

BOOL UserActivityBrowserAgent::HandleShortcutItem(
    UIApplicationShortcutItem* shortcut_item) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SceneState* scene_state = browser_->GetSceneState();
  AppInitStage init_stage = scene_state.appState.initStage;
  if (init_stage <= AppInitStage::kFirstRun) {
    return NO;
  }
  base::UmaHistogramEnumeration(kAppLaunchSource,
                                AppLaunchSource::LONG_PRESS_ON_APP_ICON);

  // Lens entry points should not open an extra new tab page.
  GURL startup_url =
      ([shortcut_item.type isEqualToString:kShortcutLensFromAppIconLongPress] ||
       [shortcut_item.type isEqualToString:kShortcutLensFromSpotlight])
          ? GURL()
          : GURL(kChromeUINewTabURL);

  AppStartupParameters* startup_params = [[AppStartupParameters alloc]
      initWithExternalURL:startup_url
              completeURL:startup_url
          applicationMode:ApplicationModeForTabOpening::NORMAL];

  if ([shortcut_item.type isEqualToString:kShortcutNewSearch]) {
    base::RecordAction(
        UserMetricsAction("ApplicationShortcut.NewSearchPressed"));
    startup_params.postOpeningAction = FOCUS_OMNIBOX;
    connection_information_.startupParameters = startup_params;
    return YES;

  } else if ([shortcut_item.type isEqualToString:kShortcutNewIncognitoSearch]) {
    base::RecordAction(
        UserMetricsAction("ApplicationShortcut.NewIncognitoSearchPressed"));
    startup_params.applicationMode = ApplicationModeForTabOpening::INCOGNITO;
    startup_params.postOpeningAction = FOCUS_OMNIBOX;
    connection_information_.startupParameters = startup_params;
    return YES;

  } else if ([shortcut_item.type isEqualToString:kShortcutVoiceSearch]) {
    base::RecordAction(
        UserMetricsAction("ApplicationShortcut.VoiceSearchPressed"));
    startup_params.postOpeningAction = START_VOICE_SEARCH;
    connection_information_.startupParameters = startup_params;
    return YES;

  } else if ([shortcut_item.type isEqualToString:kShortcutQRScanner]) {
    base::RecordAction(
        UserMetricsAction("ApplicationShortcut.ScanQRCodePressed"));
    startup_params.postOpeningAction = START_QR_CODE_SCANNER;
    connection_information_.startupParameters = startup_params;
    return YES;
  } else if ([shortcut_item.type
                 isEqualToString:kShortcutLensFromAppIconLongPress]) {
    base::RecordAction(UserMetricsAction(
        "ApplicationShortcut.LensPressedFromAppIconLongPress"));
    startup_params.postOpeningAction = START_LENS_FROM_APP_ICON_LONG_PRESS;
    connection_information_.startupParameters = startup_params;
    return YES;
  } else if ([shortcut_item.type isEqualToString:kShortcutLensFromSpotlight]) {
    base::RecordAction(
        UserMetricsAction("ApplicationShortcut.LensPressedFromSpotlight"));
    startup_params.postOpeningAction = START_LENS_FROM_SPOTLIGHT;
    connection_information_.startupParameters = startup_params;
    return YES;
  }

  // Use 16 as the maximum length of the reported value for this key (15
  // characters + '\0'). Expected values are UIApplicationShortcutItemType
  // entries in Info.plist.
  static crash_reporter::CrashKeyString<16> key("shortcut-item");
  crash_reporter::ScopedCrashKeyString crash_key(
      &key, base::SysNSStringToUTF8(shortcut_item.type));
  base::debug::DumpWithoutCrashing();
  return NO;
}

void UserActivityBrowserAgent::OpenRequestedURLs(
    const std::vector<GURL>& webpage_urls,
    BOOL application_is_active,
    BOOL incognito) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ApplicationModeForTabOpening application_mode;
  if (incognito) {
    application_mode = ApplicationModeForTabOpening::INCOGNITO;
  } else {
    application_mode = ApplicationModeForTabOpening::NORMAL;
  }
  AppStartupParameters* startup_params =
      [[AppStartupParameters alloc] initWithURLs:webpage_urls
                                 applicationMode:application_mode];
  [connection_information_ setStartupParameters:startup_params];

  AppInitStage init_stage = browser_->GetSceneState().appState.initStage;
  if (application_is_active && init_stage > AppInitStage::kFirstRun) {
    // The app is already active so the applicationDidBecomeActive: method will
    // never be called. Open the requested URLs immediately.
    OpenMultipleTabs();
    return;
  }

  // Don't record the first action as a user action, since it will not be
  // initiated by the user.
  [startup_information_ resetFirstUserActionRecorder];

  if (![connection_information_ startupParameters]) {
    startup_params.applicationMode = ApplicationModeForTabOpening::UNDETERMINED;
    if (incognito) {
      startup_params.applicationMode = ApplicationModeForTabOpening::INCOGNITO;
    }
    [connection_information_ setStartupParameters:startup_params];
  }
}

BOOL UserActivityBrowserAgent::ContinueUserActivityURL(
    NSURL* webpage_url,
    BOOL application_is_active,
    BOOL open_existing_tab) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!webpage_url) {
    return NO;
  }

  GURL webpage_GURL(net::GURLWithNSURL(webpage_url));
  if (!webpage_GURL.is_valid()) {
    return NO;
  }

  AppInitStage init_stage = browser_->GetSceneState().appState.initStage;
  if (application_is_active && init_stage > AppInitStage::kFirstRun) {
    // The app is already active so the applicationDidBecomeActive: method will
    // never be called. Open the requested URL immediately.
    ApplicationModeForTabOpening target_mode =
        [[connection_information_ startupParameters] applicationMode];
    UrlLoadParams params = UrlLoadParams::InNewTab(webpage_GURL);

    if (connection_information_.startupParameters.textQuery) {
      NSString* query = connection_information_.startupParameters.textQuery;

      GURL result = GenerateResultGURLFromSearchQuery(query);
      params.web_params.url = result;
    }

    if ([[connection_information_ startupParameters] applicationMode] !=
            ApplicationModeForTabOpening::INCOGNITO &&
        [tab_opener_ URLIsOpenedInRegularMode:webpage_GURL]) {
      // Record metric.
    }

    base::OnceClosure closure =
        base::BindOnce(&UserActivityBrowserAgent::ClearStartupParameters,
                       weak_ptr_factory_.GetWeakPtr());
    [tab_opener_
        dismissModalsAndMaybeOpenSelectedTabInMode:target_mode
                                 withUrlLoadParams:params
                                    dismissOmnibox:YES
                                        completion:base::CallbackToBlock(
                                                       std::move(closure))];
    return YES;
  }

  // Don't record the first action as a user action, since it will not be
  // initiated by the user.
  [startup_information_ resetFirstUserActionRecorder];

  if (![connection_information_ startupParameters]) {
    AppStartupParameters* startup_params = [[AppStartupParameters alloc]
        initWithExternalURL:webpage_GURL
                completeURL:webpage_GURL
            applicationMode:ApplicationModeForTabOpening::NORMAL];
    startup_params.openExistingTab = open_existing_tab;
    [connection_information_ setStartupParameters:startup_params];
  }
  return YES;
}

void UserActivityBrowserAgent::OpenMultipleTabs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BOOL incognito_mode =
      connection_information_.startupParameters.applicationMode ==
      ApplicationModeForTabOpening::INCOGNITO;
  BOOL dismiss_omnibox = [[connection_information_ startupParameters]
                             postOpeningAction] != FOCUS_OMNIBOX;

  // Using a weak reference to `this` to solve a memory leak issue.
  // `tab_opener_` and `connection_information_` are the same object in
  // some cases (SceneController). This retains the object while the block
  // exists. Then this block is passed around and in some cases it ends up
  // stored in BrowserViewController. This results in a memory leak that looks
  // like this: SceneController -> BrowserViewWrangler -> BrowserCoordinator
  // -> BrowserViewController -> SceneController
  base::OnceClosure closure =
      base::BindOnce(&UserActivityBrowserAgent::ClearStartupParameters,
                     weak_ptr_factory_.GetWeakPtr());
  [tab_opener_
      dismissModalsAndOpenMultipleTabsWithURLs:connection_information_
                                                   .startupParameters.URLs
                               inIncognitoMode:incognito_mode
                                dismissOmnibox:dismiss_omnibox
                                    completion:base::CallbackToBlock(
                                                   std::move(closure))];
}

GURL UserActivityBrowserAgent::GenerateResultGURLFromSearchQuery(
    NSString* search_query) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TemplateURLService* template_url_Service =
      ios::TemplateURLServiceFactory::GetForProfile(profile_);

  const TemplateURL* default_url =
      template_url_Service->GetDefaultSearchProvider();
  DCHECK(default_url);
  DCHECK(!default_url->url().empty());
  DCHECK(default_url->url_ref().IsValid(
      template_url_Service->search_terms_data()));
  std::u16string query_string = base::SysNSStringToUTF16(search_query);
  TemplateURLRef::SearchTermsArgs search_args(query_string);

  GURL result(default_url->url_ref().ReplaceSearchTerms(
      search_args, template_url_Service->search_terms_data()));

  return result;
}

void UserActivityBrowserAgent::OverloadContinueUserActivityURL(
    BOOL open_existing_tab,
    NSURL* webpage_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BOOL is_active = [[UIApplication sharedApplication] applicationState] ==
                   UIApplicationStateActive;
  ContinueUserActivityURL(webpage_url, is_active, open_existing_tab);
}

void UserActivityBrowserAgent::ClearStartupParameters() {
  connection_information_.startupParameters = nil;
}
