// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_mediator.h"

#import "base/ios/ios_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/common/bookmark_pref_names.h"
#import "components/browsing_data/core/browsing_data_utils.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/language/ios/browser/ios_language_detection_tab_helper.h"
#import "components/language/ios/browser/ios_language_detection_tab_helper_observer_bridge.h"
#import "components/password_manager/core/browser/manage_passwords_referrer.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/profile_metrics/browser_profile_type.h"
#import "components/reading_list/core/reading_list_model.h"
#import "components/reading_list/ios/reading_list_model_bridge_observer.h"
#import "components/search_engines/template_url_service.h"
#import "components/supervised_user/core/common/features.h"
#import "components/supervised_user/core/common/supervised_user_constants.h"
#import "components/sync/service/sync_service.h"
#import "components/translate/core/browser/translate_manager.h"
#import "components/translate/core/browser/translate_prefs.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/commerce/model/push_notification/push_notification_feature.h"
#import "ios/chrome/browser/default_browser/model/default_browser_interest_signals.h"
#import "ios/chrome/browser/find_in_page/model/abstract_find_tab_helper.h"
#import "ios/chrome/browser/follow/model/follow_browser_agent.h"
#import "ios/chrome/browser/follow/model/follow_menu_updater.h"
#import "ios/chrome/browser/follow/model/follow_tab_helper.h"
#import "ios/chrome/browser/follow/model/follow_util.h"
#import "ios/chrome/browser/intents/intents_donation_helper.h"
#import "ios/chrome/browser/iph_for_new_chrome_user/model/tab_based_iph_browser_agent.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_availability.h"
#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_recorder.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter_observer_bridge.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/policy/ui_bundled/user_policy_util.h"
#import "ios/chrome/browser/reading_list/model/offline_url_utils.h"
#import "ios/chrome/browser/search_engines/model/search_engine_observer_bridge.h"
#import "ios/chrome/browser/search_engines/model/search_engines_util.h"
#import "ios/chrome/browser/settings/model/sync/utils/identity_error_util.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_commands.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/bookmarks_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/find_in_page_commands.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/overflow_menu_customization_commands.h"
#import "ios/chrome/browser/shared/public/commands/page_info_commands.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/price_notifications_commands.h"
#import "ios/chrome/browser/shared/public/commands/quick_delete_commands.h"
#import "ios/chrome/browser/shared/public/commands/reading_list_add_command.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/text_zoom_commands.h"
#import "ios/chrome/browser/shared/public/commands/whats_new_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_capabilities.h"
#import "ios/chrome/browser/translate/model/chrome_ios_translate_client.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/destination_usage_history/constants.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/destination_usage_history/destination_usage_history.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/feature_flags.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_constants.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_metrics.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_orderer.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_swift.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_utils.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/features.h"
#import "ios/chrome/browser/ui/sharing/sharing_params.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_utils.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_util.h"
#import "ios/chrome/browser/web/model/font_size/font_size_tab_helper.h"
#import "ios/chrome/browser/web/model/web_navigation_browser_agent.h"
#import "ios/chrome/browser/window_activities/model/window_activity_helpers.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_api.h"
#import "ios/web/common/user_agent.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

using base::RecordAction;
using base::UmaHistogramEnumeration;
using base::UserMetricsAction;
using experimental_flags::IsSpotlightDebuggingEnabled;

namespace {

// Approximate number of visible page actions by default.
const unsigned int kDefaultVisiblePageActionCount = 3u;

// Struct used to count and store the number of active WhatsNew badges,
// as the FET does not support showing multiple badges for the same FET feature
// at the same time.
struct WhatsNewActiveMenusData : public base::SupportsUserData::Data {
  // The number of active menus.
  int activeMenus = 0;

  // Key to use for this type in SupportsUserData
  static constexpr char key[] = "WhatsNewActiveMenusData";
};

typedef void (^Handler)(void);

OverflowMenuFooter* CreateOverflowMenuManagedFooter(
    int nameID,
    int linkID,
    NSString* accessibilityIdentifier,
    NSString* imageName,
    Handler handler) {
  NSString* name = l10n_util::GetNSString(nameID);
  NSString* link = l10n_util::GetNSString(linkID);
  return [[OverflowMenuFooter alloc] initWithName:name
                                             link:link
                                            image:[UIImage imageNamed:imageName]
                          accessibilityIdentifier:accessibilityIdentifier
                                          handler:handler];
}

}  // namespace

@interface OverflowMenuMediator () <BookmarkModelBridgeObserver,
                                    CRWWebStateObserver,
                                    FollowMenuUpdater,
                                    IOSLanguageDetectionTabHelperObserving,
                                    OverflowMenuDestinationProvider,
                                    OverlayPresenterObserving,
                                    PrefObserverDelegate,
                                    ReadingListModelBridgeObserver,
                                    SearchEngineObserving,
                                    WebStateListObserving> {
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;

  // Observer for the content area overlay events
  std::unique_ptr<OverlayPresenterObserver> _overlayPresenterObserver;

  // Bridge to register for bookmark changes.
  std::unique_ptr<BookmarkModelBridge> _bookmarkModelBridge;

  // Bridge to register for reading list model changes.
  std::unique_ptr<ReadingListModelBridge> _readingListModelBridge;

  // Bridge to get notified of the language detection event.
  std::unique_ptr<language::IOSLanguageDetectionTabHelperObserverBridge>
      _iOSLanguageDetectionTabHelperObserverBridge;

  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  std::unique_ptr<PrefChangeRegistrar> _prefChangeRegistrar;
  // Search engine observer.
  std::unique_ptr<SearchEngineObserverBridge> _searchEngineObserver;
}

// The current web state.
@property(nonatomic, assign) web::WebState* webState;

// Whether or not the menu has been dismissed. Sometimes, the menu takes some
// time to dismiss after requesting dismissal, leading to errors were menu
// options are selected twice or at the wrong times (see crbug.com/1500367)
@property(nonatomic, assign) BOOL menuHasBeenDismissed;

// Whether an overlay is currently presented over the web content area.
@property(nonatomic, assign) BOOL webContentAreaShowingOverlay;

// Whether the web content is currently being blocked.
@property(nonatomic, assign) BOOL contentBlocked;

@property(nonatomic, strong) OverflowMenuDestination* bookmarksDestination;
@property(nonatomic, strong) OverflowMenuDestination* downloadsDestination;
@property(nonatomic, strong) OverflowMenuDestination* historyDestination;
@property(nonatomic, strong) OverflowMenuDestination* passwordsDestination;
@property(nonatomic, strong)
    OverflowMenuDestination* priceNotificationsDestination;
@property(nonatomic, strong) OverflowMenuDestination* readingListDestination;
@property(nonatomic, strong) OverflowMenuDestination* recentTabsDestination;
@property(nonatomic, strong) OverflowMenuDestination* settingsDestination;
@property(nonatomic, strong) OverflowMenuDestination* siteInfoDestination;
@property(nonatomic, strong) OverflowMenuDestination* whatsNewDestination;
@property(nonatomic, strong)
    OverflowMenuDestination* spotlightDebuggerDestination;

@property(nonatomic, strong) OverflowMenuActionGroup* appActionsGroup;
@property(nonatomic, strong) OverflowMenuActionGroup* pageActionsGroup;
@property(nonatomic, strong) OverflowMenuActionGroup* helpActionsGroup;
@property(nonatomic, strong) OverflowMenuActionGroup* editActionsGroup;

@property(nonatomic, strong) OverflowMenuAction* reloadAction;
@property(nonatomic, strong) OverflowMenuAction* stopLoadAction;
@property(nonatomic, strong) OverflowMenuAction* openTabAction;
@property(nonatomic, strong) OverflowMenuAction* openIncognitoTabAction;
@property(nonatomic, strong) OverflowMenuAction* openNewWindowAction;

@property(nonatomic, strong) OverflowMenuAction* clearBrowsingDataAction;
@property(nonatomic, strong) OverflowMenuAction* followAction;
@property(nonatomic, strong) OverflowMenuAction* addBookmarkAction;
@property(nonatomic, strong) OverflowMenuAction* editBookmarkAction;
@property(nonatomic, strong) OverflowMenuAction* readLaterAction;
@property(nonatomic, strong) OverflowMenuAction* translateAction;
@property(nonatomic, strong) OverflowMenuAction* requestDesktopAction;
@property(nonatomic, strong) OverflowMenuAction* requestMobileAction;
@property(nonatomic, strong) OverflowMenuAction* findInPageAction;
@property(nonatomic, strong) OverflowMenuAction* textZoomAction;

@property(nonatomic, strong) OverflowMenuAction* reportIssueAction;
@property(nonatomic, strong) OverflowMenuAction* helpAction;
@property(nonatomic, strong) OverflowMenuAction* shareChromeAction;

@property(nonatomic, strong) OverflowMenuAction* editActionsAction;
@property(nonatomic, strong) OverflowMenuAction* lensOverlayAction;

@end

@implementation OverflowMenuMediator

- (instancetype)init {
  self = [super init];
  if (self) {
    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _overlayPresenterObserver =
        std::make_unique<OverlayPresenterObserverBridge>(self);
  }
  return self;
}

- (void)disconnect {
  // Remove the model link so the other deallocations don't update the model
  // and thus the UI as the UI is dismissing.
  self.model = nil;
  self.menuOrderer = nil;

  self.webContentAreaOverlayPresenter = nullptr;

  if (_engagementTracker) {
    if (self.readingListDestination.badge != BadgeTypeNone) {
      _engagementTracker->Dismissed(
          feature_engagement::kIPHBadgedReadingListFeature);
    }

    if (self.whatsNewDestination.badge != BadgeTypeNone) {
      // Check if this is the last active menu with WhatsNew badge and dismiss
      // the FET if so.
      WhatsNewActiveMenusData* data = static_cast<WhatsNewActiveMenusData*>(
          _engagementTracker->GetUserData(WhatsNewActiveMenusData::key));
      if (data) {
        data->activeMenus--;
        if (data->activeMenus <= 0) {
          _engagementTracker->Dismissed(
              feature_engagement::kIPHWhatsNewUpdatedFeature);
          _engagementTracker->RemoveUserData(WhatsNewActiveMenusData::key);
        }
      } else {
        _engagementTracker->Dismissed(
            feature_engagement::kIPHWhatsNewUpdatedFeature);
      }
    }

    _engagementTracker = nullptr;
  }

  self.followBrowserAgent = nullptr;

  self.webState = nullptr;
  self.webStateList = nullptr;

  self.bookmarkModel = nullptr;
  self.readingListModel = nullptr;
  self.profilePrefs = nullptr;
  self.localStatePrefs = nullptr;

  self.syncService = nullptr;
  _searchEngineObserver.reset();
}

