// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/task_request_user_activity.h"

#import <CoreSpotlight/CoreSpotlight.h>
#import <Intents/Intents.h>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/functional/callback_helpers.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/handoff/handoff_utility.h"
#import "components/password_manager/core/browser/ui/password_check_referrer.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/application_delegate/tab_opening.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/spotlight/spotlight_util.h"
#import "ios/chrome/app/startup/app_launch_metrics.h"
#import "ios/chrome/app/task_request_private.h"
#import "ios/chrome/app/unexpected_mode_toast_util.h"
#import "ios/chrome/browser/credential_exchange/model/credential_import_manager_swift.h"
#import "ios/chrome/browser/credential_provider/model/features.h"
#import "ios/chrome/browser/intents/model/intent_type.h"
#import "ios/chrome/browser/intents/model/intents_constants.h"
#import "ios/chrome/browser/intents/model/user_activity_compatibility_util.h"
#import "ios/chrome/browser/lens/ui_bundled/lens_entrypoint.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/reading_list/model/reading_list_browser_agent.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/bookmarks_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_lens_input_selection_command.h"
#import "ios/chrome/browser/shared/public/commands/quick_delete_commands.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/common/intents/AddBookmarkToChromeIntent.h"
#import "ios/chrome/common/intents/AddReadingListItemToChromeIntent.h"
#import "ios/chrome/common/intents/OpenInChromeIncognitoIntent.h"
#import "ios/chrome/common/intents/OpenInChromeIntent.h"
#import "net/base/apple/url_conversions.h"

