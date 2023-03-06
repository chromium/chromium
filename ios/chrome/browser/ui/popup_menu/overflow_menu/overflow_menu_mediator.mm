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
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/language/ios/browser/ios_language_detection_tab_helper.h"
#import "components/language/ios/browser/ios_language_detection_tab_helper_observer_bridge.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/profile_metrics/browser_profile_type.h"
#import "components/translate/core/browser/translate_manager.h"
#import "components/translate/core/browser/translate_prefs.h"
#import "ios/chrome/browser/bookmarks/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/commerce/push_notification/push_notification_feature.h"
#import "ios/chrome/browser/find_in_page/abstract_find_tab_helper.h"
#import "ios/chrome/browser/flags/system_flags.h"
#import "ios/chrome/browser/follow/follow_browser_agent.h"
#import "ios/chrome/browser/follow/follow_menu_updater.h"
#import "ios/chrome/browser/follow/follow_tab_helper.h"
#import "ios/chrome/browser/follow/follow_util.h"
#import "ios/chrome/browser/ntp/features.h"
#import "ios/chrome/browser/overlays/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/public/overlay_presenter_observer_bridge.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/policy/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/reading_list/offline_url_utils.h"
#import "ios/chrome/browser/tabs/features.h"
#import "ios/chrome/browser/translate/chrome_ios_translate_client.h"
#import "ios/chrome/browser/ui/commands/activity_service_commands.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/bookmarks_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/ui/commands/find_in_page_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/commands/page_info_commands.h"
#import "ios/chrome/browser/ui/commands/price_notifications_commands.h"
#import "ios/chrome/browser/ui/commands/reading_list_add_command.h"
#import "ios/chrome/browser/ui/commands/text_zoom_commands.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_utils.h"
#import "ios/chrome/browser/ui/icons/symbols.h"
#import "ios/chrome/browser/ui/ntp/metrics/feed_metrics_recorder.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/destination_usage_history/destination_usage_history.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/feature_flags.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_constants.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_swift.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_utils.h"
#import "ios/chrome/browser/ui/sharing/sharing_params.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_util.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "ios/chrome/browser/web/font_size/font_size_tab_helper.h"
#import "ios/chrome/browser/web/web_navigation_browser_agent.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/window_activities/window_activity_helpers.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_api.h"
#import "ios/web/common/user_agent.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frame_util.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::RecordAction;
using base::UmaHistogramEnumeration;
using base::UserMetricsAction;
using experimental_flags::IsSpotlightDebuggingEnabled;

namespace {

// Key used for storing NSUserDefault entry to keep track of the last timestamp
// we've shown the default browser blue dot promo.
NSString* const kMostRecentTimestampBlueDotPromoShownInOverflowMenu =
    @"MostRecentTimestampBlueDotPromoShownInOverflowMenu";

typedef void (^Handler)(void);

OverflowMenuAction* CreateOverflowMenuActionWithString(
    NSString* name,
    NSString* imageName,
    NSString* accessibilityID,
    Handler handler) {
  DCHECK(!UseSymbols());
  return [[OverflowMenuAction alloc] initWithName:name
                                            image:[UIImage imageNamed:imageName]
                          accessibilityIdentifier:accessibilityID
                               enterpriseDisabled:NO
                                          handler:handler];
}

OverflowMenuAction* CreateOverflowMenuAction(int nameID,
                                             NSString* imageName,
                                             NSString* accessibilityID,
                                             Handler handler) {
  DCHECK(!UseSymbols());
  NSString* name = l10n_util::GetNSString(nameID);
  return CreateOverflowMenuActionWithString(name, imageName, accessibilityID,
                                            handler);
}

OverflowMenuAction* CreateOverflowMenuActionWithString(
    NSString* name,
    NSString* symbolName,
    bool systemSymbol,
    bool monochromeSymbol,
    NSString* accessibilityID,
    Handler handler) {
  DCHECK(UseSymbols());
  return [[OverflowMenuAction alloc] initWithName:name
                                       symbolName:symbolName
                                     systemSymbol:systemSymbol
                                 monochromeSymbol:monochromeSymbol
                          accessibilityIdentifier:accessibilityID
                               enterpriseDisabled:NO
                                          handler:handler];
}

OverflowMenuAction* CreateOverflowMenuAction(int nameID,
                                             NSString* symbolName,
                                             bool systemSymbol,
                                             bool monochromeSymbol,
                                             NSString* accessibilityID,
                                             Handler handler) {
  DCHECK(UseSymbols());
  NSString* name = l10n_util::GetNSString(nameID);

  return CreateOverflowMenuActionWithString(name, symbolName, systemSymbol,
                                            monochromeSymbol, accessibilityID,
                                            handler);
}

OverflowMenuFooter* CreateOverflowMenuManagedFooter(int nameID,
                                                    int linkID,
                                                    NSString* imageName,
                                                    Handler handler) {
  NSString* name = l10n_util::GetNSString(nameID);
  NSString* link = l10n_util::GetNSString(linkID);
  return [[OverflowMenuFooter alloc] initWithName:name
                                             link:link
                                            image:[UIImage imageNamed:imageName]
                          accessibilityIdentifier:kTextMenuEnterpriseInfo
                                          handler:handler];
}

}  // namespace

@interface OverflowMenuMediator () <BookmarkModelBridgeObserver,
                                    CRWWebStateObserver,
                                    FollowMenuUpdater,
                                    IOSLanguageDetectionTabHelperObserving,
                                    OverlayPresenterObserving,
                                    PrefObserverDelegate,
                                    WebStateListObserving> {
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;

  // Observer for the content area overlay events
  std::unique_ptr<OverlayPresenterObserver> _overlayPresenterObserver;

  // Bridge to register for bookmark changes.
  std::unique_ptr<BookmarkModelBridge> _bookmarkModelBridge;

  // Bridge to get notified of the language detection event.
  std::unique_ptr<language::IOSLanguageDetectionTabHelperObserverBridge>
      _iOSLanguageDetectionTabHelperObserverBridge;

  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  std::unique_ptr<PrefChangeRegistrar> _prefChangeRegistrar;
}

// The destination usage history, which (1) tracks which items from the carousel
// are clicked, and (2) suggests a sorted order for carousel menu items.
@property(nonatomic, strong) DestinationUsageHistory* destinationUsageHistory;

// The current web state.
@property(nonatomic, assign) web::WebState* webState;

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

@property(nonatomic, strong) OverflowMenuAction* reloadAction;
@property(nonatomic, strong) OverflowMenuAction* stopLoadAction;
@property(nonatomic, strong) OverflowMenuAction* openTabAction;
@property(nonatomic, strong) OverflowMenuAction* openIncognitoTabAction;
@property(nonatomic, strong) OverflowMenuAction* openNewWindowAction;

@property(nonatomic, strong) OverflowMenuAction* pinTabAction;
@property(nonatomic, strong) OverflowMenuAction* unpinTabAction;

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

@end

@implementation OverflowMenuMediator

@synthesize overflowMenuModel = _overflowMenuModel;

