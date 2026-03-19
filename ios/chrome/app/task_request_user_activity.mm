// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/task_request_user_activity.h"

#import <CoreSpotlight/CoreSpotlight.h>

#import "base/check.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/handoff/handoff_utility.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/spotlight/spotlight_util.h"
#import "ios/chrome/app/startup/app_launch_metrics.h"
#import "ios/chrome/app/task_request_private.h"
#import "ios/chrome/app/unexpected_mode_toast_util.h"
#import "ios/chrome/browser/credential_exchange/model/credential_import_manager_swift.h"
#import "ios/chrome/browser/credential_provider/model/features.h"
#import "ios/chrome/browser/intents/model/intent_type.h"
#import "ios/chrome/browser/intents/model/intents_constants.h"
#import "ios/chrome/browser/intents/model/user_activity_browser_agent.h"
#import "ios/chrome/browser/intents/model/user_activity_compatibility_util.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace {

// Possible user activity types, including invalid ones.
enum class UserActivityType {
  kInvalid,
  kHandoff,
  kSpotlight,
  kSearchInChrome,
  kOpenInChrome,
  kOpenInIncognito,
  kAddBookmarkToChrome,
  kAddReadingListItemToChrome,
  kOpenLatestTab,
  kOpenReadingList,
  kOpenBookmarks,
  kOpenRecentTabs,
  kOpenTabGrid,
  kVoiceSearch,
  kOpenNewTab,
  kPlayDinoGame,
  kSetChromeDefaultBrowser,
  kViewHistory,
  kOpenNewIncognitoTab,
  kManagePaymentMethods,
  kRunSafetyCheck,
  kManagePasswords,
  kManageSettings,
  kOpenLensFromIntents,
  kClearBrowsingData,
  kCredentialExchange,
};

// Maps user activity type string to user activity items.
struct UserActivityMapping {
  NSString* activity_type_string;
  UserActivityType activity_type;
};

// Records metrics of siri shortcuts with type `intent_type`.
void RecordMetricsForSiriShortcut(IntentType intent_type) {
  base::UmaHistogramEnumeration(kAppLaunchSource,
                                AppLaunchSource::SIRI_SHORTCUT);
  base::UmaHistogramEnumeration("IOS.Spotlight.LaunchedIntentType",
                                intent_type);
}