#pragma mark - Property getters/setters

- (void)setModel:(OverflowMenuModel*)model {
  _model = model;
  if (_model) {
    [self initializeModel];
    [self updateModelItemsState];
    // Any state that is required for re-ordering the menu overall (e.g. badges)
    // must be ready by this point. After this, the only order-based changes
    // that will be observed are those that show/hide whole destinations.
    [_menuOrderer reorderDestinationsForInitialMenu];
    [self updateModel];
  }
}

- (void)setMenuOrderer:(OverflowMenuOrderer*)menuOrderer {
  if (self.menuOrderer.destinationProvider == self) {
    self.menuOrderer.destinationProvider = nil;
  }
  if (self.menuOrderer.actionProvider == self) {
    self.menuOrderer.actionProvider = nil;
  }
  _menuOrderer = menuOrderer;
  self.menuOrderer.destinationProvider = self;
  self.menuOrderer.actionProvider = self;

  [self updateModel];
}

- (void)setIsIncognito:(BOOL)isIncognito {
  _isIncognito = isIncognito;
  [self updateModel];
}

- (void)setWebState:(web::WebState*)webState {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserver.get());

    if (self.followAction) {
      FollowTabHelper* followTabHelper =
          FollowTabHelper::FromWebState(_webState);
      if (followTabHelper) {
        followTabHelper->RemoveFollowMenuUpdater();
      }
    }
  }

  _webState = webState;

  if (_webState) {
    [self updateModel];
    _webState->AddObserver(_webStateObserver.get());

    // Observe the language::IOSLanguageDetectionTabHelper for `_webState`.
    _iOSLanguageDetectionTabHelperObserverBridge =
        std::make_unique<language::IOSLanguageDetectionTabHelperObserverBridge>(
            language::IOSLanguageDetectionTabHelper::FromWebState(_webState),
            self);

    FollowTabHelper* followTabHelper = FollowTabHelper::FromWebState(_webState);
    if (followTabHelper) {
      followTabHelper->SetFollowMenuUpdater(self);
    }
  }
}

- (void)setWebStateList:(WebStateList*)webStateList {
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());

    _iOSLanguageDetectionTabHelperObserverBridge.reset();
  }

  _webStateList = webStateList;

  self.webState = (_webStateList) ? _webStateList->GetActiveWebState() : nil;

  if (_webStateList) {
    _webStateList->AddObserver(_webStateListObserver.get());
  }
}

- (void)setBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel {
  _bookmarkModelBridge.reset();

  _bookmarkModel = bookmarkModel;

  if (bookmarkModel) {
    _bookmarkModelBridge =
        std::make_unique<BookmarkModelBridge>(self, bookmarkModel);
  }

  [self updateModel];
}

- (void)setReadingListModel:(ReadingListModel*)readingListModel {
  _readingListModelBridge.reset();

  _readingListModel = readingListModel;

  if (readingListModel) {
    _readingListModelBridge =
        std::make_unique<ReadingListModelBridge>(self, readingListModel);
  }

  [self updateModel];
}

- (void)setProfilePrefs:(PrefService*)profilePrefs {
  _prefObserverBridge.reset();
  _prefChangeRegistrar.reset();

  _profilePrefs = profilePrefs;

  if (_profilePrefs) {
    _prefChangeRegistrar = std::make_unique<PrefChangeRegistrar>();
    _prefChangeRegistrar->Init(profilePrefs);
    _prefObserverBridge.reset(new PrefObserverBridge(self));
    _prefObserverBridge->ObserveChangesForPreference(
        bookmarks::prefs::kEditBookmarksEnabled, _prefChangeRegistrar.get());
  }
}

- (void)setEngagementTracker:(feature_engagement::Tracker*)engagementTracker {
  _engagementTracker = engagementTracker;

  if (!engagementTracker) {
    return;
  }

  [self updateModel];
}

- (void)setWebContentAreaOverlayPresenter:
    (OverlayPresenter*)webContentAreaOverlayPresenter {
  if (_webContentAreaOverlayPresenter) {
    _webContentAreaOverlayPresenter->RemoveObserver(
        _overlayPresenterObserver.get());
    self.webContentAreaShowingOverlay = NO;
  }

  _webContentAreaOverlayPresenter = webContentAreaOverlayPresenter;

  if (_webContentAreaOverlayPresenter) {
    _webContentAreaOverlayPresenter->AddObserver(
        _overlayPresenterObserver.get());
    self.webContentAreaShowingOverlay =
        _webContentAreaOverlayPresenter->IsShowingOverlayUI();
  }
}

- (void)setWebContentAreaShowingOverlay:(BOOL)webContentAreaShowingOverlay {
  if (_webContentAreaShowingOverlay == webContentAreaShowingOverlay)
    return;
  _webContentAreaShowingOverlay = webContentAreaShowingOverlay;
  [self updateModel];
}

- (void)setSyncService:(syncer::SyncService*)syncService {
  _syncService = syncService;

  if (!syncService) {
    return;
  }

  [self updateModel];
}

- (void)setTemplateURLService:(TemplateURLService*)templateURLService {
  _templateURLService = templateURLService;
  if (_templateURLService) {
    _searchEngineObserver =
        std::make_unique<SearchEngineObserverBridge>(self, _templateURLService);
    [self searchEngineChanged];
  } else {
    _searchEngineObserver.reset();
  }
}

#pragma mark - Model Creation

- (void)initializeModel {
  __weak __typeof(self) weakSelf = self;

  // Bookmarks destination.
  self.bookmarksDestination = [self newBookmarksDestination];

  // Downloads destination.
  self.downloadsDestination = [self newDownloadsDestination];

  // History destination.
  self.historyDestination = [self newHistoryDestination];

  // Passwords destination.
  self.passwordsDestination = [self newPasswordsDestination];

  // Price Tracking destination.
  self.priceNotificationsDestination = [self newPriceNotificationsDestination];

  // Reading List destination.
  self.readingListDestination = [self newReadingListDestination];

  // Recent Tabs destination.
  self.recentTabsDestination = [self newRecentTabsDestination];

  // Settings destination.
  self.settingsDestination = [self newSettingsDestination];

  self.spotlightDebuggerDestination = [self newSpotlightDebuggerDestination];

  // WhatsNew destination.
  self.whatsNewDestination = [self newWhatsNewDestination];

  // Site Info destination.
  self.siteInfoDestination = [self newSiteInfoDestination];

  [self logTranslateAvailability];

  self.reloadAction =
      [self createOverflowMenuActionWithNameID:IDS_IOS_TOOLS_MENU_RELOAD
                                    actionType:overflow_menu::ActionType::Reload
                                    symbolName:kArrowClockWiseSymbol
                                  systemSymbol:NO
                              monochromeSymbol:NO
                               accessibilityID:kToolsMenuReload
                                  hideItemText:nil
                                       handler:^{
                                         [weakSelf reload];
                                       }];

  self.stopLoadAction =
      [self createOverflowMenuActionWithNameID:IDS_IOS_TOOLS_MENU_STOP
                                    actionType:overflow_menu::ActionType::Reload
                                    symbolName:kXMarkSymbol
                                  systemSymbol:YES
                              monochromeSymbol:NO
                               accessibilityID:kToolsMenuStop
                                  hideItemText:nil
                                       handler:^{
                                         [weakSelf stopLoading];
                                       }];

  self.openTabAction =
      [self createOverflowMenuActionWithNameID:IDS_IOS_TOOLS_MENU_NEW_TAB
                                    actionType:overflow_menu::ActionType::NewTab
                                    symbolName:kPlusInCircleSymbol
                                  systemSymbol:YES
                              monochromeSymbol:NO
                               accessibilityID:kToolsMenuNewTabId
                                  hideItemText:nil
                                       handler:^{
                                         [weakSelf openTab];
                                       }];

  self.openIncognitoTabAction = [self
      createOverflowMenuActionWithNameID:IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB
                              actionType:overflow_menu::ActionType::
                                             NewIncognitoTab
                              symbolName:kIncognitoSymbol
                            systemSymbol:NO
                        monochromeSymbol:NO
                         accessibilityID:kToolsMenuNewIncognitoTabId
                            hideItemText:nil
                                 handler:^{
                                   [weakSelf openIncognitoTab];
                                 }];

  self.openNewWindowAction = [self
      createOverflowMenuActionWithNameID:IDS_IOS_TOOLS_MENU_NEW_WINDOW
                              actionType:overflow_menu::ActionType::NewWindow
                              symbolName:kNewWindowActionSymbol
                            systemSymbol:YES
                        monochromeSymbol:NO
                         accessibilityID:kToolsMenuNewWindowId
                            hideItemText:nil
                                 handler:^{
                                   [weakSelf openNewWindow];
                                 }];

  self.clearBrowsingDataAction = [self newClearBrowsingDataAction];

  if (GetFollowActionState(self.webState) != FollowActionStateHidden) {
    OverflowMenuAction* action = [self newFollowAction];

    action.enabled = NO;
    self.followAction = action;
  }

  self.addBookmarkAction = [self newAddBookmarkAction];

  NSString* editBookmarkHideItemText =
      l10n_util::GetNSString(IDS_IOS_OVERFLOW_MENU_HIDE_ACTION_EDIT_BOOKMARK);
  self.editBookmarkAction = [self
      createOverflowMenuActionWithNameID:IDS_IOS_TOOLS_MENU_EDIT_BOOKMARK
                              actionType:overflow_menu::ActionType::Bookmark
                              symbolName:kEditActionSymbol
                            systemSymbol:YES
                        monochromeSymbol:NO
                         accessibilityID:kToolsMenuEditBookmark
                            hideItemText:editBookmarkHideItemText
                                 handler:^{
                                   [weakSelf addOrEditBookmark];
                                 }];

  self.readLaterAction = [self newReadLaterAction];

  self.translateAction = [self newTranslateAction];

  self.requestDesktopAction = [self newRequestDesktopAction];

  NSString* requestMobileHideItemText =
      l10n_util::GetNSString(IDS_IOS_OVERFLOW_MENU_HIDE_ACTION_MOBILE_SITE);
  self.requestMobileAction = [self
      createOverflowMenuActionWithNameID:IDS_IOS_TOOLS_MENU_REQUEST_MOBILE_SITE
                              actionType:overflow_menu::ActionType::DesktopSite
                              symbolName:kIPhoneSymbol
                            systemSymbol:YES
                        monochromeSymbol:YES
                         accessibilityID:kToolsMenuRequestMobileId
                            hideItemText:requestMobileHideItemText
                                 handler:^{
                                   [weakSelf requestMobileSite];
                                 }];

  self.findInPageAction = [self newFindInPageAction];

  self.textZoomAction = [self newTextZoomAction];

  self.reportIssueAction =
      [self createOverflowMenuActionWithNameID:IDS_IOS_OPTIONS_REPORT_AN_ISSUE
                                    actionType:overflow_menu::ActionType::
                                                   ReportAnIssue
                                    symbolName:kWarningSymbol
                                  systemSymbol:YES
                              monochromeSymbol:NO
                               accessibilityID:kToolsMenuReportAnIssueId
                                  hideItemText:nil
                                       handler:^{
                                         [weakSelf reportAnIssue];
                                       }];

  self.helpAction =
      [self createOverflowMenuActionWithNameID:IDS_IOS_TOOLS_MENU_HELP_MOBILE
                                    actionType:overflow_menu::ActionType::Help
                                    symbolName:kHelpSymbol
                                  systemSymbol:YES
                              monochromeSymbol:NO
                               accessibilityID:kToolsMenuHelpId
                                  hideItemText:nil
                                       handler:^{
                                         [weakSelf openHelp];
                                       }];

  self.shareChromeAction = [self
      createOverflowMenuActionWithNameID:IDS_IOS_OVERFLOW_MENU_SHARE_CHROME
                              actionType:overflow_menu::ActionType::ShareChrome
                              symbolName:kShareSymbol
                            systemSymbol:YES
                        monochromeSymbol:NO
                         accessibilityID:kToolsMenuShareChromeId
                            hideItemText:nil
                                 handler:^{
                                   [weakSelf shareChromeApp];
                                 }];

  self.editActionsAction = [self
      createOverflowMenuActionWithNameID:IDS_IOS_OVERFLOW_MENU_EDIT_ACTIONS
                              actionType:overflow_menu::ActionType::EditActions
                              symbolName:nil
                            systemSymbol:NO
                        monochromeSymbol:NO
                         accessibilityID:kToolsMenuEditActionsId
                            hideItemText:nil
                                 handler:^{
                                   [weakSelf beginCustomization];
                                 }];
  if (IsLensOverlayAvailable()) {
    self.lensOverlayAction = [self openLensOverlayAction];
  }
  self.editActionsAction.automaticallyUnhighlight = NO;
  self.editActionsAction.useButtonStyling = YES;

  // The app actions vary based on page state, so they are set in
  // `-updateModel`.
  self.appActionsGroup =
      [[OverflowMenuActionGroup alloc] initWithGroupName:@"app_actions"
                                                 actions:@[]
                                                  footer:nil];

  // The page actions vary based on page state, so they are set in
  // `-updateModel`.
  self.pageActionsGroup =
      [[OverflowMenuActionGroup alloc] initWithGroupName:@"page_actions"
                                                 actions:@[]
                                                  footer:nil];
  self.menuOrderer.pageActionsGroup = self.pageActionsGroup;

  // Footer and actions vary based on state, so they're set in -updateModel.
  self.helpActionsGroup =
      [[OverflowMenuActionGroup alloc] initWithGroupName:@"help_actions"
                                                 actions:@[]
                                                  footer:nil];

  self.editActionsGroup = [[OverflowMenuActionGroup alloc]
      initWithGroupName:@"edit_actions"
                actions:@[ self.editActionsAction ]
                 footer:nil];

  self.model.actionGroups = @[
    self.appActionsGroup, self.pageActionsGroup, self.editActionsGroup,
    self.helpActionsGroup
  ];
}