namespace {

// Possible user activity types, including invalid ones.
// If a new activity type is added, `CompatibleModeForActivityType` in
// user_activity_compatibility_util.mm needs to be updated too.
// LINT.IfChange
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
// LINT.ThenChange(ios/chrome/browser/intents/model/user_activity_compatibility_util.mm)

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

// Returns a completion block that opens the reading list.
void OpenReadingListWithBrowser(base::WeakPtr<Browser> weak_browser) {
  if (Browser* browser = weak_browser.get()) {
    id<BrowserCoordinatorCommands> handler = HandlerForProtocol(
        browser->GetCommandDispatcher(), BrowserCoordinatorCommands);
    [handler showReadingList];
  }
}

// Navigates to the bookmark manager UI.
void OpenBookmarksWithBrowser(base::WeakPtr<Browser> weak_browser) {
  if (Browser* browser = weak_browser.get()) {
    id<BrowserCoordinatorCommands> handler = HandlerForProtocol(
        browser->GetCommandDispatcher(), BrowserCoordinatorCommands);
    [handler showBookmarksManager];
  }
}

// Navigates to the password search UI.
void OpenPasswordSearchWithBrowser(base::WeakPtr<Browser> weak_browser) {
  if (Browser* browser = weak_browser.get()) {
    id<SettingsCommands> handler =
        HandlerForProtocol(browser->GetCommandDispatcher(), SettingsCommands);
    [handler showPasswordSearchPage];
  }
}

// Navigates to the settings UI.
void OpenSettingsWithBrowser(base::WeakPtr<Browser> weak_browser) {
  if (Browser* browser = weak_browser.get()) {
    id<SceneCommands> handler =
        HandlerForProtocol(browser->GetCommandDispatcher(), SceneCommands);
    [handler maybeShowSettingsFromViewController];
  }
}

// Runs the safety check.
void RunSafetyCheckWithBrowser(base::WeakPtr<Browser> weak_browser) {
  if (Browser* browser = weak_browser.get()) {
    id<SettingsCommands> handler =
        HandlerForProtocol(browser->GetCommandDispatcher(), SettingsCommands);
    [handler
        showAndStartSafetyCheckForReferrer:
            password_manager::PasswordCheckReferrer::kSafetyCheckMagicStack];
  }
}

// Navigates to the history UI.
void OpenHistoryWithBrowser(base::WeakPtr<Browser> weak_browser) {
  if (Browser* browser = weak_browser.get()) {
    id<SceneCommands> handler =
        HandlerForProtocol(browser->GetCommandDispatcher(), SceneCommands);
    [handler showHistory];
  }
}

// Navigates to the payment methods settings.
void OpenPaymentMethodsWithBrowser(base::WeakPtr<Browser> weak_browser) {
  if (Browser* browser = weak_browser.get()) {
    id<SettingsCommands> handler =
        HandlerForProtocol(browser->GetCommandDispatcher(), SettingsCommands);
    [handler showCreditCardSettings];
  }
}

// Opens Lens from intents.
void OpenLensFromIntentsWithBrowser(base::WeakPtr<Browser> weak_browser) {
  if (Browser* browser = weak_browser.get()) {
    id<LensCommands> lensHandler =
        HandlerForProtocol(browser->GetCommandDispatcher(), LensCommands);
    OpenLensInputSelectionCommand* command =
        [[OpenLensInputSelectionCommand alloc]
                initWithEntryPoint:LensEntrypoint::Intents
                 presentationStyle:LensInputSelectionPresentationStyle::
                                       SlideFromRight
            presentationCompletion:nil];
    [lensHandler openLensInputSelection:command];
  }
}

// Navigates to the tab grid.
void OpenTabGridWithBrowser(base::WeakPtr<Browser> weak_browser) {
  if (Browser* browser = weak_browser.get()) {
    id<SceneCommands> handler =
        HandlerForProtocol(browser->GetCommandDispatcher(), SceneCommands);
    [handler displayTabGridInMode:TabGridOpeningMode::kDefault];
  }
}

// Opens quick delete to clear browsing data.
void OpenClearBrowsingDataWithBrowser(base::WeakPtr<Browser> weak_browser) {
  if (Browser* browser = weak_browser.get()) {
    id<QuickDeleteCommands> handler = HandlerForProtocol(
        browser->GetCommandDispatcher(), QuickDeleteCommands);
    [handler showQuickDeleteAndCanPerformRadialWipeAnimation:YES];
  }
}

// Adds bookmarks to Chrome.
void AddBookmarkToChromeWithIntent(INIntent* intent,
                                   base::WeakPtr<Browser> weak_browser) {
  if (Browser* browser = weak_browser.get()) {
    AddBookmarkToChromeIntent* bookmark_intent =
        base::apple::ObjCCastStrict<AddBookmarkToChromeIntent>(intent);
    if (bookmark_intent && bookmark_intent.url &&
        bookmark_intent.url.count > 0) {
      id<BookmarksCommands> handler = HandlerForProtocol(
          browser->GetCommandDispatcher(), BookmarksCommands);
      [handler addBookmarks:bookmark_intent.url];
    }
  }
}

// Adds url to reading list.
void AddReadingListToChromeWithIntent(INIntent* intent,
                                      base::WeakPtr<Browser> weak_browser) {
  if (Browser* browser = weak_browser.get()) {
    AddReadingListItemToChromeIntent* typed_intent =
        base::apple::ObjCCastStrict<AddReadingListItemToChromeIntent>(intent);
    if (typed_intent && typed_intent.url && typed_intent.url.count > 0) {
      ReadingListBrowserAgent* readingListBrowserAgent =
          ReadingListBrowserAgent::FromBrowser(browser);
      readingListBrowserAgent->BulkAddURLsToReadingListWithViewSnackbar(
          typed_intent.url);
    }
  }
}

std::vector<GURL> GURLVectorWithNSURLArray(NSArray<NSURL*>* intent_urls) {
  if (!intent_urls) {
    return {};
  }
  std::vector<GURL> urls;
  urls.reserve(intent_urls.count);
  for (NSURL* intent_url in intent_urls) {
    if (GURL url = net::GURLWithNSURL(intent_url); url.is_valid()) {
      urls.push_back(std::move(url));
    }
  }
  return urls;
}

// Returns the list of URLs from an `OpenInChromeIncognitoIntent`.
std::vector<GURL> GetURLsFromOpenInIncognitoIntent(INIntent* intent) {
  OpenInChromeIncognitoIntent* incognito_intent =
      base::apple::ObjCCastStrict<OpenInChromeIncognitoIntent>(intent);
  return GURLVectorWithNSURLArray(incognito_intent.url);
}

// Returns the list of URLs from an `OpenInChromeIntent`.
std::vector<GURL> GetURLsFromOpenInChromeIntent(INIntent* intent) {
  OpenInChromeIntent* typed_intent =
      base::apple::ObjCCastStrict<OpenInChromeIntent>(intent);
  return GURLVectorWithNSURLArray(typed_intent.url);
}

}  // namespace