// Records metrics for handle a user activity of `user_activity_type`.
void RecordMetrics(UserActivityType user_activity_type,
                   NSUserActivity* user_activity) {
  switch (user_activity_type) {
    case UserActivityType::kHandoff:
      base::UmaHistogramEnumeration(kAppLaunchSource, AppLaunchSource::HANDOFF);
      break;
    case UserActivityType::kSpotlight: {
      NSString* item_id = [user_activity.userInfo
          objectForKey:CSSearchableItemActivityIdentifier];
      spotlight::Domain domain = spotlight::SpotlightDomainFromString(item_id);
      base::UmaHistogramEnumeration("IOS.Spotlight.Origin", domain);

      base::UmaHistogramEnumeration(kAppLaunchSource,
                                    AppLaunchSource::SPOTLIGHT_CHROME);
      break;
    }
    case UserActivityType::kSearchInChrome:
      RecordMetricsForSiriShortcut(IntentType::kSearchInChrome);
      base::RecordAction(
          base::UserMetricsAction("IOSLaunchedBySearchInChromeIntent"));
      break;
    case UserActivityType::kOpenInChrome:
      RecordMetricsForSiriShortcut(IntentType::kOpenInChrome);
      base::RecordAction(
          base::UserMetricsAction("IOSLaunchedByOpenInChromeIntent"));
      break;
    case UserActivityType::kOpenInIncognito:
      RecordMetricsForSiriShortcut(IntentType::kOpenInIncognito);
      base::RecordAction(
          base::UserMetricsAction("IOSLaunchedByOpenInIncognitoIntent"));
      break;
    case UserActivityType::kAddBookmarkToChrome:
      base::UmaHistogramEnumeration(kAppLaunchSource,
                                    AppLaunchSource::SIRI_SHORTCUT);
      base::RecordAction(
          base::UserMetricsAction("IOSLaunchedByAddBookmarkToChromeIntent"));
      break;
    case UserActivityType::kAddReadingListItemToChrome:
      base::UmaHistogramEnumeration(kAppLaunchSource,
                                    AppLaunchSource::SIRI_SHORTCUT);
      base::RecordAction(base::UserMetricsAction(
          "IOSLaunchedByAddReadingListItemToChromeIntent"));
      break;
    case UserActivityType::kOpenLatestTab:
      RecordMetricsForSiriShortcut(IntentType::kOpenLatestTab);
      break;
    case UserActivityType::kOpenReadingList:
      RecordMetricsForSiriShortcut(IntentType::kOpenReadingList);
      break;
    case UserActivityType::kOpenBookmarks:
      RecordMetricsForSiriShortcut(IntentType::kOpenBookmarks);
      break;
    case UserActivityType::kOpenRecentTabs:
      RecordMetricsForSiriShortcut(IntentType::kOpenRecentTabs);
      break;
    case UserActivityType::kOpenTabGrid:
      RecordMetricsForSiriShortcut(IntentType::kOpenTabGrid);
      break;
    case UserActivityType::kVoiceSearch:
      RecordMetricsForSiriShortcut(IntentType::kOpenVoiceSearch);
      break;
    case UserActivityType::kOpenNewTab:
      RecordMetricsForSiriShortcut(IntentType::kOpenNewTab);
      break;
    case UserActivityType::kPlayDinoGame:
      RecordMetricsForSiriShortcut(IntentType::kPlayDinoGame);
      break;
    case UserActivityType::kSetChromeDefaultBrowser:
      RecordMetricsForSiriShortcut(IntentType::kSetDefaultBrowser);
      break;
    case UserActivityType::kViewHistory:
      RecordMetricsForSiriShortcut(IntentType::kViewHistory);
      break;
    case UserActivityType::kOpenNewIncognitoTab:
      RecordMetricsForSiriShortcut(IntentType::kOpenNewIncognitoTab);
      break;
    case UserActivityType::kManagePaymentMethods:
      RecordMetricsForSiriShortcut(IntentType::kManagePaymentMethods);
      break;
    case UserActivityType::kRunSafetyCheck:
      RecordMetricsForSiriShortcut(IntentType::kRunSafetyCheck);
      break;
    case UserActivityType::kManagePasswords:
      RecordMetricsForSiriShortcut(IntentType::kManagePasswords);
      break;
    case UserActivityType::kManageSettings:
      RecordMetricsForSiriShortcut(IntentType::kManageSettings);
      break;
    case UserActivityType::kOpenLensFromIntents:
      RecordMetricsForSiriShortcut(IntentType::kStartLens);
      break;
    case UserActivityType::kClearBrowsingData:
      RecordMetricsForSiriShortcut(IntentType::kClearBrowsingData);
      break;
    case UserActivityType::kCredentialExchange:
      break;
    case UserActivityType::kInvalid:
      break;
  }
}