- (OverflowMenuAction*)newFollowAction {
  return [self
      createOverflowMenuActionWithName:l10n_util::GetNSString(
                                           IDS_IOS_TOOLS_MENU_CUSTOMIZE_FOLLOW)
                            actionType:overflow_menu::ActionType::Follow
                            symbolName:kPlusSymbol
                          systemSymbol:YES
                      monochromeSymbol:NO
                       accessibilityID:kToolsMenuFollow
                          hideItemText:
                              l10n_util::GetNSStringF(
                                  IDS_IOS_OVERFLOW_MENU_HIDE_ACTION_FOLLOW, u"")
                               handler:^{
                               }];
}

- (OverflowMenuAction*)newAddBookmarkAction {
  __weak __typeof(self) weakSelf = self;
  NSString* hideItemText = l10n_util::GetNSString(
      IDS_IOS_OVERFLOW_MENU_HIDE_ACTION_ADD_TO_BOOKMARKS);
  return [self
      createOverflowMenuActionWithNameID:IDS_IOS_TOOLS_MENU_ADD_TO_BOOKMARKS
                              actionType:overflow_menu::ActionType::Bookmark
                              symbolName:kAddBookmarkActionSymbol
                            systemSymbol:YES
                        monochromeSymbol:NO
                         accessibilityID:kToolsMenuAddToBookmarks
                            hideItemText:hideItemText
                                 handler:^{
                                   [weakSelf addOrEditBookmark];
                                 }];
}

- (OverflowMenuAction*)openLensOverlayAction {
  __weak __typeof(self) weakSelf = self;
  return [self
      createOverflowMenuActionWithNameID:IDS_IOS_CONTENT_CONTEXT_OPENLENSOVERLAY
                              actionType:overflow_menu::ActionType::LensOverlay
                              symbolName:kCameraLensSymbol
                            systemSymbol:NO
                        monochromeSymbol:NO
                         accessibilityID:kToolsMenuOpenLensOverlay
                            hideItemText:nil
                                 handler:^{
                                   [weakSelf startLensOverlay];
                                 }];
}

- (OverflowMenuAction*)newReadLaterAction {
  __weak __typeof(self) weakSelf = self;
  NSString* hideItemText =
      l10n_util::GetNSString(IDS_IOS_OVERFLOW_MENU_HIDE_ACTION_READING_LIST);
  return [self
      createOverflowMenuActionWithNameID:
          IDS_IOS_CONTENT_CONTEXT_ADDTOREADINGLIST
                              actionType:overflow_menu::ActionType::ReadingList
                              symbolName:kReadLaterActionSymbol
                            systemSymbol:YES
                        monochromeSymbol:NO
                         accessibilityID:kToolsMenuReadLater
                            hideItemText:hideItemText
                                 handler:^{
                                   [weakSelf addToReadingList];
                                 }];
}

- (OverflowMenuAction*)newClearBrowsingDataAction {
  __weak __typeof(self) weakSelf = self;
  NSString* hideItemText = l10n_util::GetNSString(
      IDS_IOS_OVERFLOW_MENU_HIDE_ACTION_CLEAR_BROWSING_DATA);
  return [self
      createOverflowMenuActionWithNameID:IDS_IOS_TOOLS_MENU_CLEAR_BROWSING_DATA
                              actionType:overflow_menu::ActionType::
                                             ClearBrowsingData
                              symbolName:kTrashSymbol
                            systemSymbol:YES
                        monochromeSymbol:NO
                         accessibilityID:kToolsMenuClearBrowsingData
                            hideItemText:hideItemText
                                 handler:^{
                                   [weakSelf openClearBrowsingData];
                                 }];
}

- (OverflowMenuAction*)newTranslateAction {
  __weak __typeof(self) weakSelf = self;
  return [self
      createOverflowMenuActionWithNameID:IDS_IOS_TOOLS_MENU_TRANSLATE
                              actionType:overflow_menu::ActionType::Translate
                              symbolName:kTranslateSymbol
                            systemSymbol:NO
                        monochromeSymbol:NO
                         accessibilityID:kToolsMenuTranslateId
                            hideItemText:
                                l10n_util::GetNSString(
                                    IDS_IOS_OVERFLOW_MENU_HIDE_ACTION_TRANSLATE)
                                 handler:^{
                                   [weakSelf translatePage];
                                 }];
}

- (OverflowMenuAction*)newRequestDesktopAction {
  __weak __typeof(self) weakSelf = self;
  NSString* hideItemText =
      l10n_util::GetNSString(IDS_IOS_OVERFLOW_MENU_HIDE_ACTION_DESKTOP_SITE);
  return [self
      createOverflowMenuActionWithNameID:IDS_IOS_TOOLS_MENU_REQUEST_DESKTOP_SITE
                              actionType:overflow_menu::ActionType::DesktopSite
                              symbolName:kDesktopSymbol
                            systemSymbol:YES
                        monochromeSymbol:YES
                         accessibilityID:kToolsMenuRequestDesktopId
                            hideItemText:hideItemText
                                 handler:^{
                                   [weakSelf requestDesktopSite];
                                 }];
}

- (OverflowMenuAction*)newFindInPageAction {
  __weak __typeof(self) weakSelf = self;
  NSString* hideItemText =
      l10n_util::GetNSString(IDS_IOS_OVERFLOW_MENU_HIDE_ACTION_FIND_IN_PAGE);
  return [self
      createOverflowMenuActionWithNameID:IDS_IOS_TOOLS_MENU_FIND_IN_PAGE
                              actionType:overflow_menu::ActionType::FindInPage
                              symbolName:kFindInPageActionSymbol
                            systemSymbol:YES
                        monochromeSymbol:NO
                         accessibilityID:kToolsMenuFindInPageId
                            hideItemText:hideItemText
                                 handler:^{
                                   [weakSelf openFindInPage];
                                 }];
}

- (OverflowMenuAction*)newTextZoomAction {
  __weak __typeof(self) weakSelf = self;
  return [self
      createOverflowMenuActionWithNameID:IDS_IOS_TOOLS_MENU_TEXT_ZOOM
                              actionType:overflow_menu::ActionType::TextZoom
                              symbolName:kZoomTextActionSymbol
                            systemSymbol:YES
                        monochromeSymbol:NO
                         accessibilityID:kToolsMenuTextZoom
                            hideItemText:
                                l10n_util::GetNSString(
                                    IDS_IOS_OVERFLOW_MENU_HIDE_ACTION_TEXT_ZOOM)
                                 handler:^{
                                   [weakSelf openTextZoom];
                                 }];
}

- (OverflowMenuDestination*)newBookmarksDestination {
  __weak __typeof(self) weakSelf = self;
  return
      [self createOverflowMenuDestination:IDS_IOS_TOOLS_MENU_BOOKMARKS
                              destination:overflow_menu::Destination::Bookmarks
                               symbolName:kBookmarksSymbol
                             systemSymbol:YES
                          accessibilityID:kToolsMenuBookmarksId
                                  handler:^{
                                    [weakSelf openBookmarks];
                                  }];
}

- (OverflowMenuDestination*)newHistoryDestination {
  __weak __typeof(self) weakSelf = self;
  return [self createOverflowMenuDestination:IDS_IOS_TOOLS_MENU_HISTORY
                                 destination:overflow_menu::Destination::History
                                  symbolName:kHistorySymbol
                                systemSymbol:YES
                             accessibilityID:kToolsMenuHistoryId
                                     handler:^{
                                       [weakSelf openHistory];
                                     }];
}