@implementation TaskRequestForUserActivity {
  NSUserActivity* _userActivity;
  UserActivityType _userActivityType;
  ApplicationModeForTabOpening _targetMode;
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
  // Ignore invalid user activities.
  if (_userActivityType == UserActivityType::kInvalid) {
    return;
  }
  SceneState* sceneState = [self sceneStateFromSessionID];
  CHECK(sceneState);
  Browser* browser =
      sceneState.browserProviderInterface.currentBrowserProvider.browser;
  CHECK(browser);

  PrefService* prefs = sceneState.profileState.profile->GetPrefs();
  _targetMode = IsIncognitoModeForced(prefs)
                    ? ApplicationModeForTabOpening::INCOGNITO
                    : ApplicationModeForTabOpening::NORMAL;
  if (!ProceedWithUserActivity(_userActivity, prefs)) {
    ShowToastWhenOpenInUnexpectedMode(sceneState, _targetMode);
    return;
  }

  [self handleUserActivityWithSceneState:sceneState];
}

#pragma mark - Private

- (void)handleUserActivityWithSceneState:(SceneState*)sceneState {
  std::vector<GURL> webpageGURLs;
  ProceduralBlock completion = nil;
  id<TabOpening> tabOpener = sceneState.controller;
  Browser* browser =
      sceneState.browserProviderInterface.currentBrowserProvider.browser;
  Browser* mainBrowser =
      sceneState.browserProviderInterface.mainBrowserProvider.browser;

  switch (_userActivityType) {
    case UserActivityType::kHandoff:
      // TODO(crbug.com/492115056): Add implementation.
      break;
    case UserActivityType::kSpotlight:
      // TODO(crbug.com/492115056): Add implementation.
      break;
    case UserActivityType::kSearchInChrome:
      // TODO(crbug.com/492115056): Add implementation.
      break;
    case UserActivityType::kOpenInChrome:
      webpageGURLs =
          GetURLsFromOpenInChromeIntent(_userActivity.interaction.intent);
      break;
    case UserActivityType::kOpenInIncognito:
      webpageGURLs =
          GetURLsFromOpenInIncognitoIntent(_userActivity.interaction.intent);
      _targetMode = ApplicationModeForTabOpening::INCOGNITO;
      break;
    case UserActivityType::kAddBookmarkToChrome:
      completion = base::CallbackToBlock(base::BindRepeating(
          &AddBookmarkToChromeWithIntent, _userActivity.interaction.intent,
          browser->AsWeakPtr()));
      webpageGURLs.push_back(GURL(kChromeUINewTabURL));
      break;
    case UserActivityType::kAddReadingListItemToChrome:
      completion = base::CallbackToBlock(base::BindRepeating(
          &AddReadingListToChromeWithIntent, _userActivity.interaction.intent,
          browser->AsWeakPtr()));
      webpageGURLs.push_back(GURL(kChromeUINewTabURL));
      break;
    case UserActivityType::kOpenLatestTab:
      // TODO(crbug.com/492115056): Add implementation.
      break;
    case UserActivityType::kOpenReadingList:
      completion = base::CallbackToBlock(base::BindRepeating(
          &OpenReadingListWithBrowser, browser->AsWeakPtr()));
      webpageGURLs.push_back(GURL(kChromeUINewTabURL));
      break;
    case UserActivityType::kOpenBookmarks:
      completion = base::CallbackToBlock(
          base::BindRepeating(&OpenBookmarksWithBrowser, browser->AsWeakPtr()));
      webpageGURLs.push_back(GURL(kChromeUINewTabURL));
      break;
    case UserActivityType::kOpenRecentTabs:
      // TODO(crbug.com/492115056): Add implementation.
      break;
    case UserActivityType::kOpenTabGrid:
      completion = base::CallbackToBlock(
          base::BindRepeating(&OpenTabGridWithBrowser, browser->AsWeakPtr()));
      break;
    case UserActivityType::kVoiceSearch:
      // TODO(crbug.com/492115056): Add implementation.
      break;
    case UserActivityType::kOpenNewTab:
      webpageGURLs.push_back(GURL(kChromeUINewTabURL));
      break;
    case UserActivityType::kPlayDinoGame:
      webpageGURLs.push_back(GURL(kChromeDinoGameURL));
      break;
    case UserActivityType::kSetChromeDefaultBrowser:
      // TODO(crbug.com/492115056): Add implementation.
      break;
    case UserActivityType::kViewHistory:
      completion = base::CallbackToBlock(
          base::BindRepeating(&OpenHistoryWithBrowser, browser->AsWeakPtr()));
      webpageGURLs.push_back(GURL(kChromeUINewTabURL));
      break;
    case UserActivityType::kOpenNewIncognitoTab:
      webpageGURLs.push_back(GURL(kChromeUINewTabURL));
      _targetMode = ApplicationModeForTabOpening::INCOGNITO;
      break;
    case UserActivityType::kManagePaymentMethods:
      completion = base::CallbackToBlock(base::BindRepeating(
          &OpenPaymentMethodsWithBrowser, browser->AsWeakPtr()));
      webpageGURLs.push_back(GURL(kChromeUINewTabURL));
      break;
    case UserActivityType::kRunSafetyCheck:
      completion = base::CallbackToBlock(base::BindRepeating(
          &RunSafetyCheckWithBrowser, browser->AsWeakPtr()));
      webpageGURLs.push_back(GURL(kChromeUINewTabURL));
      break;
    case UserActivityType::kManagePasswords:
      completion = base::CallbackToBlock(base::BindRepeating(
          &OpenPasswordSearchWithBrowser, browser->AsWeakPtr()));
      webpageGURLs.push_back(GURL(kChromeUINewTabURL));
      break;
    case UserActivityType::kManageSettings:
      completion = base::CallbackToBlock(
          base::BindRepeating(&OpenSettingsWithBrowser, browser->AsWeakPtr()));
      webpageGURLs.push_back(GURL(kChromeUINewTabURL));
      break;
    case UserActivityType::kOpenLensFromIntents:
      completion = base::CallbackToBlock(base::BindRepeating(
          &OpenLensFromIntentsWithBrowser, browser->AsWeakPtr()));
      webpageGURLs.push_back(GURL(kChromeUINewTabURL));
      break;
    case UserActivityType::kClearBrowsingData:
      completion = base::CallbackToBlock(base::BindRepeating(
          &OpenClearBrowsingDataWithBrowser, mainBrowser->AsWeakPtr()));
      webpageGURLs.push_back(GURL(kChromeUINewTabURL));
      break;
    case UserActivityType::kCredentialExchange:
      // TODO(crbug.com/492115056): Add implementation.
      break;
    case UserActivityType::kInvalid:
      NOTREACHED();
  }

  // Handle the case where no URLs need to be opened but we have a completion
  // block.
  if (webpageGURLs.empty()) {
    if (completion) {
      id<SceneCommands> handler =
          HandlerForProtocol(browser->GetCommandDispatcher(), SceneCommands);
      [handler dismissModalDialogsWithCompletion:completion];
    }
    return;
  }

  // Handle the case where multiple URLS need to be opened.
  if (webpageGURLs.size() > 1) {
    [tabOpener
        dismissModalsAndOpenMultipleTabsWithURLs:webpageGURLs
                                 inIncognitoMode:
                                     (_targetMode ==
                                      ApplicationModeForTabOpening::INCOGNITO)
                                  dismissOmnibox:YES
                                      completion:completion];
  } else if (webpageGURLs.size() == 1) {
    const GURL& webpageGURL = webpageGURLs[0];
    if (!webpageGURL.is_valid()) {
      return;
    }

    // TODO(crbug.com/462018636): Find a centralized solition for dino game
    // intents. Potentially move this logic inside TabOpener.
    UrlLoadParams params = UrlLoadParams::InNewTab(webpageGURL);
    if (_userActivityType == UserActivityType::kPlayDinoGame) {
      params.web_params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
    }

    [tabOpener dismissModalsAndMaybeOpenSelectedTabInMode:_targetMode
                                        withUrlLoadParams:params
                                           dismissOmnibox:YES
                                               completion:completion];
  }

  // TODO(crbug.com/492115056): In new implementation if an action is allowed
  // when there is an enterprise policy (incognito forced or incognito disabled)
  // a toast is not displayed, this is different compared to old implementation.
  // Confirm the correct behavior and update code accordingly.
}

@end