+ (void)setTabPinned:(BOOL)pinned
            webState:(web::WebState*)webState
        webStateList:(WebStateList*)webStateList {
  if (!webState || !webStateList) {
    return;
  }

  int webStateIndex = webStateList->GetIndexOfWebState(webState);
  webStateList->SetWebStatePinnedAt(webStateIndex, pinned);
}

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
  _overflowMenuModel = nil;

  self.webContentAreaOverlayPresenter = nullptr;

  if (_engagementTracker) {
    if (self.readingListDestination.badge != BadgeTypeNone) {
      _engagementTracker->Dismissed(
          feature_engagement::kIPHBadgedReadingListFeature);
    }

    _engagementTracker = nullptr;
  }

  if (self.webState && self.followAction) {
    FollowTabHelper* followTabHelper =
        FollowTabHelper::FromWebState(self.webState);
    if (followTabHelper) {
      followTabHelper->RemoveFollowMenuUpdater();
    }
  }

  self.followBrowserAgent = nullptr;

  [self.destinationUsageHistory stop];
  self.destinationUsageHistory = nil;

  self.webState = nullptr;
  self.webStateList = nullptr;

  self.bookmarkModel = nullptr;
  self.browserStatePrefs = nullptr;
  self.localStatePrefs = nullptr;
}

#pragma mark - Property getters/setters

- (OverflowMenuModel*)overflowMenuModel {
  if (!_overflowMenuModel) {
    _overflowMenuModel = [self createModel];
    [self updateModel];
  }
  return _overflowMenuModel;
}

- (void)setIsIncognito:(BOOL)isIncognito {
  _isIncognito = isIncognito;
  [self updateModel];
}

- (void)setWebState:(web::WebState*)webState {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserver.get());
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
  }
}

- (void)setWebStateList:(WebStateList*)webStateList {
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());

    _iOSLanguageDetectionTabHelperObserverBridge.reset();
  }

  _webStateList = webStateList;

  self.webState = (_webStateList) ? _webStateList->GetActiveWebState() : nil;

  if (self.webState) {
    FollowTabHelper* followTabHelper =
        FollowTabHelper::FromWebState(self.webState);
    if (followTabHelper) {
      followTabHelper->SetFollowMenuUpdater(self);
    }
  }

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

- (void)setBrowserStatePrefs:(PrefService*)browserStatePrefs {
  _prefObserverBridge.reset();
  _prefChangeRegistrar.reset();

  _browserStatePrefs = browserStatePrefs;

  if (_browserStatePrefs) {
    _prefChangeRegistrar = std::make_unique<PrefChangeRegistrar>();
    _prefChangeRegistrar->Init(browserStatePrefs);
    _prefObserverBridge.reset(new PrefObserverBridge(self));
    _prefObserverBridge->ObserveChangesForPreference(
        bookmarks::prefs::kEditBookmarksEnabled, _prefChangeRegistrar.get());
  }
}