- (OverflowMenuDestination*)newReadingListDestination {
  __weak __typeof(self) weakSelf = self;
  return [self
      createOverflowMenuDestination:IDS_IOS_TOOLS_MENU_READING_LIST
                        destination:overflow_menu::Destination::ReadingList
                         symbolName:kReadingListSymbol
                       systemSymbol:NO
                    accessibilityID:kToolsMenuReadingListId
                            handler:^{
                              [weakSelf openReadingList];
                            }];
}

- (OverflowMenuDestination*)newPasswordsDestination {
  __weak __typeof(self) weakSelf = self;
  return
      [self createOverflowMenuDestination:IDS_IOS_TOOLS_MENU_PASSWORD_MANAGER
                              destination:overflow_menu::Destination::Passwords
                               symbolName:kPasswordSymbol
                             systemSymbol:NO
                          accessibilityID:kToolsMenuPasswordsId
                                  handler:^{
                                    [weakSelf openPasswords];
                                  }];
}

- (OverflowMenuDestination*)newDownloadsDestination {
  __weak __typeof(self) weakSelf = self;
  return
      [self createOverflowMenuDestination:IDS_IOS_TOOLS_MENU_DOWNLOADS
                              destination:overflow_menu::Destination::Downloads
                               symbolName:kDownloadSymbol
                             systemSymbol:YES
                          accessibilityID:kToolsMenuDownloadsId
                                  handler:^{
                                    [weakSelf openDownloads];
                                  }];
}

- (OverflowMenuDestination*)newRecentTabsDestination {
  __weak __typeof(self) weakSelf = self;
  return
      [self createOverflowMenuDestination:IDS_IOS_TOOLS_MENU_RECENT_TABS
                              destination:overflow_menu::Destination::RecentTabs
                               symbolName:kRecentTabsSymbol
                             systemSymbol:NO
                          accessibilityID:kToolsMenuOtherDevicesId
                                  handler:^{
                                    [weakSelf openRecentTabs];
                                  }];
}

- (OverflowMenuDestination*)newSiteInfoDestination {
  __weak __typeof(self) weakSelf = self;
  OverflowMenuDestination* destination =
      [self createOverflowMenuDestination:IDS_IOS_TOOLS_MENU_SITE_INFORMATION
                              destination:overflow_menu::Destination::SiteInfo
                               symbolName:kTunerSymbol
                             systemSymbol:NO
                          accessibilityID:kToolsMenuSiteInformation
                                  handler:^{
                                    [weakSelf openSiteInformation];
                                  }];
  destination.canBeHidden = NO;
  return destination;
}

- (OverflowMenuDestination*)newSettingsDestination {
  __weak __typeof(self) weakSelf = self;
  OverflowMenuDestination* destination =
      [self createOverflowMenuDestination:IDS_IOS_TOOLS_MENU_SETTINGS
                              destination:overflow_menu::Destination::Settings
                               symbolName:kSettingsSymbol
                             systemSymbol:YES
                          accessibilityID:kToolsMenuSettingsId
                                  handler:^{
                                    [weakSelf openSettings];
                                  }];
  destination.canBeHidden = NO;
  return destination;
}

- (OverflowMenuDestination*)newWhatsNewDestination {
  __weak __typeof(self) weakSelf = self;
  return
      [self createOverflowMenuDestination:IDS_IOS_TOOLS_MENU_WHATS_NEW
                              destination:overflow_menu::Destination::WhatsNew
                               symbolName:kCheckmarkSealSymbol
                             systemSymbol:YES
                          accessibilityID:kToolsMenuWhatsNewId
                                  handler:^{
                                    [weakSelf openWhatsNew];
                                  }];
}

- (OverflowMenuDestination*)newSpotlightDebuggerDestination {
  __weak __typeof(self) weakSelf = self;
  return [self destinationForSpotlightDebugger:^{
    [weakSelf openSpotlightDebugger];
  }];
}

- (OverflowMenuDestination*)newPriceNotificationsDestination {
  __weak __typeof(self) weakSelf = self;
  return [self createOverflowMenuDestination:IDS_IOS_TOOLS_MENU_PRICE_TRACKING
                                 destination:overflow_menu::Destination::
                                                 PriceNotifications
                                  symbolName:kDownTrendSymbol
                                systemSymbol:NO
                             accessibilityID:kToolsMenuPriceNotifications
                                     handler:^{
                                       [weakSelf openPriceNotifications];
                                     }];
}

- (NSString*)hideItemTextForDestination:
    (overflow_menu::Destination)destination {
  switch (destination) {
    case overflow_menu::Destination::SiteInfo:
    case overflow_menu::Destination::Settings:
    case overflow_menu::Destination::SpotlightDebugger:
      // These items are unhideable.
      return nil;
    case overflow_menu::Destination::Bookmarks:
      return l10n_util::GetNSString(
          IDS_IOS_OVERFLOW_MENU_HIDE_DESTINATION_BOOKMARKS);
    case overflow_menu::Destination::History:
      return l10n_util::GetNSString(
          IDS_IOS_OVERFLOW_MENU_HIDE_DESTINATION_HISTORY);
    case overflow_menu::Destination::ReadingList:
      return l10n_util::GetNSString(
          IDS_IOS_OVERFLOW_MENU_HIDE_DESTINATION_READING_LIST);
    case overflow_menu::Destination::Passwords:
      return l10n_util::GetNSString(
          IDS_IOS_OVERFLOW_MENU_HIDE_DESTINATION_PASSWORDS);
    case overflow_menu::Destination::Downloads:
      return l10n_util::GetNSString(
          IDS_IOS_OVERFLOW_MENU_HIDE_DESTINATION_DOWNLOADS);
    case overflow_menu::Destination::RecentTabs:
      return l10n_util::GetNSString(
          IDS_IOS_OVERFLOW_MENU_HIDE_DESTINATION_RECENT_TABS);
    case overflow_menu::Destination::WhatsNew:
      return l10n_util::GetNSString(
          IDS_IOS_OVERFLOW_MENU_HIDE_DESTINATION_WHATS_NEW);
    case overflow_menu::Destination::PriceNotifications:
      return l10n_util::GetNSString(
          IDS_IOS_OVERFLOW_MENU_HIDE_DESTINATION_PRICE_NOTIFICATIONS);
  }
}

#pragma mark - Model Creation Utilities

// Creates an OverflowMenuDestination to be displayed in the destinations
// carousel.
- (OverflowMenuDestination*)
    createOverflowMenuDestination:(int)nameID
                      destination:(overflow_menu::Destination)destination
                       symbolName:(NSString*)symbolName
                     systemSymbol:(BOOL)systemSymbol
                  accessibilityID:(NSString*)accessibilityID
                          handler:(Handler)handler {
  __weak __typeof(self) weakSelf = self;

  NSString* name = l10n_util::GetNSString(nameID);

  auto handlerWithMetrics = ^{
    if (weakSelf.menuHasBeenDismissed) {
      return;
    }
    overflow_menu::RecordUmaActionForDestination(destination);

    [weakSelf.menuOrderer recordClickForDestination:destination];

    [weakSelf logFeatureEngagementEventForClickOnDestination:destination];

    handler();
  };

  OverflowMenuDestination* result =
      [[OverflowMenuDestination alloc] initWithName:name
                                         symbolName:symbolName
                                       systemSymbol:systemSymbol
                                   monochromeSymbol:NO
                            accessibilityIdentifier:accessibilityID
                                 enterpriseDisabled:NO
                                displayNewLabelIcon:NO
                                            handler:handlerWithMetrics];

  result.destination = static_cast<NSInteger>(destination);

  NSMutableArray<OverflowMenuLongPressItem*>* longPressItems =
      [[NSMutableArray alloc] init];

  NSString* hideItemText = [self hideItemTextForDestination:destination];
  if (hideItemText) {
    [longPressItems addObject:[[OverflowMenuLongPressItem alloc]
                                  initWithTitle:hideItemText
                                     symbolName:@"eye.slash"
                                        handler:^{
                                          [weakSelf
                                              hideDestination:destination];
                                        }]];
  }
  [longPressItems
      addObject:[[OverflowMenuLongPressItem alloc]
                    initWithTitle:l10n_util::GetNSString(
                                      IDS_IOS_OVERFLOW_MENU_EDIT_ACTIONS)
                       symbolName:@"pencil"
                          handler:^{
                            [weakSelf beginCustomization];
                          }]];
  result.longPressItems = longPressItems;

  __weak __typeof(result) weakResult = result;
  result.onShownToggleCallback = ^{
    [weakSelf onShownToggledForDestination:weakResult];
  };

  return result;
}

// Creates an OverflowMenuAction with the given name to be displayed.
- (OverflowMenuAction*)
    createOverflowMenuActionWithName:(NSString*)name
                          actionType:(overflow_menu::ActionType)actionType
                          symbolName:(NSString*)symbolName
                        systemSymbol:(BOOL)systemSymbol
                    monochromeSymbol:(BOOL)monochromeSymbol
                     accessibilityID:(NSString*)accessibilityID
                        hideItemText:(NSString*)hideItemText
                             handler:(Handler)handler {
  Handler newHandler =
      [self fullOverflowMenuActionHandlerForActionType:actionType
                                               handler:handler];

  OverflowMenuAction* action =
      [[OverflowMenuAction alloc] initWithName:name
                                    symbolName:symbolName
                                  systemSymbol:systemSymbol
                              monochromeSymbol:monochromeSymbol
                       accessibilityIdentifier:accessibilityID
                            enterpriseDisabled:NO
                           displayNewLabelIcon:NO
                                       handler:newHandler];
  action.actionType = static_cast<NSInteger>(actionType);

  ActionRanking reorderableActions = [self basePageActions];
  // If this action is not reorderable, then don't add any longpress items.
  bool actionIsReorderable =
      std::find(reorderableActions.begin(), reorderableActions.end(),
                actionType) != reorderableActions.end();
  if (actionIsReorderable) {
    action.longPressItems =
        [self actionLongPressItemsForActionType:actionType
                                   hideItemText:hideItemText];
  }
  return action;
}

// Creates an OverflowMenuAction with the given nameID as a localized string ID
// to be displayed.
- (OverflowMenuAction*)
    createOverflowMenuActionWithNameID:(int)nameID
                            actionType:(overflow_menu::ActionType)actionType
                            symbolName:(NSString*)symbolName
                          systemSymbol:(BOOL)systemSymbol
                      monochromeSymbol:(BOOL)monochromeSymbol
                       accessibilityID:(NSString*)accessibilityID
                          hideItemText:(NSString*)hideItemText
                               handler:(Handler)handler {
  NSString* name = l10n_util::GetNSString(nameID);

  return [self createOverflowMenuActionWithName:name
                                     actionType:actionType
                                     symbolName:symbolName
                                   systemSymbol:systemSymbol
                               monochromeSymbol:monochromeSymbol
                                accessibilityID:accessibilityID
                                   hideItemText:hideItemText
                                        handler:handler];
}

