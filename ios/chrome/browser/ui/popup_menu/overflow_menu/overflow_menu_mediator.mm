// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_mediator.h"

#import "base/ios/ios_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/language/ios/browser/ios_language_detection_tab_helper.h"
#import "components/language/ios/browser/ios_language_detection_tab_helper_observer_bridge.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/find_in_page/find_tab_helper.h"
#import "ios/chrome/browser/follow/follow_java_script_feature.h"
#import "ios/chrome/browser/overlays/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/public/overlay_presenter_observer_bridge.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#include "ios/chrome/browser/policy/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/policy_util.h"
#include "ios/chrome/browser/reading_list/offline_url_utils.h"
#import "ios/chrome/browser/translate/chrome_ios_translate_client.h"
#import "ios/chrome/browser/ui/activity_services/canonical_url_retriever.h"
#include "ios/chrome/browser/ui/bookmarks/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/find_in_page_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/commands/reading_list_add_command.h"
#import "ios/chrome/browser/ui/commands/text_zoom_commands.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_utils.h"
#import "ios/chrome/browser/ui/follow/follow_site_info.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/feature_flags.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_swift.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/web/font_size/font_size_tab_helper.h"
#import "ios/chrome/browser/web/web_navigation_browser_agent.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/window_activities/window_activity_helpers.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/follow/follow_provider.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_provider.h"
#include "ios/web/common/user_agent.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::RecordAction;
using base::UmaHistogramEnumeration;
using base::UserMetricsAction;

namespace {

typedef void (^Handler)(void);

OverflowMenuAction* CreateOverflowMenuAction(int nameID,
                                             NSString* imageName,
                                             NSString* accessibilityID,
                                             Handler handler) {
  NSString* name = l10n_util::GetNSString(nameID);
  return [[OverflowMenuAction alloc] initWithName:name
                                        imageName:imageName
                          accessibilityIdentifier:accessibilityID
                               enterpriseDisabled:NO
                                          handler:handler];
}

OverflowMenuDestination* CreateOverflowMenuDestination(
    int nameID,
    NSString* imageName,
    NSString* accessibilityID,
    Handler handler) {
  NSString* name = l10n_util::GetNSString(nameID);
  return [[OverflowMenuDestination alloc] initWithName:name
                                             imageName:imageName
                               accessibilityIdentifier:accessibilityID
                                    enterpriseDisabled:NO
                                               handler:handler];
}

OverflowMenuFooter* CreateOverflowMenuManagedFooter(int nameID,
                                                    int linkID,
                                                    NSString* imageName,
                                                    Handler handler) {
  NSString* name = l10n_util::GetNSString(nameID);
  NSString* link = l10n_util::GetNSString(linkID);
  return [[OverflowMenuFooter alloc] initWithName:name
                                             link:link
                                        imageName:imageName
                          accessibilityIdentifier:kTextMenuEnterpriseInfo
                                          handler:handler];
}

}  // namespace

@interface OverflowMenuMediator () <BookmarkModelBridgeObserver,
                                    CRWWebStateObserver,
                                    IOSLanguageDetectionTabHelperObserving,
                                    OverlayPresenterObserving,
                                    PrefObserverDelegate,
                                    WebStateListObserving> {
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;

  // Observer for the content area overlay events
  std::unique_ptr<OverlayPresenterObserver> _overlayPresenterObserver;

  // Bridge to register for bookmark changes.
  std::unique_ptr<bookmarks::BookmarkModelBridge> _bookmarkModelBridge;

  // Bridge to get notified of the language detection event.
  std::unique_ptr<language::IOSLanguageDetectionTabHelperObserverBridge>
      _iOSLanguageDetectionTabHelperObserverBridge;

  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  std::unique_ptr<PrefChangeRegistrar> _prefChangeRegistrar;
}

// The current web state.
@property(nonatomic, assign) web::WebState* webState;

// The current web site information.
@property(nonatomic, strong) FollowSiteInfo* siteInfo;