- (void)setLocalStatePrefs:(PrefService*)localStatePrefs {
  _localStatePrefs = localStatePrefs;

  if (!self.isIncognito) {
    self.destinationUsageHistory =
        [[DestinationUsageHistory alloc] initWithPrefService:localStatePrefs];
    self.destinationUsageHistory.visibleDestinationsCount =
        self.visibleDestinationsCount;
    [self.destinationUsageHistory start];
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

#pragma mark - Model Creation

- (OverflowMenuModel*)createModel {
  __weak __typeof(self) weakSelf = self;

  if (UseSymbols()) {
    // Bookmarks destination.
    self.bookmarksDestination = [self
        createOverflowMenuDestination:IDS_IOS_TOOLS_MENU_BOOKMARKS
                          destination:overflow_menu::Destination::Bookmarks
                           symbolName:kBookmarksSymbol
                         systemSymbol:YES
                      accessibilityID:kToolsMenuBookmarksId
                              handler:^{
                                [weakSelf openBookmarks];
                              }];
  } else {
    // Bookmarks destination.
    self.bookmarksDestination = [self
        createOverflowMenuDestination:IDS_IOS_TOOLS_MENU_BOOKMARKS
                          destination:overflow_menu::Destination::Bookmarks
                            imageName:@"overflow_menu_destination_bookmarks"
                      accessibilityID:kToolsMenuBookmarksId
                              handler:^{
                                [weakSelf openBookmarks];
                              }];
  }

  if (UseSymbols()) {
    // Downloads destination.
    self.downloadsDestination = [self
        createOverflowMenuDestination:IDS_IOS_TOOLS_MENU_DOWNLOADS
                          destination:overflow_menu::Destination::Downloads
                           symbolName:kDownloadSymbol
                         systemSymbol:YES
                      accessibilityID:kToolsMenuDownloadsId
                              handler:^{
                                [weakSelf openDownloads];
                              }];
  } else {
    // Downloads destination.
    self.downloadsDestination = [self
        createOverflowMenuDestination:IDS_IOS_TOOLS_MENU_DOWNLOADS
                          destination:overflow_menu::Destination::Downloads
                            imageName:@"overflow_menu_destination_downloads"
                      accessibilityID:kToolsMenuDownloadsId
                              handler:^{
                                [weakSelf openDownloads];
                              }];
  }

  if (UseSymbols()) {
    // History destination.
    self.historyDestination =
        [self createOverflowMenuDestination:IDS_IOS_TOOLS_MENU_HISTORY
                                destination:overflow_menu::Destination::History
                                 symbolName:kHistorySymbol
                               systemSymbol:YES
                            accessibilityID:kToolsMenuHistoryId
                                    handler:^{
                                      [weakSelf openHistory];
                                    }];
  } else {
    // History destination.
    self.historyDestination =
        [self createOverflowMenuDestination:IDS_IOS_TOOLS_MENU_HISTORY
                                destination:overflow_menu::Destination::History
                                  imageName:@"overflow_menu_destination_history"
                            accessibilityID:kToolsMenuHistoryId
                                    handler:^{
                                      [weakSelf openHistory];
                                    }];
  }

  if (UseSymbols()) {
    // Passwords destination.
    self.passwordsDestination = [self
        createOverflowMenuDestination:IDS_IOS_TOOLS_MENU_PASSWORD_MANAGER
                          destination:overflow_menu::Destination::Passwords
                           symbolName:kPasswordSymbol
                         systemSymbol:NO
                      accessibilityID:kToolsMenuPasswordsId
                              handler:^{
                                [weakSelf openPasswords];
                              }];
  } else {
    // Passwords destination.
    self.passwordsDestination = [self
        createOverflowMenuDestination:IDS_IOS_TOOLS_MENU_PASSWORD_MANAGER
                          destination:overflow_menu::Destination::Passwords
                            imageName:@"overflow_menu_destination_passwords"
                      accessibilityID:kToolsMenuPasswordsId
                              handler:^{
                                [weakSelf openPasswords];
                              }];
  }

  if (UseSymbols()) {
    // Price Tracking destination.
    self.priceNotificationsDestination =
        [self createOverflowMenuDestination:
                  IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_TITLE
                                destination:overflow_menu::Destination::
                                                PriceNotifications
                                 symbolName:kDownTrendSymbol
                               systemSymbol:NO
                            accessibilityID:kToolsMenuPriceNotifications
                                    handler:^{
                                      [weakSelf openPriceNotifications];
                                    }];
  } else {
    self.priceNotificationsDestination = [self
        createOverflowMenuDestination:
            IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_TITLE
                          destination:overflow_menu::Destination::
                                          PriceNotifications
                            imageName:
                                @"overflow_menu_destination_price_notifications"
                      accessibilityID:kToolsMenuPriceNotifications
                              handler:^{
                                [weakSelf openPriceNotifications];
                              }];
  }

  if (UseSymbols()) {
    // Reading List destination.
    self.readingListDestination = [self
        createOverflowMenuDestination:IDS_IOS_TOOLS_MENU_READING_LIST
                          destination:overflow_menu::Destination::ReadingList
                           symbolName:kReadingListSymbol
                         systemSymbol:NO
                      accessibilityID:kToolsMenuReadingListId
                              handler:^{
                                [weakSelf openReadingList];
                              }];
  } else {
    // Reading List destination.
    self.readingListDestination = [self
        createOverflowMenuDestination:IDS_IOS_TOOLS_MENU_READING_LIST
                          destination:overflow_menu::Destination::ReadingList
                            imageName:@"overflow_menu_destination_reading_list"
                      accessibilityID:kToolsMenuReadingListId
                              handler:^{
                                [weakSelf openReadingList];
                              }];
  }

  if (UseSymbols()) {
    // Recent Tabs destination.
    self.recentTabsDestination = [self
        createOverflowMenuDestination:IDS_IOS_TOOLS_MENU_RECENT_TABS
                          destination:overflow_menu::Destination::RecentTabs
                           symbolName:kRecentTabsSymbol
                         systemSymbol:NO
                      accessibilityID:kToolsMenuOtherDevicesId
                              handler:^{
                                [weakSelf openRecentTabs];
                              }];
  } else {
    // Recent Tabs destination.
    self.recentTabsDestination = [self
        createOverflowMenuDestination:IDS_IOS_TOOLS_MENU_RECENT_TABS
                          destination:overflow_menu::Destination::RecentTabs
                            imageName:@"overflow_menu_destination_recent_tabs"
                      accessibilityID:kToolsMenuOtherDevicesId
                              handler:^{
                                [weakSelf openRecentTabs];
                              }];
  }

  if (UseSymbols()) {
    // Settings destination.
    self.settingsDestination =
        [self createOverflowMenuDestination:IDS_IOS_TOOLS_MENU_SETTINGS
                                destination:overflow_menu::Destination::Settings
                                 symbolName:kSettingsSymbol
                               systemSymbol:YES
                            accessibilityID:kToolsMenuSettingsId
                                    handler:^{
                                      [weakSelf openSettings];
                                    }];
  } else {
    // Settings destination.
    self.settingsDestination = [self
        createOverflowMenuDestination:IDS_IOS_TOOLS_MENU_SETTINGS
                          destination:overflow_menu::Destination::Settings
                            imageName:@"overflow_menu_destination_settings"
                      accessibilityID:kToolsMenuSettingsId
                              handler:^{
                                [weakSelf openSettings];
                              }];
  }

  self.spotlightDebuggerDestination = [self destinationForSpotlightDebugger:^{
    [weakSelf openSpotlightDebugger];
  }];

  if (UseSymbols()) {
    // WhatsNew destination.
    self.whatsNewDestination = [self
        createOverflowMenuDestination:IDS_IOS_CONTENT_SUGGESTIONS_WHATS_NEW
                          destination:overflow_menu::Destination::WhatsNew
                           symbolName:kCheckmarkSealSymbol
                         systemSymbol:YES
                      accessibilityID:kToolsMenuWhatsNewId
                              handler:^{
                                [weakSelf openWhatsNew];
                              }];
  } else {
    // WhatsNew destination.
    self.whatsNewDestination = [self
        createOverflowMenuDestination:IDS_IOS_CONTENT_SUGGESTIONS_WHATS_NEW
                          destination:overflow_menu::Destination::WhatsNew
                            imageName:@"overflow_menu_destination_whats_new"
                      accessibilityID:kToolsMenuWhatsNewId
                              handler:^{
                                [weakSelf openWhatsNew];
                              }];
  }

  if (UseSymbols()) {
    // Site Info destination.
    self.siteInfoDestination =
        [self createOverflowMenuDestination:IDS_IOS_TOOLS_MENU_SITE_INFORMATION
                                destination:overflow_menu::Destination::SiteInfo
                                 symbolName:kInfoCircleSymbol
                               systemSymbol:YES
                            accessibilityID:kToolsMenuSiteInformation
                                    handler:^{
                                      [weakSelf openSiteInformation];
                                    }];
  } else {
    // Site Info destination.
    self.siteInfoDestination = [self
        createOverflowMenuDestination:IDS_IOS_TOOLS_MENU_SITE_INFORMATION
                          destination:overflow_menu::Destination::SiteInfo
                            imageName:@"overflow_menu_destination_site_info"
                      accessibilityID:kToolsMenuSiteInformation
                              handler:^{
                                [weakSelf openSiteInformation];
                              }];
  }

  [self logTranslateAvailability];

  if (UseSymbols()) {
    self.reloadAction = CreateOverflowMenuAction(
        IDS_IOS_TOOLS_MENU_RELOAD, kArrowClockWiseSymbol, /*systemSymbol=*/NO,
        /*monochromeSymbol=*/NO, kToolsMenuReload, ^{
          [weakSelf reload];
        });

    self.stopLoadAction = CreateOverflowMenuAction(
        IDS_IOS_TOOLS_MENU_STOP, kXMarkSymbol, /*systemSymbol=*/YES,
        /*monochromeSymbol=*/NO, kToolsMenuStop, ^{
          [weakSelf stopLoading];
        });

    self.openTabAction = CreateOverflowMenuAction(
        IDS_IOS_TOOLS_MENU_NEW_TAB, kNewTabCircleActionSymbol,
        /*systemSymbol=*/YES, /*monochromeSymbol=*/NO, kToolsMenuNewTabId, ^{
          [weakSelf openTab];
        });

    self.openIncognitoTabAction = CreateOverflowMenuAction(
        IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB, kIncognitoSymbol,
        /*systemSymbol=*/NO, /*monochromeSymbol=*/NO,
        kToolsMenuNewIncognitoTabId, ^{
          [weakSelf openIncognitoTab];
        });

    self.openNewWindowAction = CreateOverflowMenuAction(
        IDS_IOS_TOOLS_MENU_NEW_WINDOW, kNewWindowActionSymbol,
        /*systemSymbol=*/YES, /*monochromeSymbol=*/NO, kToolsMenuNewWindowId, ^{
          [weakSelf openNewWindow];
        });

    self.pinTabAction = CreateOverflowMenuAction(
        IDS_IOS_TOOLS_MENU_PIN_TAB, kPinSymbol,
        /*systemSymbol=*/YES, /*monochromeSymbol=*/NO, kToolsMenuPinTabId, ^{
          [weakSelf pinTab];
        });

    self.unpinTabAction = CreateOverflowMenuAction(
        IDS_IOS_TOOLS_MENU_UNPIN_TAB, kPinSlashSymbol,
        /*systemSymbol=*/YES, /*monochromeSymbol=*/NO, kToolsMenuUnpinTabId, ^{
          [weakSelf unpinTab];
        });

    self.clearBrowsingDataAction = CreateOverflowMenuAction(
        IDS_IOS_TOOLS_MENU_CLEAR_BROWSING_DATA, kTrashSymbol,
        /*systemSymbol=*/YES, /*monochromeSymbol=*/NO,
        kToolsMenuClearBrowsingData, ^{
          [weakSelf openClearBrowsingData];
        });

    if (GetFollowActionState(self.webState) != FollowActionStateHidden) {
      OverflowMenuAction* action = CreateOverflowMenuActionWithString(
          l10n_util::GetNSStringF(IDS_IOS_TOOLS_MENU_FOLLOW, u""), kPlusSymbol,
          /*systemSymbol=*/YES, /*monochromeSymbol=*/NO, kToolsMenuFollow,
          ^{
          });

      action.enabled = NO;
      self.followAction = action;
    }

    self.addBookmarkAction = CreateOverflowMenuAction(
        IDS_IOS_TOOLS_MENU_ADD_TO_BOOKMARKS, kAddBookmarkActionSymbol,
        /*systemSymbol=*/YES, /*monochromeSymbol=*/NO, kToolsMenuAddToBookmarks,
        ^{
          [weakSelf addOrEditBookmark];
        });

    self.editBookmarkAction = CreateOverflowMenuAction(
        IDS_IOS_TOOLS_MENU_EDIT_BOOKMARK, kEditActionSymbol,
        /*systemSymbol=*/YES, /*monochromeSymbol=*/NO, kToolsMenuEditBookmark,
        ^{
          [weakSelf addOrEditBookmark];
        });

    self.readLaterAction = CreateOverflowMenuAction(
        IDS_IOS_CONTENT_CONTEXT_ADDTOREADINGLIST, kReadLaterActionSymbol,
        /*systemSymbol=*/YES, /*monochromeSymbol=*/NO, kToolsMenuReadLater, ^{
          [weakSelf addToReadingList];
        });

    self.translateAction = CreateOverflowMenuAction(
        IDS_IOS_TOOLS_MENU_TRANSLATE, kTranslateSymbol,
        /*systemSymbol=*/NO, /*monochromeSymbol=*/NO, kToolsMenuTranslateId, ^{
          [weakSelf translatePage];
        });

    self.requestDesktopAction = CreateOverflowMenuAction(
        IDS_IOS_TOOLS_MENU_REQUEST_DESKTOP_SITE, kDesktopSymbol,
        /*systemSymbol=*/YES, /*monochromeSymbol=*/YES,
        kToolsMenuRequestDesktopId, ^{
          [weakSelf requestDesktopSite];
        });

    self.requestMobileAction = CreateOverflowMenuAction(
        IDS_IOS_TOOLS_MENU_REQUEST_MOBILE_SITE, kIPhoneSymbol,
        /*systemSymbol=*/YES, /*monochromeSymbol=*/YES,
        kToolsMenuRequestMobileId, ^{
          [weakSelf requestMobileSite];
        });

    self.findInPageAction = CreateOverflowMenuAction(
        IDS_IOS_TOOLS_MENU_FIND_IN_PAGE, kFindInPageActionSymbol,
        /*systemSymbol=*/YES, /*monochromeSymbol=*/NO, kToolsMenuFindInPageId,
        ^{
          [weakSelf openFindInPage];
        });

    self.textZoomAction = CreateOverflowMenuAction(
        IDS_IOS_TOOLS_MENU_TEXT_ZOOM, kZoomTextActionSymbol,
        /*systemSymbol=*/YES, /*monochromeSymbol=*/NO, kToolsMenuTextZoom, ^{
          [weakSelf openTextZoom];
        });

    self.reportIssueAction = CreateOverflowMenuAction(
        IDS_IOS_OPTIONS_REPORT_AN_ISSUE, kWarningSymbol, /*systemSymbol=*/YES,
        /*monochromeSymbol=*/NO, kToolsMenuReportAnIssueId, ^{
          [weakSelf reportAnIssue];
        });

    self.helpAction = CreateOverflowMenuAction(
        IDS_IOS_TOOLS_MENU_HELP_MOBILE, kHelpSymbol, /*systemSymbol=*/YES,
        /*monochromeSymbol=*/NO, kToolsMenuHelpId, ^{
          [weakSelf openHelp];
        });

    self.shareChromeAction = CreateOverflowMenuAction(
        IDS_IOS_OVERFLOW_MENU_SHARE_CHROME, kShareSymbol, /*systemSymbol=*/YES,
        /*monochromeSymbol=*/NO, kToolsMenuShareChromeId, ^{
          [weakSelf shareChromeApp];
        });
  } else {
    self.reloadAction = CreateOverflowMenuAction(IDS_IOS_TOOLS_MENU_RELOAD,
                                                 @"overflow_menu_action_reload",
                                                 kToolsMenuReload, ^{
                                                   [weakSelf reload];
                                                 });

    self.stopLoadAction = CreateOverflowMenuAction(IDS_IOS_TOOLS_MENU_STOP,
                                                   @"overflow_menu_action_stop",
                                                   kToolsMenuStop, ^{
                                                     [weakSelf stopLoading];
                                                   });

    self.openTabAction = CreateOverflowMenuAction(
        IDS_IOS_TOOLS_MENU_NEW_TAB, @"overflow_menu_action_new_tab",
        kToolsMenuNewTabId, ^{
          [weakSelf openTab];
        });

    self.openIncognitoTabAction = CreateOverflowMenuAction(
        IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB, @"overflow_menu_action_incognito",
        kToolsMenuNewIncognitoTabId, ^{
          [weakSelf openIncognitoTab];
        });

    self.openNewWindowAction = CreateOverflowMenuAction(
        IDS_IOS_TOOLS_MENU_NEW_WINDOW, @"overflow_menu_action_new_window",
        kToolsMenuNewWindowId, ^{
          [weakSelf openNewWindow];
        });

    self.pinTabAction = CreateOverflowMenuAction(
        IDS_IOS_TOOLS_MENU_PIN_TAB, @"overflow_menu_action_pin_tab",
        kToolsMenuPinTabId, ^{
          [weakSelf pinTab];
        });

    self.unpinTabAction = CreateOverflowMenuAction(
        IDS_IOS_TOOLS_MENU_UNPIN_TAB, @"overflow_menu_action_unpin_tab",
        kToolsMenuUnpinTabId, ^{
          [weakSelf unpinTab];
        });

    self.clearBrowsingDataAction =
        CreateOverflowMenuAction(IDS_IOS_TOOLS_MENU_CLEAR_BROWSING_DATA,
                                 @"overflow_menu_action_clear_browsing_data",
                                 kToolsMenuClearBrowsingData, ^{
                                   [weakSelf openClearBrowsingData];
                                 });

    self.followAction = [self createFollowActionIfNeeded];

    self.addBookmarkAction = CreateOverflowMenuAction(
        IDS_IOS_TOOLS_MENU_ADD_TO_BOOKMARKS, @"overflow_menu_action_bookmark",
        kToolsMenuAddToBookmarks, ^{
          [weakSelf addOrEditBookmark];
        });

    self.editBookmarkAction = CreateOverflowMenuAction(
        IDS_IOS_TOOLS_MENU_EDIT_BOOKMARK, @"overflow_menu_action_edit_bookmark",
        kToolsMenuEditBookmark, ^{
          [weakSelf addOrEditBookmark];
        });

    self.readLaterAction = CreateOverflowMenuAction(
        IDS_IOS_CONTENT_CONTEXT_ADDTOREADINGLIST,
        @"overflow_menu_action_read_later", kToolsMenuReadLater, ^{
          [weakSelf addToReadingList];
        });

    self.translateAction = CreateOverflowMenuAction(
        IDS_IOS_TOOLS_MENU_TRANSLATE, @"overflow_menu_action_translate",
        kToolsMenuTranslateId, ^{
          [weakSelf translatePage];
        });

    self.requestDesktopAction = CreateOverflowMenuAction(
        IDS_IOS_TOOLS_MENU_REQUEST_DESKTOP_SITE,
        @"overflow_menu_action_request_desktop", kToolsMenuRequestDesktopId, ^{
          [weakSelf requestDesktopSite];
        });

    self.requestMobileAction = CreateOverflowMenuAction(
        IDS_IOS_TOOLS_MENU_REQUEST_MOBILE_SITE,
        @"overflow_menu_action_request_mobile", kToolsMenuRequestMobileId, ^{
          [weakSelf requestMobileSite];
        });

    self.findInPageAction = CreateOverflowMenuAction(
        IDS_IOS_TOOLS_MENU_FIND_IN_PAGE, @"overflow_menu_action_find_in_page",
        kToolsMenuFindInPageId, ^{
          [weakSelf openFindInPage];
        });

    self.textZoomAction = CreateOverflowMenuAction(
        IDS_IOS_TOOLS_MENU_TEXT_ZOOM, @"overflow_menu_action_text_zoom",
        kToolsMenuTextZoom, ^{
          [weakSelf openTextZoom];
        });

    self.reportIssueAction = CreateOverflowMenuAction(
        IDS_IOS_OPTIONS_REPORT_AN_ISSUE, @"overflow_menu_action_report_issue",
        kToolsMenuReportAnIssueId, ^{
          [weakSelf reportAnIssue];
        });

    self.helpAction = CreateOverflowMenuAction(IDS_IOS_TOOLS_MENU_HELP_MOBILE,
                                               @"overflow_menu_action_help",
                                               kToolsMenuHelpId, ^{
                                                 [weakSelf openHelp];
                                               });

    self.shareChromeAction = CreateOverflowMenuAction(
        IDS_IOS_OVERFLOW_MENU_SHARE_CHROME,
        @"overflow_menu_action_share_chrome", kToolsMenuShareChromeId, ^{
          [weakSelf shareChromeApp];
        });
  }

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

  // Footer and actions vary based on state, so they're set in -updateModel.
  self.helpActionsGroup =
      [[OverflowMenuActionGroup alloc] initWithGroupName:@"help_actions"
                                                 actions:@[]
                                                  footer:nil];

  // Destinations and footer vary based on state, so they're set in
  // -updateModel.
  return [[OverflowMenuModel alloc] initWithDestinations:@[]
                                            actionGroups:@[
                                              self.appActionsGroup,
                                              self.pageActionsGroup,
                                              self.helpActionsGroup,
                                            ]];
}

#pragma mark - Private

// Creates an OverflowMenuDestination to be displayed in the destinations
// carousel.
- (OverflowMenuDestination*)
    createOverflowMenuDestination:(int)nameID
                      destination:(overflow_menu::Destination)destination
                       symbolName:(NSString*)symbolName
                     systemSymbol:(BOOL)systemSymbol
                  accessibilityID:(NSString*)accessibilityID
                          handler:(Handler)handler {
  DCHECK(UseSymbols());
  __weak __typeof(self) weakSelf = self;

  NSString* name = l10n_util::GetNSString(nameID);

  auto handlerWithMetrics = ^{
    overflow_menu::RecordUmaActionForDestination(destination);

    if (IsSmartSortingNewOverflowMenuEnabled()) {
      [weakSelf.destinationUsageHistory recordClickForDestination:destination];
    }

    handler();
  };

  OverflowMenuDestination* result =
      [[OverflowMenuDestination alloc] initWithName:name
                                         symbolName:symbolName
                                       systemSymbol:systemSymbol
                                   monochromeSymbol:NO
                            accessibilityIdentifier:accessibilityID
                                 enterpriseDisabled:NO
                                            handler:handlerWithMetrics];

  result.destination = static_cast<NSInteger>(destination);

  return result;
}
// Creates an OverflowMenuDestination to be displayed in the destinations
// carousel.
- (OverflowMenuDestination*)
    createOverflowMenuDestination:(int)nameID
                      destination:(overflow_menu::Destination)destination
                        imageName:(NSString*)imageName
                  accessibilityID:(NSString*)accessibilityID
                          handler:(Handler)handler {
  DCHECK(!UseSymbols());
  __weak __typeof(self) weakSelf = self;

  NSString* name = l10n_util::GetNSString(nameID);

  auto handlerWithMetrics = ^{
    overflow_menu::RecordUmaActionForDestination(destination);

    if (IsSmartSortingNewOverflowMenuEnabled()) {
      [weakSelf.destinationUsageHistory recordClickForDestination:destination];
    }

    handler();
  };

  OverflowMenuDestination* result = [[OverflowMenuDestination alloc]
                 initWithName:name
                        image:[UIImage imageNamed:imageName]
      accessibilityIdentifier:accessibilityID
           enterpriseDisabled:NO
                      handler:handlerWithMetrics];

  result.destination = static_cast<NSInteger>(destination);

  return result;
}

// Creates an OverflowMenuDestination for the Spotlight debugger.
- (OverflowMenuDestination*)destinationForSpotlightDebugger:(Handler)handler {
  OverflowMenuDestination* result = [[OverflowMenuDestination alloc]
                 initWithName:@"Spotlight Debugger"
                        image:[UIImage
                                  imageNamed:
                                      @"overflow_menu_destination_settings"]
      accessibilityIdentifier:@"Spotlight Debugger"
           enterpriseDisabled:NO
                      handler:handler];
  result.destination =
      static_cast<NSInteger>(overflow_menu::Destination::SpotlightDebugger);
  return result;
}

// Adds What's New to the OverflowMenuDestination to be displayed in the
// destinations carousel.
- (NSArray<OverflowMenuDestination*>*)insertWhatsNewToDestinations:
    (NSArray<OverflowMenuDestination*>*)destinations {
  NSMutableArray<OverflowMenuDestination*>* newDestinations =
      [[NSMutableArray alloc] init];

  if (WasWhatsNewUsed()) {
    // Place What's New at the bottom of the overflow menu carousel.
    [newDestinations addObjectsFromArray:destinations];
    [newDestinations addObject:self.whatsNewDestination];
    return newDestinations;
  }

  // Set the new label badge.
  self.whatsNewDestination.badge = BadgeTypeNewLabel;

  // Place What's New at the top of the overflow menucarousel.
  [newDestinations addObject:self.whatsNewDestination];
  [newDestinations addObjectsFromArray:destinations];

  return newDestinations;
}

// Decides whether the default browser blue dot promo should be active, and if
// it is, move the settings destination to 4th position and add the blue dot
// badge.
- (NSArray<OverflowMenuDestination*>*)maybeActivateDefaultBrowserBlueDotPromo:
    (NSArray<OverflowMenuDestination*>*)destinations {
  if (!self.engagementTracker) {
    return destinations;
  }

  if (ShouldTriggerDefaultBrowserBlueDotBadgeFeature(
          feature_engagement::kIPHiOSDefaultBrowserOverflowMenuBadgeFeature,
          self.engagementTracker)) {
    // Add the blue dot promo badge to the settings destination.
    self.settingsDestination.badge = BadgeTypeBlueDot;

    // Move the settings destination to the 4th position of the destinations,
    // otherwise respecting the original order.
    NSMutableArray<OverflowMenuDestination*>* newDestinations =
        [[NSMutableArray alloc] init];

    for (OverflowMenuDestination* destination in destinations) {
      if (destination == self.settingsDestination) {
        continue;
      }

      if ([newDestinations count] == 3) {
        [newDestinations addObject:self.settingsDestination];
      }

      [newDestinations addObject:destination];
    }

    // If we've only started showing the blue dot recently (<6 hours), don't
    // notify the FET again that the promo is being shown, since we're not in a
    // new user session. We record the badge being shown per user session,
    // instead of per time it is shown since the badge needs to be shown accross
    // 3 user sessions.
    if (!HasRecentTimestampForKey(
            kMostRecentTimestampBlueDotPromoShownInOverflowMenu)) {
      self.engagementTracker->NotifyEvent(
          feature_engagement::events::kBlueDotPromoOverflowMenuShownNewSession);
    }

    return newDestinations;
  }

  return destinations;
}

// Adds SpotlightDebugger to the OverflowMenuDestination to be displayed in the
// destinations carousel.
- (NSArray<OverflowMenuDestination*>*)insertSpotlightDebuggerToDestinations:
    (NSArray<OverflowMenuDestination*>*)destinations {
  DCHECK(IsSpotlightDebuggingEnabled());

  NSMutableArray<OverflowMenuDestination*>* newDestinations =
      [[NSMutableArray alloc] init];

  // Place the debugger at the top of the overflow menu carousel.
  [newDestinations addObject:self.spotlightDebuggerDestination];
  [newDestinations addObjectsFromArray:destinations];

  return newDestinations;
}

// Creates an NSArray containing the destinations contained in the overflow menu
// carousel.
- (NSArray<OverflowMenuDestination*>*)baseDestinations {
  NSMutableArray* baseDestinations = [[NSMutableArray alloc] initWithArray:@[
    self.bookmarksDestination,
    self.historyDestination,
    self.readingListDestination,
    self.passwordsDestination,
    self.downloadsDestination,
    self.recentTabsDestination,
    self.siteInfoDestination,
    self.settingsDestination,
  ]];

  if (self.webState &&
      IsPriceTrackingEnabled(ChromeBrowserState::FromBrowserState(
          self.webState->GetBrowserState())) &&
      IsSmartSortingPriceTrackingDestinationEnabled()) {
    [baseDestinations addObject:self.priceNotificationsDestination];
  }

  return baseDestinations;
}

// Make sure the model to match the current page state.
- (void)updateModel {
  // If the model hasn't been created, there's no need to update.
  if (!_overflowMenuModel) {
    return;
  }

  NSArray<OverflowMenuDestination*>* baseDestinations = [self baseDestinations];

  if (self.destinationUsageHistory && IsSmartSortingNewOverflowMenuEnabled()) {
    baseDestinations = [self.destinationUsageHistory
        sortedDestinationsFromCarouselDestinations:baseDestinations];
  }

  // What's New defies the smart sorting rules of the overflow menu to appear
  // either at the front of the carousel or the back. Thus, What's New is
  // inserted after smart sorting returns the sorted destinations.
  if (IsWhatsNewEnabled()) {
    baseDestinations = [self insertWhatsNewToDestinations:baseDestinations];
  }

  if (IsSpotlightDebuggingEnabled()) {
    baseDestinations =
        [self insertSpotlightDebuggerToDestinations:baseDestinations];
  }

  baseDestinations =
      [self maybeActivateDefaultBrowserBlueDotPromo:baseDestinations];

  self.overflowMenuModel.destinations = [baseDestinations
      filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(
                                                   id object,
                                                   NSDictionary* bindings) {
        if (object == self.siteInfoDestination) {
          return [self currentWebPageSupportsSiteInfo];
        }
        // All other destinations are displayed in regular mode.
        if (!self.isIncognito) {
          return true;
        }
        return object != self.historyDestination &&
               object != self.recentTabsDestination;
      }]];

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

  if (IsPinnedTabsOverflowEnabled()) {
    [appActions addObject:([self isTabPinned] ? self.unpinTabAction
                                              : self.pinTabAction)];
  }

  self.appActionsGroup.actions = appActions;

  BOOL pageIsBookmarked =
      self.webState && self.bookmarkModel &&
      self.bookmarkModel->IsBookmarked(self.webState->GetVisibleURL());

  NSMutableArray<OverflowMenuAction*>* pageActions =
      [[NSMutableArray alloc] init];

  // Try to create the followAction if there isn't one. It's possible that
  // sometimes when creating the model the followActionState is hidden so the
  // followAction hasn't been created but at the time when updating the model,
  // the followAction should be valid.
  if (!self.followAction) {
    self.followAction = [self createFollowActionIfNeeded];
    DCHECK(!self.followAction || self.webState != nullptr);
  }

  if (self.followAction) {
    [pageActions addObject:self.followAction];
    FollowTabHelper* followTabHelper =
        FollowTabHelper::FromWebState(self.webState);
    if (followTabHelper) {
      followTabHelper->UpdateFollowMenuItem();
    }
  }

  // Add actions before a possible Clear Browsing Data action.
  [pageActions addObjectsFromArray:@[
    (pageIsBookmarked) ? self.editBookmarkAction : self.addBookmarkAction,
    self.readLaterAction
  ]];

  // Clear Browsing Data Action is not relevant in incognito, so don't show it.
  // History is also hidden for similar reasons.
  if (!self.isIncognito) {
    [pageActions addObject:self.clearBrowsingDataAction];
  }

  // Add actions after a possible Clear Browsing Data action.
  [pageActions addObjectsFromArray:@[
    self.translateAction,
    ([self userAgentType] != web::UserAgentType::DESKTOP)
        ? self.requestDesktopAction
        : self.requestMobileAction,
    self.findInPageAction, self.textZoomAction
  ]];

  self.pageActionsGroup.actions = pageActions;

  NSMutableArray<OverflowMenuAction*>* helpActions =
      [[NSMutableArray alloc] init];

  if (ios::provider::IsUserFeedbackSupported()) {
    [helpActions addObject:self.reportIssueAction];
  }

  [helpActions addObject:self.helpAction];

  if (IsNewOverflowMenuShareChromeActionEnabled()) {
    [helpActions addObject:self.shareChromeAction];
  }

  self.helpActionsGroup.actions = helpActions;

  // Set footer (on last section), if any.
  if (_browserPolicyConnector &&
      _browserPolicyConnector->HasMachineLevelPolicies()) {
    self.helpActionsGroup.footer = CreateOverflowMenuManagedFooter(
        IDS_IOS_TOOLS_MENU_ENTERPRISE_MANAGED,
        IDS_IOS_TOOLS_MENU_ENTERPRISE_LEARN_MORE,
        @"overflow_menu_footer_managed", ^{
          [self enterpriseLearnMore];
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
      IsIncognitoModeForced(self.browserStatePrefs);
  self.openIncognitoTabAction.enterpriseDisabled =
      IsIncognitoModeDisabled(self.browserStatePrefs);

  // Set badges if necessary
  if (self.engagementTracker &&
      self.engagementTracker->ShouldTriggerHelpUI(
          feature_engagement::kIPHBadgedReadingListFeature)) {
    self.readingListDestination.badge = BadgeTypeBlueDot;
  }
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
  return self.browserStatePrefs->GetBoolean(
      bookmarks::prefs::kEditBookmarksEnabled);
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

  OverflowMenuAction* action = [[OverflowMenuAction alloc]
                 initWithName:l10n_util::GetNSStringF(IDS_IOS_TOOLS_MENU_FOLLOW,
                                                      u"")
                        image:[UIImage
                                  imageNamed:@"overflow_menu_action_follow"]
      accessibilityIdentifier:kToolsMenuFollow
           enterpriseDisabled:NO
                      handler:^{
                      }];
  action.enabled = NO;
  return action;
}

// Returns 'YES' if the current tab is pinned.
- (BOOL)isTabPinned {
  DCHECK(self.webState);
  DCHECK(self.webStateList);

  int webStateIndex = self.webStateList->GetIndexOfWebState(self.webState);
  return self.webStateList->IsWebStatePinnedAt(webStateIndex);
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

#pragma mark - WebStateObserving

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(ActiveWebStateChangeReason)reason {
  self.webState = newWebState;
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
- (void)bookmarkNodeChildrenChanged:
    (const bookmarks::BookmarkNode*)bookmarkNode {
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

- (void)bookmarkNodeChanged:(const bookmarks::BookmarkNode*)bookmarkNode {
  [self updateModel];
}
- (void)bookmarkNode:(const bookmarks::BookmarkNode*)bookmarkNode
     movedFromParent:(const bookmarks::BookmarkNode*)oldParent
            toParent:(const bookmarks::BookmarkNode*)newParent {
  // No-op -- required by BookmarkModelBridgeObserver but not used.
}
- (void)bookmarkNodeDeleted:(const bookmarks::BookmarkNode*)node
                 fromFolder:(const bookmarks::BookmarkNode*)folder {
  [self updateModel];
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
    self.followAction.storedImage =
        [UIImage imageNamed:@"overflow_menu_action_unfollow"];
    self.followAction.handler = ^{
      [weakSelf unfollowWebPage:webPageURLs];
    };
  } else {
    __weak __typeof(self) weakSelf = self;
    self.followAction.name = l10n_util::GetNSStringF(
        IDS_IOS_TOOLS_MENU_FOLLOW, base::SysNSStringToUTF16(domainName));
    self.followAction.storedImage =
        [UIImage imageNamed:@"overflow_menu_action_follow"];
    self.followAction.handler = ^{
      [weakSelf followWebPage:webPageURLs];
    };
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

#pragma mark - Action handlers

// Dismisses the menu and reloads the current page.
- (void)reload {
  RecordAction(UserMetricsAction("MobileMenuReload"));
  [self.popupMenuCommandsHandler dismissPopupMenuAnimated:YES];
  self.navigationAgent->Reload();
}

// Dismisses the menu and stops the current page load.
- (void)stopLoading {
  RecordAction(UserMetricsAction("MobileMenuStop"));
  [self.popupMenuCommandsHandler dismissPopupMenuAnimated:YES];
  self.navigationAgent->StopLoading();
}

// Dismisses the menu and opens a new tab.
- (void)openTab {
  RecordAction(UserMetricsAction("MobileMenuNewTab"));
  RecordAction(UserMetricsAction("MobileTabNewTab"));

  [self.popupMenuCommandsHandler dismissPopupMenuAnimated:YES];
  [self.dispatcher openURLInNewTab:[OpenNewTabCommand commandWithIncognito:NO]];
}

// Dismisses the menu and opens a new incognito tab.
- (void)openIncognitoTab {
  RecordAction(UserMetricsAction("MobileMenuNewIncognitoTab"));
  [self.popupMenuCommandsHandler dismissPopupMenuAnimated:YES];
  [self.dispatcher
      openURLInNewTab:[OpenNewTabCommand commandWithIncognito:YES]];
}

// Dismisses the menu and opens a new window.
- (void)openNewWindow {
  RecordAction(UserMetricsAction("MobileMenuNewWindow"));
  [self.popupMenuCommandsHandler dismissPopupMenuAnimated:YES];
  [self.dispatcher
      openNewWindowWithActivity:ActivityToLoadURL(WindowActivityToolsOrigin,
                                                  GURL(kChromeUINewTabURL))];
}

// Dismisses the menu and pins the tab.
- (void)pinTab {
  [[self class] setTabPinned:YES
                    webState:self.webState
                webStateList:self.webStateList];
  [self.popupMenuCommandsHandler showSnackbarForPinnedState:YES
                                                   webState:self.webState];
  [self.popupMenuCommandsHandler dismissPopupMenuAnimated:YES];
}

// Dismisses the menu and unpins the tab.
- (void)unpinTab {
  [[self class] setTabPinned:NO
                    webState:self.webState
                webStateList:self.webStateList];
  [self.popupMenuCommandsHandler showSnackbarForPinnedState:NO
                                                   webState:self.webState];
  [self.popupMenuCommandsHandler dismissPopupMenuAnimated:YES];
}

// Dismisses the menu and opens the Clear Browsing Data screen.
- (void)openClearBrowsingData {
  RecordAction(UserMetricsAction("MobileMenuClearBrowsingData"));
  [self.popupMenuCommandsHandler dismissPopupMenuAnimated:YES];
  [self.dispatcher showClearBrowsingDataSettings];
}

// Follows the website corresponding to `webPage` and dismisses the menu.
- (void)followWebPage:(WebPageURLs*)webPage {
  // FollowBrowserAgent may be null after -disconnect has been called.
  FollowBrowserAgent* followBrowserAgent = self.followBrowserAgent;
  if (followBrowserAgent)
    followBrowserAgent->FollowWebSite(webPage, FollowSource::OverflowMenu);
  [self.popupMenuCommandsHandler dismissPopupMenuAnimated:YES];
}

// Unfollows the website corresponding to `webPage` and dismisses the menu.
- (void)unfollowWebPage:(WebPageURLs*)webPage {
  // FollowBrowserAgent may be null after -disconnect has been called.
  FollowBrowserAgent* followBrowserAgent = self.followBrowserAgent;
  if (followBrowserAgent)
    followBrowserAgent->UnfollowWebSite(webPage, FollowSource::OverflowMenu);
  [self.popupMenuCommandsHandler dismissPopupMenuAnimated:YES];
}

// Dismisses the menu and adds the current page as a bookmark or opens the
// bookmark edit screen if the current page is bookmarked.
- (void)addOrEditBookmark {
  RecordAction(UserMetricsAction("MobileMenuAddToBookmarks"));
  // Dismissing the menu disconnects the mediator, so save anything cleaned up
  // there.
  web::WebState* currentWebState = self.webState;
  [self.popupMenuCommandsHandler dismissPopupMenuAnimated:YES];
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);
  if (!currentWebState) {
    return;
  }
  BookmarkAddCommand* command =
      [[BookmarkAddCommand alloc] initWithWebState:currentWebState
                              presentFolderChooser:NO];
  [self.bookmarksCommandsHandler bookmark:command];
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
  reading_list::AddToReadingListUsingCanonicalUrl(self.dispatcher, webState);

  [self.popupMenuCommandsHandler dismissPopupMenuAnimated:YES];
}

// Dismisses the menu and starts translating the current page.
- (void)translatePage {
  base::RecordAction(UserMetricsAction("MobileMenuTranslate"));
  [self.popupMenuCommandsHandler dismissPopupMenuAnimated:YES];
  [self.dispatcher showTranslate];
}

// Dismisses the menu and requests the desktop version of the current page
- (void)requestDesktopSite {
  RecordAction(UserMetricsAction("MobileMenuRequestDesktopSite"));
  [self.popupMenuCommandsHandler dismissPopupMenuAnimated:YES];
  self.navigationAgent->RequestDesktopSite();
  [self.dispatcher showDefaultSiteViewIPH];
}

// Dismisses the menu and requests the mobile version of the current page
- (void)requestMobileSite {
  RecordAction(UserMetricsAction("MobileMenuRequestMobileSite"));
  [self.popupMenuCommandsHandler dismissPopupMenuAnimated:YES];
  self.navigationAgent->RequestMobileSite();
}

// Dismisses the menu and opens Find In Page
- (void)openFindInPage {
  RecordAction(UserMetricsAction("MobileMenuFindInPage"));
  [self.popupMenuCommandsHandler dismissPopupMenuAnimated:YES];
  [self.dispatcher openFindInPage];
}

// Dismisses the menu and opens Text Zoom
- (void)openTextZoom {
  RecordAction(UserMetricsAction("MobileMenuTextZoom"));
  [self.popupMenuCommandsHandler dismissPopupMenuAnimated:YES];
  [self.dispatcher openTextZoom];
}

// Dismisses the menu and opens the Report an Issue screen.
- (void)reportAnIssue {
  RecordAction(UserMetricsAction("MobileMenuReportAnIssue"));
  [self.popupMenuCommandsHandler dismissPopupMenuAnimated:YES];
  [self.dispatcher
      showReportAnIssueFromViewController:self.baseViewController
                                   sender:UserFeedbackSender::ToolsMenu];
}

// Dismisses the menu and opens the help screen.
- (void)openHelp {
  RecordAction(UserMetricsAction("MobileMenuHelp"));
  [self.popupMenuCommandsHandler dismissPopupMenuAnimated:YES];
  [self.dispatcher showHelpPage];
}

#pragma mark - Destinations Handlers

// Dismisses the menu and opens bookmarks.
- (void)openBookmarks {
  [self.popupMenuCommandsHandler dismissPopupMenuAnimated:YES];
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);
  [self.dispatcher showBookmarksManager];
}

// Dismisses the menu and opens share sheet to share Chrome's app store link
- (void)shareChromeApp {
  [self.popupMenuCommandsHandler dismissPopupMenuAnimated:YES];
  [self.dispatcher shareChromeApp];
}

// Dismisses the menu and opens history.
- (void)openHistory {
  [self.popupMenuCommandsHandler dismissPopupMenuAnimated:YES];
  [self.dispatcher showHistory];
}

// Dismisses the menu and opens reading list.
- (void)openReadingList {
  [self.popupMenuCommandsHandler dismissPopupMenuAnimated:YES];
  // TODO(crbug.com/906662): This will need to be called on the
  // BrowserCoordinatorCommands handler.
  [self.dispatcher showReadingList];
}

// Dismisses the menu and opens password list.
- (void)openPasswords {
  [self.popupMenuCommandsHandler dismissPopupMenuAnimated:YES];
  [self.dispatcher
      showSavedPasswordsSettingsFromViewController:self.baseViewController
                                  showCancelButton:NO];
}

// Dismisses the menu and opens price notifications list.
- (void)openPriceNotifications {
  RecordAction(UserMetricsAction("MobileMenuPriceNotifications"));
  _engagementTracker->NotifyEvent(
      feature_engagement::events::kPriceNotificationsUsed);
  [self.popupMenuCommandsHandler dismissPopupMenuAnimated:YES];
  [self.dispatcher showPriceNotifications];
}

// Dismisses the menu and opens downloads.
- (void)openDownloads {
  [self.popupMenuCommandsHandler dismissPopupMenuAnimated:YES];
  profile_metrics::BrowserProfileType type =
      self.isIncognito ? profile_metrics::BrowserProfileType::kIncognito
                       : profile_metrics::BrowserProfileType::kRegular;
  UmaHistogramEnumeration("Download.OpenDownloadsFromMenu.PerProfileType",
                          type);
  // TODO(crbug.com/906662): This will need to be called on the
  // BrowserCoordinatorCommands handler.
  [self.dispatcher showDownloadsFolder];
}

// Dismisses the menu and opens recent tabs.
- (void)openRecentTabs {
  [self.popupMenuCommandsHandler dismissPopupMenuAnimated:YES];
  // TODO(crbug.com/906662): This will need to be called on the
  // BrowserCoordinatorCommands handler.
  [self.dispatcher showRecentTabs];
}

// Dismisses the menu and shows page information.
- (void)openSiteInformation {
  [self.popupMenuCommandsHandler dismissPopupMenuAnimated:YES];
  [self.pageInfoCommandsHandler showPageInfo];
}

// Dismisses the menu and opens What's New.
- (void)openWhatsNew {
  if (!WasWhatsNewUsed()) {
    SetWhatsNewUsed();
  }

  if (self.engagementTracker) {
    self.engagementTracker->NotifyEvent(
        feature_engagement::events::kViewedWhatsNew);
  }
  [self.popupMenuCommandsHandler dismissPopupMenuAnimated:YES];
  [self.dispatcher showWhatsNew];
}

// Dismisses the menu and opens settings.
- (void)openSettings {
  if (self.settingsDestination.badge == BadgeTypeBlueDot &&
      self.engagementTracker) {
    self.engagementTracker->NotifyEvent(
        feature_engagement::events::kBlueDotPromoOverflowMenuDismissed);
  }
  [self.popupMenuCommandsHandler dismissPopupMenuAnimated:YES];
  profile_metrics::BrowserProfileType type =
      self.isIncognito ? profile_metrics::BrowserProfileType::kIncognito
                       : profile_metrics::BrowserProfileType::kRegular;
  UmaHistogramEnumeration("Settings.OpenSettingsFromMenu.PerProfileType", type);
  [self.dispatcher showSettingsFromViewController:self.baseViewController];
}

- (void)enterpriseLearnMore {
  [self.popupMenuCommandsHandler dismissPopupMenuAnimated:YES];
  [self.dispatcher
      openURLInNewTab:[OpenNewTabCommand commandWithURLFromChrome:
                                             GURL(kChromeUIManagementURL)]];
}

- (void)openSpotlightDebugger {
  DCHECK(IsSpotlightDebuggingEnabled());
  [self.popupMenuCommandsHandler dismissPopupMenuAnimated:YES];
  [self.dispatcher showSpotlightDebugger];
}

@end