// Adds any necessary additions to the handler for any specific action.
- (Handler)fullOverflowMenuActionHandlerForActionType:
               (overflow_menu::ActionType)actionType
                                              handler:(Handler)handler {
  __weak __typeof(self) weakSelf = self;
  return ^{
    if (weakSelf.menuHasBeenDismissed) {
      return;
    }
    [weakSelf logFeatureEngagementEventForClickOnAction:actionType];
    handler();
  };
}

// Returns the LongPress items for the given action and hide item text. Can
// be used if actions need to update their name after action creation, as
// the hide item text should correspond to the name.
- (NSArray<OverflowMenuLongPressItem*>*)
    actionLongPressItemsForActionType:(overflow_menu::ActionType)actionType
                         hideItemText:(NSString*)hideItemText {
  __weak __typeof(self) weakSelf = self;
  NSMutableArray<OverflowMenuLongPressItem*>* longPressItems =
      [[NSMutableArray alloc] init];
  if (hideItemText) {
    [longPressItems addObject:[[OverflowMenuLongPressItem alloc]
                                  initWithTitle:hideItemText
                                     symbolName:@"eye.slash"
                                        handler:^{
                                          [weakSelf hideActionType:actionType];
                                        }]];
  }
  [longPressItems
      addObject:[[OverflowMenuLongPressItem alloc]
                    initWithTitle:l10n_util::GetNSString(
                                      IDS_IOS_OVERFLOW_MENU_EDIT_ACTIONS)
                       symbolName:@"pencil"
                          handler:^{
                            [weakSelf
                                beginCustomizationFromActionType:actionType];
                          }]];
  return [longPressItems copy];
}

#pragma mark - Private

// Creates an OverflowMenuDestination for the Spotlight debugger.
- (OverflowMenuDestination*)destinationForSpotlightDebugger:(Handler)handler {
  OverflowMenuDestination* result =
      [[OverflowMenuDestination alloc] initWithName:@"Spotlight Debugger"
                                         symbolName:kSettingsSymbol
                                       systemSymbol:YES
                                   monochromeSymbol:NO
                            accessibilityIdentifier:@"Spotlight Debugger"
                                 enterpriseDisabled:NO
                                displayNewLabelIcon:NO
                                            handler:handler];
  result.destination =
      static_cast<NSInteger>(overflow_menu::Destination::SpotlightDebugger);
  return result;
}

- (DestinationRanking)baseDestinations {
  std::vector<overflow_menu::Destination> destinations = {
      overflow_menu::Destination::Bookmarks,
      overflow_menu::Destination::History,
      overflow_menu::Destination::ReadingList,
      overflow_menu::Destination::Passwords,
      overflow_menu::Destination::Downloads,
      overflow_menu::Destination::RecentTabs,
      overflow_menu::Destination::SiteInfo,
      overflow_menu::Destination::Settings,
  };

  if (IsPriceNotificationsEnabled()) {
    destinations.push_back(overflow_menu::Destination::PriceNotifications);
  }

  destinations.push_back(overflow_menu::Destination::WhatsNew);

  return destinations;
}

// Returns YES if the Overflow Menu should indicate an identity error.
- (BOOL)shouldIndicateIdentityError {
  if (!self.syncService) {
    return NO;
  }

  return ShouldIndicateIdentityErrorInOverflowMenu(self.syncService);
}

// Updates the model to match the current page state.
- (void)updateModel {
  // First update the items' states, and then update all the orders.
  [self updateModelItemsState];
  [self updateModelOrdering];
}

// Updates the state of the individual model items (actions, destinations,
// group footers).
- (void)updateModelItemsState {
  // If the model hasn't been created, there's no need to update.
  if (!self.model) {
    return;
  }

  bool hasMachineLevelPolicies =
      _browserPolicyConnector &&
      _browserPolicyConnector->HasMachineLevelPolicies();
  bool canFetchUserPolicies =
      _authenticationService && _profilePrefs &&
      CanFetchUserPolicy(_authenticationService, _profilePrefs);
  // Set footer (on last section), if any.
  web::BrowserState* browserState =
      self.webState ? self.webState->GetBrowserState() : nullptr;
  ProfileIOS* profile = ProfileIOS::FromBrowserState(browserState);
  if (hasMachineLevelPolicies || canFetchUserPolicies) {
    // Set the Enterprise footer if there are machine level or user level
    // (aka ProfileIOS level) policies.
    self.helpActionsGroup.footer = CreateOverflowMenuManagedFooter(
        IDS_IOS_TOOLS_MENU_ENTERPRISE_MANAGED,
        IDS_IOS_TOOLS_MENU_ENTERPRISE_LEARN_MORE, kTextMenuEnterpriseInfo,
        @"overflow_menu_footer_managed", ^{
          [self enterpriseLearnMore];
        });
  } else if (profile && supervised_user::IsSubjectToParentalControls(profile)) {
    self.helpActionsGroup.footer = CreateOverflowMenuManagedFooter(
        IDS_IOS_TOOLS_MENU_PARENT_MANAGED, IDS_IOS_TOOLS_MENU_PARENT_LEARN_MORE,
        kTextMenuFamilyLinkInfo, @"overflow_menu_footer_family_link", ^{
          [self parentLearnMore];
        });
  } else {
    self.helpActionsGroup.footer = nil;
  }

  // The "Add to Reading List" functionality requires JavaScript execution,
  // which is paused while overlays are displayed over the web content area.
  self.readLaterAction.enabled =
      !self.webContentAreaShowingOverlay && [self isCurrentURLWebURL];

  BOOL bookmarkEnabled =
      [self isCurrentURLWebURL] && [self isEditBookmarksEnabled];
  self.addBookmarkAction.enabled = bookmarkEnabled;
  self.editBookmarkAction.enabled = bookmarkEnabled;
  self.translateAction.enabled = [self isTranslateEnabled];
  self.findInPageAction.enabled = [self isFindInPageEnabled];
  self.textZoomAction.enabled = [self isTextZoomEnabled];
  self.requestDesktopAction.enabled =
      [self userAgentType] == web::UserAgentType::MOBILE;
  self.requestMobileAction.enabled =
      [self userAgentType] == web::UserAgentType::DESKTOP;

  // Enable/disable items based on enterprise policies.
  self.openTabAction.enterpriseDisabled =
      IsIncognitoModeForced(self.profilePrefs);
  self.openIncognitoTabAction.enterpriseDisabled =
      IsIncognitoModeDisabled(self.profilePrefs);

  if (IsLensOverlayAvailable()) {
    self.lensOverlayAction.enabled =
        search_engines::SupportsSearchImageWithLens(self.templateURLService);
  }
}

// Updates the order of the items in each section or group.
- (void)updateModelOrdering {
  // If the model hasn't been created, there's no need to update.
  if (!self.model) {
    return;
  }

  [self.menuOrderer updateDestinations];

  NSMutableArray<OverflowMenuAction*>* appActions =
      [[NSMutableArray alloc] init];

  // The reload/stop action is only shown when the reload button is not in the
  // toolbar. The reload button is shown in the toolbar when the toolbar is not
  // split.
  if (IsSplitToolbarMode(self.baseViewController)) {
    OverflowMenuAction* reloadStopAction =
        ([self isPageLoading]) ? self.stopLoadAction : self.reloadAction;
    [appActions addObject:reloadStopAction];
  }

  [appActions
      addObjectsFromArray:@[ self.openTabAction, self.openIncognitoTabAction ]];

  if (base::ios::IsMultipleScenesSupported()) {
    [appActions addObject:self.openNewWindowAction];
  }

  self.appActionsGroup.actions = appActions;

  [self.menuOrderer updatePageActions];

  NSMutableArray<OverflowMenuAction*>* helpActions =
      [[NSMutableArray alloc] init];

  if (ios::provider::IsUserFeedbackSupported()) {
    [helpActions addObject:self.reportIssueAction];
  }

  [helpActions addObject:self.helpAction];
  [helpActions addObject:self.shareChromeAction];

  self.helpActionsGroup.actions = helpActions;
}

// Returns whether the page can be manually translated. If `forceMenuLogging` is
// true the translate client will log this result.
- (BOOL)canManuallyTranslate:(BOOL)forceMenuLogging {
  if (!self.webState) {
    return NO;
  }

  auto* translate_client =
      ChromeIOSTranslateClient::FromWebState(self.webState);
  if (!translate_client) {
    return NO;
  }

  translate::TranslateManager* translate_manager =
      translate_client->GetTranslateManager();
  DCHECK(translate_manager);
  return translate_manager->CanManuallyTranslate(forceMenuLogging);
}

// Returns whether translate is enabled on the current page.
- (BOOL)isTranslateEnabled {
  return [self canManuallyTranslate:NO];
}

// Determines whether or not translate is available on the page and logs the
// result. This method should only be called once per popup menu shown.
- (void)logTranslateAvailability {
  [self canManuallyTranslate:YES];
}

// Whether find in page is enabled.
- (BOOL)isFindInPageEnabled {
  if (!self.webState) {
    return NO;
  }

  auto* helper = GetConcreteFindTabHelperFromWebState(self.webState);
  return (helper && helper->CurrentPageSupportsFindInPage() &&
          !helper->IsFindUIActive());
}

// Whether or not text zoom is enabled for this page.
- (BOOL)isTextZoomEnabled {
  if (self.webContentAreaShowingOverlay) {
    return NO;
  }

  if (!self.webState) {
    return NO;
  }
  FontSizeTabHelper* helper = FontSizeTabHelper::FromWebState(self.webState);
  return helper && helper->CurrentPageSupportsTextZoom() &&
         !helper->IsTextZoomUIActive();
}

// Returns YES if user is allowed to edit any bookmarks.
- (BOOL)isEditBookmarksEnabled {
  return self.profilePrefs->GetBoolean(bookmarks::prefs::kEditBookmarksEnabled);
}

// Whether the page is currently loading.
- (BOOL)isPageLoading {
  return (self.webState) ? self.webState->IsLoading() : NO;
}

// Whether the current page is a web page.
- (BOOL)isCurrentURLWebURL {
  if (!self.webState) {
    return NO;
  }

  const GURL& URL = self.webState->GetLastCommittedURL();
  return URL.is_valid() && !web::GetWebClient()->IsAppSpecificURL(URL);
}