// Whether an overlay is currently presented over the web content area.
@property(nonatomic, assign) BOOL webContentAreaShowingOverlay;

// Whether the web content is currently being blocked.
@property(nonatomic, assign) BOOL contentBlocked;

@property(nonatomic, strong) OverflowMenuDestination* bookmarksDestination;
@property(nonatomic, strong) OverflowMenuDestination* downloadsDestination;
@property(nonatomic, strong) OverflowMenuDestination* historyDestination;
@property(nonatomic, strong) OverflowMenuDestination* passwordsDestination;
@property(nonatomic, strong) OverflowMenuDestination* readingListDestination;
@property(nonatomic, strong) OverflowMenuDestination* recentTabsDestination;
@property(nonatomic, strong) OverflowMenuDestination* settingsDestination;
@property(nonatomic, strong) OverflowMenuDestination* siteInfoDestination;

@property(nonatomic, strong) OverflowMenuActionGroup* appActionsGroup;
@property(nonatomic, strong) OverflowMenuActionGroup* pageActionsGroup;
@property(nonatomic, strong) OverflowMenuActionGroup* helpActionsGroup;

@property(nonatomic, strong) OverflowMenuAction* reloadAction;
@property(nonatomic, strong) OverflowMenuAction* stopLoadAction;
@property(nonatomic, strong) OverflowMenuAction* openTabAction;
@property(nonatomic, strong) OverflowMenuAction* openIncognitoTabAction;
@property(nonatomic, strong) OverflowMenuAction* openNewWindowAction;

@property(nonatomic, strong) OverflowMenuAction* followAction;
@property(nonatomic, strong) OverflowMenuAction* unfollowAction;
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

@end

@implementation OverflowMenuMediator

@synthesize overflowMenuModel = _overflowMenuModel;

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
    if (self.readingListDestination.showBadge) {
      _engagementTracker->Dismissed(
          feature_engagement::kIPHBadgedReadingListFeature);
    }

    _engagementTracker = nullptr;
  }

  self.webState = nullptr;
  self.webStateList = nullptr;
  self.siteInfo = nil;

  self.bookmarkModel = nullptr;
  self.prefService = nullptr;
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

    // Observe the language::IOSLanguageDetectionTabHelper for |_webState|.
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

  if (_webStateList) {
    _webStateList->AddObserver(_webStateListObserver.get());
  }
}

- (void)setBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel {
  _bookmarkModelBridge.reset();

  _bookmarkModel = bookmarkModel;

  if (bookmarkModel) {
    _bookmarkModelBridge =
        std::make_unique<bookmarks::BookmarkModelBridge>(self, bookmarkModel);
  }

  [self updateModel];
}