// Returns the user activity type of `user_activity`.
UserActivityType UserActivityTypeOf(NSUserActivity* user_activity) {
  const UserActivityMapping kUserActivityMap[] = {
      {handoff::kChromeHandoffActivityType, UserActivityType::kHandoff},
      {NSUserActivityTypeBrowsingWeb, UserActivityType::kHandoff},
      {CSSearchableItemActionType, UserActivityType::kSpotlight},
      {kSiriShortcutSearchInChrome, UserActivityType::kSearchInChrome},
      {kSiriShortcutOpenInChrome, UserActivityType::kOpenInChrome},
      {kSiriShortcutOpenInIncognito, UserActivityType::kOpenInIncognito},
      {kSiriShortcutAddBookmarkToChrome,
       UserActivityType::kAddBookmarkToChrome},
      {kSiriShortcutAddReadingListItemToChrome,
       UserActivityType::kAddReadingListItemToChrome},
      {kSiriOpenLatestTab, UserActivityType::kOpenLatestTab},
      {kSiriOpenReadingList, UserActivityType::kOpenReadingList},
      {kSiriOpenBookmarks, UserActivityType::kOpenBookmarks},
      {kSiriOpenRecentTabs, UserActivityType::kOpenRecentTabs},
      {kSiriOpenTabGrid, UserActivityType::kOpenTabGrid},
      {kSiriVoiceSearch, UserActivityType::kVoiceSearch},
      {kSiriOpenNewTab, UserActivityType::kOpenNewTab},
      {kSiriPlayDinoGame, UserActivityType::kPlayDinoGame},
      {kSiriSetChromeDefaultBrowser,
       UserActivityType::kSetChromeDefaultBrowser},
      {kSiriViewHistory, UserActivityType::kViewHistory},
      {kSiriOpenNewIncognitoTab, UserActivityType::kOpenNewIncognitoTab},
      {kSiriManagePaymentMethods, UserActivityType::kManagePaymentMethods},
      {kSiriRunSafetyCheck, UserActivityType::kRunSafetyCheck},
      {kSiriManagePasswords, UserActivityType::kManagePasswords},
      {kSiriManageSettings, UserActivityType::kManageSettings},
      {kSiriOpenLensFromIntents, UserActivityType::kOpenLensFromIntents},
      {kSiriClearBrowsingData, UserActivityType::kClearBrowsingData},
  };

  NSString* activity_type = user_activity.activityType;

  if (@available(iOS 26, *)) {
    if (CredentialExchangeEnabled() &&
        [activity_type isEqualToString:[CredentialImportManager
                                           credentialExchangeActivity]]) {
      return UserActivityType::kCredentialExchange;
    }
  }

  for (const auto& item : kUserActivityMap) {
    if ([activity_type isEqualToString:item.activity_type_string]) {
      return item.activity_type;
    }
  }

  return UserActivityType::kInvalid;
}

}  // namespace

@implementation TaskRequestForUserActivity {
  NSUserActivity* _userActivity;
  UserActivityType _userActivityType;
}

- (instancetype)initWithUserActivity:(NSUserActivity*)userActivity
                          sceneState:(SceneState*)sceneState
                         isColdStart:(BOOL)isColdStart {
  if ((self = [super initWithSceneState:sceneState isColdStart:isColdStart])) {
    _userActivity = userActivity;
    _userActivityType = UserActivityTypeOf(userActivity);
    RecordMetrics(_userActivityType, userActivity);
  }
  return self;
}

- (void)execute {
  if (self.isColdStart) {
    [self executeFromColdStart];
  } else {
    [self executeFromWarmStart];
  }
}

#pragma mark - Private

- (void)executeFromWarmStart {
  SceneState* sceneState = [self sceneStateFromSessionID];
  CHECK(sceneState);
  Browser* browser =
      sceneState.browserProviderInterface.currentBrowserProvider.browser;
  CHECK(browser);

  PrefService* prefs = sceneState.profileState.profile->GetPrefs();
  if (!ProceedWithUserActivity(_userActivity, prefs)) {
    ApplicationModeForTabOpening targetMode =
        IsIncognitoModeForced(prefs) ? ApplicationModeForTabOpening::INCOGNITO
                                     : ApplicationModeForTabOpening::NORMAL;
    ShowToastWhenOpenInUnexpectedMode(sceneState, targetMode);
    return;
  }

  UserActivityBrowserAgent* userActivityBrowserAgent =
      UserActivityBrowserAgent::FromBrowser(browser);
  userActivityBrowserAgent->ContinueUserActivity(_userActivity, YES);
}

- (void)executeFromColdStart {
  // TODO(crbug.com/462018636): Handle cold start with userActivity.
}

@end