// Whether the current web page has available site info.
- (BOOL)currentWebPageSupportsSiteInfo {
  if (!self.webState) {
    return NO;
  }
  web::NavigationItem* navItem =
      self.webState->GetNavigationManager()->GetVisibleItem();
  if (!navItem) {
    return NO;
  }
  const GURL& URL = navItem->GetURL();
  // Show site info for offline pages.
  if (reading_list::IsOfflineURL(URL)) {
    return YES;
  }
  // Do not show site info for NTP.
  if (URL.spec() == kChromeUIAboutNewTabURL ||
      URL.spec() == kChromeUINewTabURL) {
    return NO;
  }

  if (self.contentBlocked) {
    return NO;
  }

  return navItem->GetVirtualURL().is_valid();
}

// Returns the UserAgentType currently in use.
- (web::UserAgentType)userAgentType {
  if (!self.webState) {
    return web::UserAgentType::NONE;
  }
  web::NavigationItem* visibleItem =
      self.webState->GetNavigationManager()->GetVisibleItem();
  if (!visibleItem) {
    return web::UserAgentType::NONE;
  }

  return visibleItem->GetUserAgentType();
}

// Creates a follow action if needed, when the follow action state is not
// hidden.
- (OverflowMenuAction*)createFollowActionIfNeeded {
  // Returns nil if the follow action state is hidden.
  if (GetFollowActionState(self.webState) == FollowActionStateHidden) {
    return nil;
  }

  OverflowMenuAction* action = [self newFollowAction];
  action.enabled = NO;
  return action;
}

- (void)dismissMenu {
  self.menuHasBeenDismissed = YES;
  [self.popupMenuHandler dismissPopupMenuAnimated:YES];
}

// Possibly logs a feature engagement tracker event when the user clicks on a
// destination.
- (void)logFeatureEngagementEventForClickOnDestination:
    (overflow_menu::Destination)destination {
  if (DestinationWasInitiallyVisible(
          destination, self.model.destinations,
          self.menuOrderer.visibleDestinationsCount)) {
    return;
  }

  if (self.engagementTracker) {
    self.engagementTracker->NotifyEvent(
        feature_engagement::events::kIOSOverflowMenuOffscreenItemUsed);
  }
}

// Possibly logs a feature engagement tracker event when the user clicks on an
// action.
- (void)logFeatureEngagementEventForClickOnAction:
    (overflow_menu::ActionType)action {
  if (ActionWasInitiallyVisible(action, self.pageActionsGroup.actions,
                                kDefaultVisiblePageActionCount)) {
    return;
  }

  if (self.engagementTracker) {
    self.engagementTracker->NotifyEvent(
        feature_engagement::events::kIOSOverflowMenuOffscreenItemUsed);
  }
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  DCHECK_EQ(_webState, webState);
  [self updateModel];
}

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation {
  DCHECK_EQ(_webState, webState);
  [self updateModel];
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  DCHECK_EQ(_webState, webState);
  [self updateModel];
}

- (void)webStateDidStartLoading:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self updateModel];
}

- (void)webStateDidStopLoading:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self updateModel];
}

- (void)webState:(web::WebState*)webState
    didChangeLoadingProgress:(double)progress {
  DCHECK_EQ(_webState, webState);
  [self updateModel];
}

- (void)webStateDidChangeBackForwardState:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self updateModel];
}

- (void)webStateDidChangeVisibleSecurityState:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self updateModel];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  self.webState = nullptr;
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  if (!status.active_web_state_change()) {
    return;
  }

  self.webState = status.new_active_web_state;
  if (self.webState && self.followAction) {
    FollowTabHelper* followTabHelper =
        FollowTabHelper::FromWebState(self.webState);
    if (followTabHelper) {
      followTabHelper->SetFollowMenuUpdater(self);
    }
  }
}

#pragma mark - BookmarkModelBridgeObserver

// If an added or removed bookmark is the same as the current url, update the
// toolbar so the star highlight is kept in sync.
- (void)didChangeChildrenForNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  [self updateModel];
}

// If all bookmarks are removed, update the toolbar so the star highlight is
// kept in sync.
- (void)bookmarkModelRemovedAllNodes {
  [self updateModel];
}

// In case we are on a bookmarked page before the model is loaded.
- (void)bookmarkModelLoaded {
  [self updateModel];
}

- (void)didChangeNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  [self updateModel];
}
- (void)didMoveNode:(const bookmarks::BookmarkNode*)bookmarkNode
         fromParent:(const bookmarks::BookmarkNode*)oldParent
           toParent:(const bookmarks::BookmarkNode*)newParent {
  // No-op -- required by BookmarkModelBridgeObserver but not used.
}
- (void)didDeleteNode:(const bookmarks::BookmarkNode*)node
           fromFolder:(const bookmarks::BookmarkNode*)folder {
  [self updateModel];
}

#pragma mark - ReadingListModelBridgeObserver

- (void)readingListModelLoaded:(const ReadingListModel*)model {
  [self updateModel];
}

- (void)readingListModelDidApplyChanges:(const ReadingListModel*)model {
  [self updateModel];
}

#pragma mark - SearchEngineObserving

- (void)searchEngineChanged {
  [self updateModel];
}

- (void)templateURLServiceShuttingDown:(TemplateURLService*)urlService {
  _templateURLService = nullptr;
}

#pragma mark - FollowMenuUpdater

- (void)updateFollowMenuItemWithWebPage:(WebPageURLs*)webPageURLs
                               followed:(BOOL)followed
                             domainName:(NSString*)domainName
                                enabled:(BOOL)enable {
  DCHECK(IsWebChannelsEnabled());
  self.followAction.enabled = enable;
  if (followed) {
    __weak __typeof(self) weakSelf = self;
    self.followAction.name = l10n_util::GetNSStringF(
        IDS_IOS_TOOLS_MENU_UNFOLLOW, base::SysNSStringToUTF16(domainName));
    self.followAction.symbolName = kXMarkSymbol;
    self.followAction.handler = [self
        fullOverflowMenuActionHandlerForActionType:overflow_menu::ActionType::
                                                       Follow
                                           handler:^{
                                             [weakSelf
                                                 unfollowWebPage:webPageURLs];
                                           }];
    NSString* hideItemText =
        l10n_util::GetNSStringF(IDS_IOS_OVERFLOW_MENU_HIDE_ACTION_UNFOLLOW,
                                base::SysNSStringToUTF16(domainName));
    self.followAction.longPressItems = [self
        actionLongPressItemsForActionType:overflow_menu::ActionType::Follow
                             hideItemText:hideItemText];
  } else {
    __weak __typeof(self) weakSelf = self;
    self.followAction.name = l10n_util::GetNSStringF(
        IDS_IOS_TOOLS_MENU_FOLLOW, base::SysNSStringToUTF16(domainName));
    self.followAction.symbolName = kPlusSymbol;
    self.followAction.handler = [self
        fullOverflowMenuActionHandlerForActionType:overflow_menu::ActionType::
                                                       Follow
                                           handler:^{
                                             [weakSelf
                                                 followWebPage:webPageURLs];
                                           }];
    NSString* hideItemText =
        l10n_util::GetNSStringF(IDS_IOS_OVERFLOW_MENU_HIDE_ACTION_FOLLOW,
                                base::SysNSStringToUTF16(domainName));
    self.followAction.longPressItems = [self
        actionLongPressItemsForActionType:overflow_menu::ActionType::Follow
                             hideItemText:hideItemText];
  }
}

#pragma mark - BrowserContainerConsumer

- (void)setContentBlocked:(BOOL)contentBlocked {
  if (_contentBlocked == contentBlocked) {
    return;
  }
  _contentBlocked = contentBlocked;
  [self updateModel];
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == bookmarks::prefs::kEditBookmarksEnabled)
    [self updateModel];
}

#pragma mark - IOSLanguageDetectionTabHelperObserving

- (void)iOSLanguageDetectionTabHelper:
            (language::IOSLanguageDetectionTabHelper*)tabHelper
                 didDetermineLanguage:
                     (const translate::LanguageDetectionDetails&)details {
  [self updateModel];
}

#pragma mark - OverlayPresenterObserving

- (void)overlayPresenter:(OverlayPresenter*)presenter
    willShowOverlayForRequest:(OverlayRequest*)request
          initialPresentation:(BOOL)initialPresentation {
  self.webContentAreaShowingOverlay = YES;
}

- (void)overlayPresenter:(OverlayPresenter*)presenter
    didHideOverlayForRequest:(OverlayRequest*)request {
  self.webContentAreaShowingOverlay = NO;
}

- (void)overlayPresenterDestroyed:(OverlayPresenter*)presenter {
  self.webContentAreaOverlayPresenter = nullptr;
}

#pragma mark - OverflowMenuDestinationProvider

- (OverflowMenuDestination*)destinationForDestinationType:
    (overflow_menu::Destination)destinationType {
  switch (destinationType) {
    case overflow_menu::Destination::Bookmarks:
      return self.bookmarksDestination;
    case overflow_menu::Destination::History:
      return (self.isIncognito) ? nil : self.historyDestination;
    case overflow_menu::Destination::ReadingList:
      // Set badges if necessary.
      if (self.engagementTracker &&
          self.engagementTracker->ShouldTriggerHelpUI(
              feature_engagement::kIPHBadgedReadingListFeature)) {
        self.readingListDestination.badge = BadgeTypePromo;
      }
      return self.readingListModel->loaded() ? self.readingListDestination
                                             : nil;
    case overflow_menu::Destination::Passwords:
      return self.passwordsDestination;
    case overflow_menu::Destination::Downloads:
      return self.downloadsDestination;
    case overflow_menu::Destination::RecentTabs:
      return self.isIncognito ? nil : self.recentTabsDestination;
    case overflow_menu::Destination::SiteInfo:
      return ([self currentWebPageSupportsSiteInfo]) ? self.siteInfoDestination
                                                     : nil;
    case overflow_menu::Destination::Settings:
      if ([self shouldIndicateIdentityError]) {
        self.settingsDestination.badge = BadgeTypeError;
      } else if (self.hasSettingsBlueDot) {
        self.settingsDestination.badge = BadgeTypePromo;
      }
      return self.settingsDestination;
    case overflow_menu::Destination::WhatsNew:
      // Possibly set the new label badge if it was never used before.
      if (self.whatsNewDestination.badge == BadgeTypeNone &&
          !WasWhatsNewUsed() && self.engagementTracker) {
        // First check if another active menu (e.g. in another window) has an
        // active badge. If so, just set the badge here without querying the
        // FET. Only query the FET if there is no currently active badge.
        WhatsNewActiveMenusData* data = static_cast<WhatsNewActiveMenusData*>(
            self.engagementTracker->GetUserData(WhatsNewActiveMenusData::key));
        if (data) {
          self.whatsNewDestination.badge = BadgeTypeNew;
          data->activeMenus++;
        } else if (self.engagementTracker->ShouldTriggerHelpUI(
                       feature_engagement::kIPHWhatsNewUpdatedFeature)) {
          std::unique_ptr<WhatsNewActiveMenusData> new_data =
              std::make_unique<WhatsNewActiveMenusData>();
          new_data->activeMenus++;
          self.engagementTracker->SetUserData(WhatsNewActiveMenusData::key,
                                              std::move(new_data));
          self.whatsNewDestination.badge = BadgeTypeNew;
        }
      }
      return self.whatsNewDestination;
    case overflow_menu::Destination::SpotlightDebugger:
      return self.spotlightDebuggerDestination;
    case overflow_menu::Destination::PriceNotifications:
      BOOL priceNotificationsActive =
          self.webState && IsPriceTrackingEnabled(ProfileIOS::FromBrowserState(
                               self.webState->GetBrowserState()));
      return (priceNotificationsActive) ? self.priceNotificationsDestination
                                        : nil;
  }
}