- (void)setPrefService:(PrefService*)prefService {
  _prefObserverBridge.reset();
  _prefChangeRegistrar.reset();

  _prefService = prefService;

  if (_prefService) {
    _prefChangeRegistrar = std::make_unique<PrefChangeRegistrar>();
    _prefChangeRegistrar->Init(prefService);
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

#pragma mark - Model Creation

- (OverflowMenuModel*)createModel {
  __weak __typeof(self) weakSelf = self;

  self.bookmarksDestination = CreateOverflowMenuDestination(
      IDS_IOS_TOOLS_MENU_BOOKMARKS, @"overflow_menu_destination_bookmarks",
      kToolsMenuBookmarksId, ^{
        [weakSelf openBookmarks];
      });
  self.downloadsDestination = CreateOverflowMenuDestination(
      IDS_IOS_TOOLS_MENU_DOWNLOADS, @"overflow_menu_destination_downloads",
      kToolsMenuDownloadsId, ^{
        [weakSelf openDownloads];
      });
  self.historyDestination = CreateOverflowMenuDestination(
      IDS_IOS_TOOLS_MENU_HISTORY, @"overflow_menu_destination_history",
      kToolsMenuHistoryId, ^{
        [weakSelf openHistory];
      });

  int passwordTitleID = IsPasswordManagerBrandingUpdateEnabled()
                            ? IDS_IOS_TOOLS_MENU_PASSWORD_MANAGER
                            : IDS_IOS_TOOLS_MENU_PASSWORDS;
  NSString* passwordIconImageName =
      IsPasswordManagerBrandingUpdateEnabled()
          ? @"overflow_menu_destination_passwords_rebrand"
          : @"overflow_menu_destination_passwords";
  self.passwordsDestination = CreateOverflowMenuDestination(
      passwordTitleID, passwordIconImageName, @"", ^{
        [weakSelf openPasswords];
      });

  self.readingListDestination = CreateOverflowMenuDestination(
      IDS_IOS_TOOLS_MENU_READING_LIST,
      @"overflow_menu_destination_reading_list", kToolsMenuReadingListId, ^{
        [weakSelf openReadingList];
      });
  self.recentTabsDestination = CreateOverflowMenuDestination(
      IDS_IOS_TOOLS_MENU_RECENT_TABS, @"overflow_menu_destination_recent_tabs",
      kToolsMenuOtherDevicesId, ^{
        [weakSelf openRecentTabs];
      });
  self.settingsDestination = CreateOverflowMenuDestination(
      IDS_IOS_TOOLS_MENU_SETTINGS, @"overflow_menu_destination_settings",
      kToolsMenuSettingsId, ^{
        [weakSelf openSettings];
      });
  self.siteInfoDestination = CreateOverflowMenuDestination(
      IDS_IOS_TOOLS_MENU_SITE_INFORMATION,
      @"overflow_menu_destination_site_info", kToolsMenuSiteInformation, ^{
        [weakSelf openSiteInformation];
      });

  self.reloadAction = CreateOverflowMenuAction(IDS_IOS_TOOLS_MENU_RELOAD,
                                               @"overflow_menu_action_reload",
                                               kToolsMenuReload, ^{
                                                 [weakSelf reload];
                                               });
  self.stopLoadAction = CreateOverflowMenuAction(
      IDS_IOS_TOOLS_MENU_STOP, @"overflow_menu_action_stop", kToolsMenuStop, ^{
        [weakSelf stopLoading];
      });

  self.openTabAction = CreateOverflowMenuAction(IDS_IOS_TOOLS_MENU_NEW_TAB,
                                                @"overflow_menu_action_new_tab",
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

  // The app actions vary based on page state, so they are set in
  // |-updateModel|.
  self.appActionsGroup =
      [[OverflowMenuActionGroup alloc] initWithGroupName:@"app_actions"
                                                 actions:@[]
                                                  footer:nil];

  self.followAction = CreateOverflowMenuAction(
      IDS_IOS_TOOLS_MENU_FOLLOW, @"overflow_menu_action_follow",
      kToolsMenuFollow, ^{
        [weakSelf updateFollowStatus:YES];
      });
  self.unfollowAction = CreateOverflowMenuAction(
      IDS_IOS_TOOLS_MENU_UNFOLLOW, @"overflow_menu_action_unfollow",
      kToolsMenuUnfollow, ^{
        [weakSelf updateFollowStatus:NO];
      });
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
  [self logTranslateAvailability];
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

  // The page actions vary based on page state, so they are set in
  // |-updateModel|.
  self.pageActionsGroup =
      [[OverflowMenuActionGroup alloc] initWithGroupName:@"page_actions"
                                                 actions:@[]
                                                  footer:nil];

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

// Make sure the model to match the current page state.
- (void)updateModel {
  // If the model hasn't been created, there's no need to update.
  if (!_overflowMenuModel) {
    return;
  }

  NSArray<OverflowMenuDestination*>* baseDestinations = @[
    self.bookmarksDestination,
    self.historyDestination,
    self.readingListDestination,
    self.passwordsDestination,
    self.downloadsDestination,
    self.recentTabsDestination,
    self.siteInfoDestination,
    self.settingsDestination,
  ];

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

  self.appActionsGroup.actions = appActions;

  BOOL pageIsBookmarked =
      self.webState && self.bookmarkModel &&
      self.bookmarkModel->IsBookmarked(self.webState->GetVisibleURL());
  self.pageActionsGroup.actions = @[
    (pageIsBookmarked) ? self.editBookmarkAction : self.addBookmarkAction,
    self.readLaterAction, self.translateAction,
    ([self userAgentType] != web::UserAgentType::DESKTOP)
        ? self.requestDesktopAction
        : self.requestMobileAction,
    self.findInPageAction, self.textZoomAction
  ];

  // Add the follow/unfollow action.
  if (self.followActionState != FollowActionStateHidden) {
    DCHECK(IsWebChannelsEnabled());
    __weak __typeof(self) weakSelf = self;
    FollowJavaScriptFeature::GetInstance()->GetFollowSiteInfo(
        self.webState, base::BindOnce(^(FollowSiteInfo* siteInfo) {
          if (siteInfo) {
            OverflowMenuMediator* strongSelf = weakSelf;
            if (!strongSelf) {
              return;
            }
            strongSelf.siteInfo = siteInfo;
            BOOL siteFollowed = ios::GetChromeBrowserProvider()
                                    .GetFollowProvider()
                                    ->GetFollowStatus(siteInfo);
            if (!siteFollowed) {
              strongSelf.pageActionsGroup.actions =
                  [@[ strongSelf.followAction ]
                      arrayByAddingObjectsFromArray:strongSelf.pageActionsGroup
                                                        .actions];
            } else {
              strongSelf.pageActionsGroup.actions =
                  [@[ strongSelf.unfollowAction ]
                      arrayByAddingObjectsFromArray:strongSelf.pageActionsGroup
                                                        .actions];
            }
          }
        }));
  }

  NSMutableArray<OverflowMenuAction*>* helpActions =
      [[NSMutableArray alloc] init];

  if (ios::GetChromeBrowserProvider()
          .GetUserFeedbackProvider()
          ->IsUserFeedbackEnabled()) {
    [helpActions addObject:self.reportIssueAction];
  }

  [helpActions addObject:self.helpAction];

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

  // Enable/disable items based on page state.

  // The "Add to Reading List" functionality requires JavaScript execution,
  // which is paused while overlays are displayed over the web content area.
  self.readLaterAction.enabled =
      !self.webContentAreaShowingOverlay && [self isCurrentURLWebURL];

  BOOL followEnabled = self.followActionState == FollowActionStateEnabled;
  self.followAction.enabled = followEnabled;
  self.unfollowAction.enabled = followEnabled;
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
      IsIncognitoModeForced(self.prefService);
  self.openIncognitoTabAction.enterpriseDisabled =
      IsIncognitoModeDisabled(self.prefService);

  // Set badges if necessary
  self.readingListDestination.showBadge =
      self.engagementTracker &&
      self.engagementTracker->ShouldTriggerHelpUI(
          feature_engagement::kIPHBadgedReadingListFeature);
}

// Returns whether the page can be manually translated. If |forceMenuLogging| is
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

  auto* helper = FindTabHelper::FromWebState(self.webState);
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
  return self.prefService->GetBoolean(bookmarks::prefs::kEditBookmarksEnabled);
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
  [self.dispatcher dismissPopupMenuAnimated:YES];
  self.navigationAgent->Reload();
}

// Dismisses the menu and stops the current page load.
- (void)stopLoading {
  RecordAction(UserMetricsAction("MobileMenuStop"));
  [self.dispatcher dismissPopupMenuAnimated:YES];
  self.navigationAgent->StopLoading();
}

// Dismisses the menu and opens a new tab.
- (void)openTab {
  RecordAction(UserMetricsAction("MobileMenuNewTab"));
  [self.dispatcher dismissPopupMenuAnimated:YES];
  [self.dispatcher openURLInNewTab:[OpenNewTabCommand commandWithIncognito:NO]];
}

// Dismisses the menu and opens a new incognito tab.
- (void)openIncognitoTab {
  RecordAction(UserMetricsAction("MobileMenuNewIncognitoTab"));
  [self.dispatcher dismissPopupMenuAnimated:YES];
  [self.dispatcher
      openURLInNewTab:[OpenNewTabCommand commandWithIncognito:YES]];
}

// Dismisses the menu and opens a new window.
- (void)openNewWindow {
  RecordAction(UserMetricsAction("MobileMenuNewWindow"));
  [self.dispatcher dismissPopupMenuAnimated:YES];
  [self.dispatcher
      openNewWindowWithActivity:ActivityToLoadURL(WindowActivityToolsOrigin,
                                                  GURL(kChromeUINewTabURL))];
}

// Updates the follow status of the website to |followStatus|, and dismisses the
// menu.
- (void)updateFollowStatus:(BOOL)followStatus {
  if (followStatus) {
    ios::GetChromeBrowserProvider().GetFollowProvider()->UpdateFollowStatus(
        self.siteInfo, YES);
  }
  // TODO(crbug.com/1264872): add implementation when the followStatus is NO.
  [self.dispatcher dismissPopupMenuAnimated:YES];
}

// Dismisses the menu and adds the current page as a bookmark or opens the
// bookmark edit screen if the current page is bookmarked.
- (void)addOrEditBookmark {
  RecordAction(UserMetricsAction("MobileMenuAddToBookmarks"));
  [self.dispatcher dismissPopupMenuAnimated:YES];
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);
  [self.dispatcher bookmarkCurrentPage];
}

// Dismisses the menu and adds the current page to the reading list.
- (void)addToReadingList {
  RecordAction(UserMetricsAction("MobileMenuReadLater"));

  // Dismissing the menu disconnects the mediator, so save anything cleaned up
  // there.
  web::WebState* webState = self.webState;
  [self.dispatcher dismissPopupMenuAnimated:YES];

  if (!webState) {
    return;
  }

  // The mediator can be destroyed when this callback is executed. So it is not
  // possible to use a weak self.
  __weak id<BrowserCommands> weakDispatcher = self.dispatcher;
  GURL visibleURL = webState->GetVisibleURL();
  NSString* title = base::SysUTF16ToNSString(webState->GetTitle());
  activity_services::RetrieveCanonicalUrl(webState, ^(const GURL& URL) {
    const GURL& pageURL = !URL.is_empty() ? URL : visibleURL;
    if (!pageURL.is_valid() || !pageURL.SchemeIsHTTPOrHTTPS()) {
      return;
    }

    ReadingListAddCommand* command =
        [[ReadingListAddCommand alloc] initWithURL:pageURL title:title];
    [weakDispatcher addToReadingList:command];
  });
}

// Dismisses the menu and starts translating the current page.
- (void)translatePage {
  base::RecordAction(UserMetricsAction("MobileMenuTranslate"));
  [self.dispatcher dismissPopupMenuAnimated:YES];
  [self.dispatcher showTranslate];
}

// Dismisses the menu and requests the desktop version of the current page
- (void)requestDesktopSite {
  RecordAction(UserMetricsAction("MobileMenuRequestDesktopSite"));
  [self.dispatcher dismissPopupMenuAnimated:YES];
  self.navigationAgent->RequestDesktopSite();
  [self.dispatcher showDefaultSiteViewIPH];
}

// Dismisses the menu and requests the mobile version of the current page
- (void)requestMobileSite {
  RecordAction(UserMetricsAction("MobileMenuRequestMobileSite"));
  [self.dispatcher dismissPopupMenuAnimated:YES];
  self.navigationAgent->RequestMobileSite();
}

// Dismisses the menu and opens Find In Page
- (void)openFindInPage {
  RecordAction(UserMetricsAction("MobileMenuFindInPage"));
  [self.dispatcher dismissPopupMenuAnimated:YES];
  [self.dispatcher openFindInPage];
}

// Dismisses the menu and opens Text Zoom
- (void)openTextZoom {
  RecordAction(UserMetricsAction("MobileMenuTextZoom"));
  [self.dispatcher dismissPopupMenuAnimated:YES];
  [self.dispatcher openTextZoom];
}

// Dismisses the menu and opens the Report an Issue screen.
- (void)reportAnIssue {
  RecordAction(UserMetricsAction("MobileMenuReportAnIssue"));
  [self.dispatcher dismissPopupMenuAnimated:YES];
  [self.dispatcher
      showReportAnIssueFromViewController:self.baseViewController
                                   sender:UserFeedbackSender::ToolsMenu];
}

// Dismisses the menu and opens the help screen.
- (void)openHelp {
  RecordAction(UserMetricsAction("MobileMenuHelp"));
  [self.dispatcher dismissPopupMenuAnimated:YES];
  [self.dispatcher showHelpPage];
}

#pragma mark - Destinations Handlers

// Dismisses the menu and opens bookmarks.
- (void)openBookmarks {
  [self.dispatcher dismissPopupMenuAnimated:YES];
  RecordAction(UserMetricsAction("MobileMenuAllBookmarks"));
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);
  [self.dispatcher showBookmarksManager];
}

// Dismisses the menu and opens history.
- (void)openHistory {
  [self.dispatcher dismissPopupMenuAnimated:YES];
  RecordAction(UserMetricsAction("MobileMenuHistory"));
  [self.dispatcher showHistory];
}

// Dismisses the menu and opens reading list.
- (void)openReadingList {
  [self.dispatcher dismissPopupMenuAnimated:YES];
  RecordAction(UserMetricsAction("MobileMenuReadingList"));
  [self.dispatcher showReadingList];
}

// Dismisses the menu and opens password list.
- (void)openPasswords {
  [self.dispatcher dismissPopupMenuAnimated:YES];
  RecordAction(UserMetricsAction("MobileMenuPasswords"));
  [self.dispatcher
      showSavedPasswordsSettingsFromViewController:self.baseViewController
                                  showCancelButton:NO];
}

// Dismisses the menu and opens downloads.
- (void)openDownloads {
  [self.dispatcher dismissPopupMenuAnimated:YES];
  RecordAction(UserMetricsAction("MobileDownloadFolderUIShownFromToolsMenu"));
  profile_metrics::BrowserProfileType type =
      self.isIncognito ? profile_metrics::BrowserProfileType::kIncognito
                       : profile_metrics::BrowserProfileType::kRegular;
  UmaHistogramEnumeration("Download.OpenDownloadsFromMenu.PerProfileType",
                          type);
  [self.dispatcher showDownloadsFolder];
}

// Dismisses the menu and opens recent tabs.
- (void)openRecentTabs {
  [self.dispatcher dismissPopupMenuAnimated:YES];
  RecordAction(UserMetricsAction("MobileMenuRecentTabs"));
  [self.dispatcher showRecentTabs];
}

// Dismisses the menu and shows page information.
- (void)openSiteInformation {
  [self.dispatcher dismissPopupMenuAnimated:YES];
  RecordAction(UserMetricsAction("MobileMenuSiteInformation"));
  [self.dispatcher showPageInfo];
}

// Dismisses the menu and opens settings.
- (void)openSettings {
  [self.dispatcher dismissPopupMenuAnimated:YES];
  RecordAction(UserMetricsAction("MobileMenuSettings"));
  profile_metrics::BrowserProfileType type =
      self.isIncognito ? profile_metrics::BrowserProfileType::kIncognito
                       : profile_metrics::BrowserProfileType::kRegular;
  UmaHistogramEnumeration("Settings.OpenSettingsFromMenu.PerProfileType", type);
  [self.dispatcher showSettingsFromViewController:self.baseViewController];
}

- (void)enterpriseLearnMore {
  [self.dispatcher dismissPopupMenuAnimated:YES];
  [self.dispatcher
      openURLInNewTab:[OpenNewTabCommand commandWithURLFromChrome:
                                             GURL(kChromeUIManagementURL)]];
}

@end