- (OverflowMenuDestination*)customizationDestinationForDestinationType:
    (overflow_menu::Destination)destinationType {
  switch (destinationType) {
    case overflow_menu::Destination::Bookmarks:
      return [self newBookmarksDestination];
    case overflow_menu::Destination::History:
      return [self newHistoryDestination];
    case overflow_menu::Destination::ReadingList:
      return [self newReadingListDestination];
    case overflow_menu::Destination::Passwords:
      return [self newPasswordsDestination];
    case overflow_menu::Destination::Downloads:
      return [self newDownloadsDestination];
    case overflow_menu::Destination::RecentTabs:
      return [self newRecentTabsDestination];
    case overflow_menu::Destination::SiteInfo:
      return [self newSiteInfoDestination];
    case overflow_menu::Destination::Settings:
      return [self newSettingsDestination];
    case overflow_menu::Destination::WhatsNew:
      return [self newWhatsNewDestination];
    case overflow_menu::Destination::SpotlightDebugger:
      return [self newSpotlightDebuggerDestination];
    case overflow_menu::Destination::PriceNotifications:
      return [self newPriceNotificationsDestination];
  }
}

- (void)destinationCustomizationCompleted {
  if (self.engagementTracker &&
      self.settingsDestination.badge == BadgeTypePromo) {
    self.engagementTracker->NotifyEvent(
        feature_engagement::events::kBlueDotOverflowMenuCustomized);
    [self.popupMenuHandler updateToolsMenuBlueDotVisibility];
  }
}

#pragma mark - OverflowMenuActionProvider

- (ActionRanking)basePageActions {
  ActionRanking actions = {
      overflow_menu::ActionType::Follow,
      overflow_menu::ActionType::Bookmark,
      overflow_menu::ActionType::ReadingList,
      overflow_menu::ActionType::ClearBrowsingData,
      overflow_menu::ActionType::Translate,
      overflow_menu::ActionType::DesktopSite,
      overflow_menu::ActionType::FindInPage,
      overflow_menu::ActionType::TextZoom,
  };

  if (IsLensOverlayAvailable()) {
    actions.push_back(overflow_menu::ActionType::LensOverlay);
  }

  return actions;
}

- (OverflowMenuAction*)actionForActionType:
    (overflow_menu::ActionType)actionType {
  switch (actionType) {
    case overflow_menu::ActionType::Reload:
      return ([self isPageLoading]) ? self.stopLoadAction : self.reloadAction;
    case overflow_menu::ActionType::NewTab:
      return self.openTabAction;
    case overflow_menu::ActionType::NewIncognitoTab:
      return self.openIncognitoTabAction;
    case overflow_menu::ActionType::NewWindow:
      return self.openNewWindowAction;
    case overflow_menu::ActionType::Follow: {
      // Try to create the followAction if there isn't one. It's possible that
      // sometimes when creating the model the followActionState is hidden so
      // the followAction hasn't been created but at the time when updating the
      // model, the followAction should be valid.
      if (!self.followAction) {
        self.followAction = [self createFollowActionIfNeeded];
        DCHECK(!self.followAction || self.webState != nullptr);
      }

      if (self.followAction) {
        FollowTabHelper* followTabHelper =
            FollowTabHelper::FromWebState(self.webState);
        if (followTabHelper) {
          followTabHelper->UpdateFollowMenuItem();
        }
      }
      return self.followAction;
    }
    case overflow_menu::ActionType::Bookmark: {
      BOOL pageIsBookmarked =
          self.webState && self.bookmarkModel &&
          self.bookmarkModel->IsBookmarked(self.webState->GetVisibleURL());
      return (pageIsBookmarked) ? self.editBookmarkAction
                                : self.addBookmarkAction;
    }
    case overflow_menu::ActionType::ReadingList:
      return self.readLaterAction;
    case overflow_menu::ActionType::ClearBrowsingData:
      // Showing the Clear Browsing Data Action would be confusing in incognito.
      return (self.isIncognito) ? nil : self.clearBrowsingDataAction;
    case overflow_menu::ActionType::Translate:
      return self.translateAction;
    case overflow_menu::ActionType::DesktopSite:
      return ([self userAgentType] != web::UserAgentType::DESKTOP)
                 ? self.requestDesktopAction
                 : self.requestMobileAction;
    case overflow_menu::ActionType::FindInPage:
      return self.findInPageAction;
    case overflow_menu::ActionType::TextZoom:
      return self.textZoomAction;
    case overflow_menu::ActionType::ReportAnIssue:
      return self.reportIssueAction;
    case overflow_menu::ActionType::Help:
      return self.helpAction;
    case overflow_menu::ActionType::ShareChrome:
      return self.shareChromeAction;
    case overflow_menu::ActionType::EditActions:
      return self.editActionsAction;
    case overflow_menu::ActionType::LensOverlay:
      return self.lensOverlayAction;
  }
}

// Returns an action for the given `actionType` suitable for displaying in a
// customization UI. This means that it should not depend on any page state
// for things like choosing which variant of an action to show (e.g. Add to
// Bookmarks vs Edit Bookmarks) and shouldn't be enabled based on page state.
- (OverflowMenuAction*)customizationActionForActionType:
    (overflow_menu::ActionType)actionType {
  switch (actionType) {
    // These actions should not be customizable.
    case overflow_menu::ActionType::Reload:
    case overflow_menu::ActionType::NewTab:
    case overflow_menu::ActionType::NewIncognitoTab:
    case overflow_menu::ActionType::NewWindow:
    case overflow_menu::ActionType::ReportAnIssue:
    case overflow_menu::ActionType::Help:
    case overflow_menu::ActionType::ShareChrome:
    case overflow_menu::ActionType::EditActions:
      NOTREACHED_IN_MIGRATION();
      return nil;
    case overflow_menu::ActionType::Follow:
      return [self newFollowAction];
    case overflow_menu::ActionType::Bookmark:
      return [self newAddBookmarkAction];
    case overflow_menu::ActionType::ReadingList:
      return [self newReadLaterAction];
    case overflow_menu::ActionType::ClearBrowsingData:
      return [self newClearBrowsingDataAction];
    case overflow_menu::ActionType::Translate:
      return [self newTranslateAction];
    case overflow_menu::ActionType::DesktopSite:
      return [self newRequestDesktopAction];
    case overflow_menu::ActionType::FindInPage:
      return [self newFindInPageAction];
    case overflow_menu::ActionType::TextZoom:
      return [self newTextZoomAction];
    case overflow_menu::ActionType::LensOverlay:
      return [self openLensOverlayAction];
  }
}

#pragma mark - Action handlers

// Dismisses the menu and reloads the current page.
- (void)reload {
  RecordAction(UserMetricsAction("MobileMenuReload"));
  self.tabBasedIPHBrowserAgent->NotifyMultiGestureRefreshEvent();
  [self dismissMenu];
  self.navigationAgent->Reload();
}

// Dismisses the menu and stops the current page load.
- (void)stopLoading {
  RecordAction(UserMetricsAction("MobileMenuStop"));
  [self dismissMenu];
  self.navigationAgent->StopLoading();
}

// Dismisses the menu and opens a new tab.
- (void)openTab {
  RecordAction(UserMetricsAction("MobileMenuNewTab"));
  RecordAction(UserMetricsAction("MobileTabNewTab"));

  [self dismissMenu];
  [self.applicationHandler
      openURLInNewTab:[OpenNewTabCommand commandWithIncognito:NO]];
}

// Dismisses the menu and opens a new incognito tab.
- (void)openIncognitoTab {
  RecordAction(UserMetricsAction("MobileMenuNewIncognitoTab"));
  [self dismissMenu];
  [self.applicationHandler
      openURLInNewTab:[OpenNewTabCommand commandWithIncognito:YES]];
}

// Dismisses the menu and opens a new window.
- (void)openNewWindow {
  RecordAction(UserMetricsAction("MobileMenuNewWindow"));
  [self dismissMenu];
  [self.applicationHandler
      openNewWindowWithActivity:ActivityToLoadURL(WindowActivityToolsOrigin,
                                                  GURL(kChromeUINewTabURL))];
}

// Dismisses the menu and opens the Clear Browsing Data screen.
- (void)openClearBrowsingData {
  RecordAction(UserMetricsAction("MobileMenuClearBrowsingData"));
  base::UmaHistogramEnumeration(
      browsing_data::kDeleteBrowsingDataDialogHistogram,
      browsing_data::DeleteBrowsingDataDialogAction::
          kMenuItemEntryPointSelected);

  [self dismissMenu];
  if (IsIosQuickDeleteEnabled()) {
    [self.quickDeleteHandler
        showQuickDeleteAndCanPerformTabsClosureAnimation:YES];
  } else {
    [self.settingsHandler showClearBrowsingDataSettings];
  }
}

// Follows the website corresponding to `webPage` and dismisses the menu.
- (void)followWebPage:(WebPageURLs*)webPage {
  // FollowBrowserAgent may be null after -disconnect has been called.
  FollowBrowserAgent* followBrowserAgent = self.followBrowserAgent;
  if (followBrowserAgent)
    followBrowserAgent->FollowWebSite(webPage, FollowSource::OverflowMenu);
  [self dismissMenu];
}

// Unfollows the website corresponding to `webPage` and dismisses the menu.
- (void)unfollowWebPage:(WebPageURLs*)webPage {
  // FollowBrowserAgent may be null after -disconnect has been called.
  FollowBrowserAgent* followBrowserAgent = self.followBrowserAgent;
  if (followBrowserAgent)
    followBrowserAgent->UnfollowWebSite(webPage, FollowSource::OverflowMenu);
  [self dismissMenu];
}

// Dismisses the menu and adds the current page as a bookmark or opens the
// bookmark edit screen if the current page is bookmarked.
- (void)addOrEditBookmark {
  RecordAction(UserMetricsAction("MobileMenuAddToOrEditBookmark"));
  // Dismissing the menu disconnects the mediator, so save anything cleaned up
  // there.
  web::WebState* currentWebState = self.webState;
  [self dismissMenu];
  if (!currentWebState) {
    return;
  }
  [self.bookmarksHandler bookmarkWithWebState:currentWebState];
}

// Dismisses the menu and adds the current page to the reading list.
- (void)addToReadingList {
  RecordAction(UserMetricsAction("MobileMenuReadLater"));

  web::WebState* webState = self.webState;
  if (!webState) {
    return;
  }

  // Fetching the canonical URL is asynchronous (and happen on a background
  // thread), so the operation can be started before the UI is dismissed.
  reading_list::AddToReadingListUsingCanonicalUrl(self.readingListBrowserAgent,
                                                  webState);

  [self dismissMenu];
}

// Dismisses the menu and starts translating the current page.
- (void)translatePage {
  base::RecordAction(UserMetricsAction("MobileMenuTranslate"));
  [self dismissMenu];
  [self.browserCoordinatorHandler showTranslate];
}

// Dismisses the menu and requests the desktop version of the current page
- (void)requestDesktopSite {
  RecordAction(UserMetricsAction("MobileMenuRequestDesktopSite"));
  [self dismissMenu];
  self.navigationAgent->RequestDesktopSite();
  [self.helpHandler
      presentInProductHelpWithType:InProductHelpType::kDefaultSiteView];
}

// Dismisses the menu and requests the mobile version of the current page
- (void)requestMobileSite {
  RecordAction(UserMetricsAction("MobileMenuRequestMobileSite"));
  [self dismissMenu];
  self.navigationAgent->RequestMobileSite();
}

// Dismisses the menu and opens Find In Page
- (void)openFindInPage {
  RecordAction(UserMetricsAction("MobileMenuFindInPage"));
  [self dismissMenu];
  [self.findInPageHandler openFindInPage];
}

// Dismisses the menu and opens Text Zoom
- (void)openTextZoom {
  RecordAction(UserMetricsAction("MobileMenuTextZoom"));
  [self dismissMenu];
  [self.textZoomHandler openTextZoom];
}

// Dismisses the menu and opens the Report an Issue screen.
- (void)reportAnIssue {
  RecordAction(UserMetricsAction("MobileMenuReportAnIssue"));
  [self dismissMenu];
  [self.applicationHandler
      showReportAnIssueFromViewController:self.baseViewController
                                   sender:UserFeedbackSender::ToolsMenu];
}

// Dismisses the menu and opens the help screen.
- (void)openHelp {
  RecordAction(UserMetricsAction("MobileMenuHelp"));
  [self dismissMenu];
  [self.browserCoordinatorHandler showHelpPage];
}

// Begins the action edit flow.
- (void)beginCustomization {
  // Clear the new badge if it's active.
  self.editActionsAction.displayNewLabelIcon = NO;
  self.editActionsAction.highlighted = NO;
  [self.overflowMenuCustomizationHandler showMenuCustomization];
}

- (void)beginCustomizationFromActionType:(overflow_menu::ActionType)actionType {
  [self.overflowMenuCustomizationHandler
      showMenuCustomizationFromActionType:actionType];
}

- (void)hideDestination:(overflow_menu::Destination)destination {
  DestinationCustomizationModel* destinationCustomizationModel =
      self.menuOrderer.destinationCustomizationModel;
  for (OverflowMenuDestination* menuDestination in destinationCustomizationModel
           .shownDestinations) {
    if (menuDestination.destination == static_cast<int>(destination)) {
      menuDestination.shown = NO;
    }
  }
  [self.menuOrderer commitDestinationsUpdate];
}

- (void)hideActionType:(overflow_menu::ActionType)actionType {
  ActionCustomizationModel* actionCustomizationModel =
      self.menuOrderer.actionCustomizationModel;
  for (OverflowMenuAction* action in actionCustomizationModel.shownActions) {
    if (action.actionType == static_cast<int>(actionType)) {
      action.shown = NO;
    }
  }
  [self.menuOrderer commitActionsUpdate];
}

// Creates and opens the lens overlay UI.
- (void)startLensOverlay {
  RecordAction(UserMetricsAction("MobileMenuLensOverlay"));
  [self dismissMenu];
  [self.lensOverlayHandler
      createAndShowLensUI:YES
               entrypoint:LensOverlayEntrypoint::kOverflowMenu
               completion:nil];
}

#pragma mark - Destinations Handlers

// Dismisses the menu and opens bookmarks.
- (void)openBookmarks {
  [self dismissMenu];
  [self.browserCoordinatorHandler showBookmarksManager];
}

// Dismisses the menu and opens share sheet to share Chrome's app store link
- (void)shareChromeApp {
  [self dismissMenu];
  [self.activityServiceHandler shareChromeApp];
}

// Dismisses the menu and opens history.
- (void)openHistory {
  if (base::FeatureList::IsEnabled(
          feature_engagement::kIPHiOSHistoryOnOverflowMenuFeature) &&
      _engagementTracker) {
    _engagementTracker->NotifyEvent(
        feature_engagement::events::kHistoryOnOverflowMenuUsed);
  }
  [IntentDonationHelper donateIntent:IntentType::kViewHistory];
  [self dismissMenu];
  [self.applicationHandler showHistory];
}

// Dismisses the menu and opens reading list.
- (void)openReadingList {
  [self dismissMenu];
  [self.browserCoordinatorHandler showReadingList];
}

// Dismisses the menu and opens password list.
- (void)openPasswords {
  UmaHistogramEnumeration(
      "PasswordManager.ManagePasswordsReferrer",
      password_manager::ManagePasswordsReferrer::kChromeMenuItem);
  [self dismissMenu];
  [self.settingsHandler
      showSavedPasswordsSettingsFromViewController:self.baseViewController
                                  showCancelButton:NO];
}

// Dismisses the menu and opens price notifications list.
- (void)openPriceNotifications {
  RecordAction(UserMetricsAction("MobileMenuPriceNotifications"));
  _engagementTracker->NotifyEvent(
      feature_engagement::events::kPriceNotificationsUsed);
  [self dismissMenu];
  [self.priceNotificationHandler showPriceNotifications];
}

// Dismisses the menu and opens downloads.
- (void)openDownloads {
  [self dismissMenu];
  profile_metrics::BrowserProfileType type =
      self.isIncognito ? profile_metrics::BrowserProfileType::kIncognito
                       : profile_metrics::BrowserProfileType::kRegular;
  UmaHistogramEnumeration("Download.OpenDownloadsFromMenu.PerProfileType",
                          type);
  [self.browserCoordinatorHandler showDownloadsFolder];
}

// Dismisses the menu and opens recent tabs.
- (void)openRecentTabs {
  [self dismissMenu];
  [self.browserCoordinatorHandler showRecentTabs];
}

// Dismisses the menu and shows page information.
- (void)openSiteInformation {
  [self dismissMenu];
  [self.pageInfoHandler showPageInfo];
}

// Dismisses the menu and opens What's New.
- (void)openWhatsNew {
  [self dismissMenu];
  [self.whatsNewHandler showWhatsNew];
}

// Dismisses the menu and opens settings.
- (void)openSettings {
  if (!IsBlueDotOnToolsMenuButtoneEnabled() &&
      self.settingsDestination.badge == BadgeTypePromo &&
      self.engagementTracker) {
    self.engagementTracker->NotifyEvent(
        feature_engagement::events::kBlueDotPromoOverflowMenuDismissed);
    [self.popupMenuHandler updateToolsMenuBlueDotVisibility];
  }
  [self dismissMenu];
  profile_metrics::BrowserProfileType type =
      self.isIncognito ? profile_metrics::BrowserProfileType::kIncognito
                       : profile_metrics::BrowserProfileType::kRegular;
  UmaHistogramEnumeration("Settings.OpenSettingsFromMenu.PerProfileType", type);
  [self.applicationHandler
      showSettingsFromViewController:self.baseViewController
            hasDefaultBrowserBlueDot:(self.settingsDestination.badge ==
                                      BadgeTypePromo)];
}

- (void)enterpriseLearnMore {
  [self dismissMenu];
  [self.applicationHandler
      openURLInNewTab:[OpenNewTabCommand commandWithURLFromChrome:
                                             GURL(kChromeUIManagementURL)]];
}

- (void)parentLearnMore {
  [self dismissMenu];
  GURL familyLinkURL = GURL(supervised_user::kManagedByParentUiMoreInfoUrl);
  [self.applicationHandler
      openURLInNewTab:[OpenNewTabCommand
                          commandWithURLFromChrome:familyLinkURL]];
}

- (void)openSpotlightDebugger {
  DCHECK(IsSpotlightDebuggingEnabled());
  [self dismissMenu];
  [self.browserCoordinatorHandler showSpotlightDebugger];
}

// Make any necessary updates for when `destination`'s shown state is toggled.
- (void)onShownToggledForDestination:(OverflowMenuDestination*)destination {
  // If customization is not in progress, there's no need to update any UI.
  if (!self.menuOrderer.isDestinationCustomizationInProgress) {
    return;
  }

  overflow_menu::Destination destinationType =
      static_cast<overflow_menu::Destination>(destination.destination);
  overflow_menu::ActionType correspondingActionType;
  NSString* subtitle;
  switch (destinationType) {
    case overflow_menu::Destination::History:
    case overflow_menu::Destination::Passwords:
    case overflow_menu::Destination::Downloads:
    case overflow_menu::Destination::RecentTabs:
    case overflow_menu::Destination::SiteInfo:
    case overflow_menu::Destination::Settings:
    case overflow_menu::Destination::WhatsNew:
    case overflow_menu::Destination::SpotlightDebugger:
    case overflow_menu::Destination::PriceNotifications:
      // Most destinations have no corresponding destination and nothing special
      // to be done when their shown state is toggled.
      return;
    case overflow_menu::Destination::Bookmarks:
      correspondingActionType = overflow_menu::ActionType::Bookmark;
      subtitle = l10n_util::GetNSString(
          IDS_IOS_OVERFLOW_MENU_HIDDEN_BOOKMARKS_SUBTITLE);
      break;
    case overflow_menu::Destination::ReadingList:
      correspondingActionType = overflow_menu::ActionType::ReadingList;
      subtitle = l10n_util::GetNSString(
          IDS_IOS_OVERFLOW_MENU_HIDDEN_READING_LIST_SUBTITLE);
      break;
  }

  [self.menuOrderer customizationUpdateToggledShown:destination.shown
                                forLinkedActionType:correspondingActionType
                                     actionSubtitle:subtitle];
}

@end
