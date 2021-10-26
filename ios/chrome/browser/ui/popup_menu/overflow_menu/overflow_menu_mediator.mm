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
#import "components/prefs/ios/pref_observer_bridge.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/policy/browser_policy_connector_ios.h"
#import "ios/chrome/browser/ui/activity_services/canonical_url_retriever.h"
#include "ios/chrome/browser/ui/bookmarks/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/find_in_page_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/commands/reading_list_add_command.h"
#import "ios/chrome/browser/ui/commands/text_zoom_commands.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_utils.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_swift.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/web/web_navigation_browser_agent.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/window_activities/window_activity_helpers.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/web/common/user_agent.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
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
                                             Handler handler) {
  NSString* name = l10n_util::GetNSString(nameID);
  return [[OverflowMenuAction alloc] initWithName:name
                                        imageName:imageName
                               enterpriseDisabled:NO
                                          handler:handler];
}

OverflowMenuDestination* CreateOverflowMenuDestination(int nameID,
                                                       NSString* imageName,
                                                       Handler handler) {
  NSString* name = l10n_util::GetNSString(nameID);
  return [[OverflowMenuDestination alloc] initWithName:name
                                             imageName:imageName
                                    enterpriseDisabled:NO
                                               handler:handler];
}

}  // namespace

@interface OverflowMenuMediator () <BookmarkModelBridgeObserver,
                                    CRWWebStateObserver,
                                    PrefObserverDelegate,
                                    WebStateListObserving> {
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;

  // Bridge to register for bookmark changes.
  std::unique_ptr<bookmarks::BookmarkModelBridge> _bookmarkModelBridge;

  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  std::unique_ptr<PrefChangeRegistrar> _prefChangeRegistrar;
}

// The current web state.
@property(nonatomic, assign) web::WebState* webState;

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

@property(nonatomic, strong) OverflowMenuAction* reloadAction;
@property(nonatomic, strong) OverflowMenuAction* stopLoadAction;
@property(nonatomic, strong) OverflowMenuAction* openTabAction;
@property(nonatomic, strong) OverflowMenuAction* openIncognitoTabAction;
@property(nonatomic, strong) OverflowMenuAction* openNewWindowAction;

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
  }
  return self;
}

- (void)dealloc {
  // Remove the model link so the other deallocations don't update the model
  // and thus the UI as the UI is dismissing.
  _overflowMenuModel = nil;

  self.webState = nullptr;
  self.webStateList = nullptr;

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

- (void)setWebState:(web::WebState*)webState {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserver.get());
  }

  _webState = webState;

  if (_webState) {
    [self updateModel];
    _webState->AddObserver(_webStateObserver.get());
  }
}

- (void)setWebStateList:(WebStateList*)webStateList {
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
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

#pragma mark - Model Creation

- (OverflowMenuModel*)createModel {
  __weak __typeof(self) weakSelf = self;

  self.bookmarksDestination = CreateOverflowMenuDestination(
      IDS_IOS_TOOLS_MENU_BOOKMARKS, @"overflow_menu_destination_bookmarks", ^{
        [weakSelf openBookmarks];
      });
  self.downloadsDestination = CreateOverflowMenuDestination(
      IDS_IOS_TOOLS_MENU_DOWNLOADS, @"overflow_menu_destination_downloads", ^{
        [weakSelf openDownloads];
      });
  self.historyDestination = CreateOverflowMenuDestination(
      IDS_IOS_TOOLS_MENU_HISTORY, @"overflow_menu_destination_history", ^{
        [weakSelf openHistory];
      });
  self.passwordsDestination = CreateOverflowMenuDestination(
      IDS_IOS_TOOLS_MENU_PASSWORDS, @"overflow_menu_destination_passwords", ^{
        [weakSelf openPasswords];
      });
  self.readingListDestination = CreateOverflowMenuDestination(
      IDS_IOS_TOOLS_MENU_READING_LIST,
      @"overflow_menu_destination_reading_list", ^{
        [weakSelf openReadingList];
      });
  self.recentTabsDestination =
      CreateOverflowMenuDestination(IDS_IOS_TOOLS_MENU_RECENT_TABS,
                                    @"overflow_menu_destination_recent_tabs", ^{
                                      [weakSelf openRecentTabs];
                                    });
  self.settingsDestination = CreateOverflowMenuDestination(
      IDS_IOS_TOOLS_MENU_SETTINGS, @"overflow_menu_destination_settings", ^{
        [weakSelf openSettings];
      });
  self.siteInfoDestination =
      CreateOverflowMenuDestination(IDS_IOS_TOOLS_MENU_SITE_INFORMATION,
                                    @"overflow_menu_destination_site_info", ^{
                                      [weakSelf openSiteInformation];
                                    });

  self.reloadAction = CreateOverflowMenuAction(
      IDS_IOS_TOOLS_MENU_RELOAD, @"overflow_menu_action_reload", ^{
        [weakSelf reload];
      });
  self.stopLoadAction = CreateOverflowMenuAction(
      IDS_IOS_TOOLS_MENU_STOP, @"overflow_menu_action_stop", ^{
        [weakSelf stopLoading];
      });

  self.openTabAction = CreateOverflowMenuAction(
      IDS_IOS_TOOLS_MENU_NEW_TAB, @"overflow_menu_action_new_tab", ^{
        [weakSelf openTab];
      });
  self.openIncognitoTabAction =
      CreateOverflowMenuAction(IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB,
                               @"overflow_menu_action_incognito", ^{
                                 [weakSelf openIncognitoTab];
                               });

  self.openNewWindowAction = CreateOverflowMenuAction(
      IDS_IOS_TOOLS_MENU_NEW_WINDOW, @"overflow_menu_action_new_window", ^{
        [weakSelf openNewWindow];
      });

  // The app actions vary based on page state, so they are set in
  // |-updateModel|.
  self.appActionsGroup =
      [[OverflowMenuActionGroup alloc] initWithGroupName:@"app_actions"
                                                 actions:@[]];

  self.addBookmarkAction = CreateOverflowMenuAction(
      IDS_IOS_TOOLS_MENU_ADD_TO_BOOKMARKS, @"overflow_menu_action_bookmark", ^{
        [weakSelf addOrEditBookmark];
      });
  self.editBookmarkAction =
      CreateOverflowMenuAction(IDS_IOS_TOOLS_MENU_EDIT_BOOKMARK,
                               @"overflow_menu_action_edit_bookmark", ^{
                                 [weakSelf addOrEditBookmark];
                               });
  self.readLaterAction =
      CreateOverflowMenuAction(IDS_IOS_CONTENT_CONTEXT_ADDTOREADINGLIST,
                               @"overflow_menu_action_read_later", ^{
                                 [weakSelf addToReadingList];
                               });
  self.translateAction = CreateOverflowMenuAction(
      IDS_IOS_TOOLS_MENU_TRANSLATE, @"overflow_menu_action_translate", ^{
        [weakSelf translatePage];
      });
  self.requestDesktopAction =
      CreateOverflowMenuAction(IDS_IOS_TOOLS_MENU_REQUEST_DESKTOP_SITE,
                               @"overflow_menu_action_request_desktop", ^{
                                 [weakSelf requestDesktopSite];
                               });
  self.requestMobileAction =
      CreateOverflowMenuAction(IDS_IOS_TOOLS_MENU_REQUEST_MOBILE_SITE,
                               @"overflow_menu_action_request_mobile", ^{
                                 [weakSelf requestMobileSite];
                               });
  self.findInPageAction = CreateOverflowMenuAction(
      IDS_IOS_TOOLS_MENU_FIND_IN_PAGE, @"overflow_menu_action_find_in_page", ^{
        [weakSelf openFindInPage];
      });
  self.textZoomAction = CreateOverflowMenuAction(
      IDS_IOS_TOOLS_MENU_TEXT_ZOOM, @"overflow_menu_action_text_zoom", ^{
        [weakSelf openTextZoom];
      });

  // The page actions vary based on page state, so they are set in
  // |-updateModel|.
  self.pageActionsGroup =
      [[OverflowMenuActionGroup alloc] initWithGroupName:@"page_actions"
                                                 actions:@[]];

  self.reportIssueAction = CreateOverflowMenuAction(
      IDS_IOS_OPTIONS_REPORT_AN_ISSUE, @"overflow_menu_action_report_issue", ^{
        [weakSelf reportAnIssue];
      });
  self.helpAction = CreateOverflowMenuAction(IDS_IOS_TOOLS_MENU_HELP_MOBILE,
                                             @"overflow_menu_action_help", ^{
                                               [weakSelf openHelp];
                                             });

  OverflowMenuActionGroup* helpActionsGroup =
      [[OverflowMenuActionGroup alloc] initWithGroupName:@"help_actions"
                                                 actions:@[
                                                   self.reportIssueAction,
                                                   self.helpAction,
                                                 ]];

  return [[OverflowMenuModel alloc] initWithDestinations:@[
    self.bookmarksDestination,
    self.historyDestination,
    self.readingListDestination,
    self.passwordsDestination,
    self.downloadsDestination,
    self.recentTabsDestination,
    self.siteInfoDestination,
    self.settingsDestination,
  ]
                                            actionGroups:@[
                                              self.appActionsGroup,
                                              self.pageActionsGroup,
                                              helpActionsGroup,
                                            ]];
}

#pragma mark - Private

// Make sure the model to match the current page state.
- (void)updateModel {
  // If the model hasn't been created, there's no need to update.
  if (!_overflowMenuModel) {
    return;
  }

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
}

// Whether the page is currently loading.
- (BOOL)isPageLoading {
  if (!self.webState)
    return NO;
  return self.webState->IsLoading();
}

// Returns the UserAgentType currently in use.
- (web::UserAgentType)userAgentType {
  if (!self.webState)
    return web::UserAgentType::NONE;
  web::NavigationItem* visibleItem =
      self.webState->GetNavigationManager()->GetVisibleItem();
  if (!visibleItem)
    return web::UserAgentType::NONE;

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

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == bookmarks::prefs::kEditBookmarksEnabled)
    [self updateModel];
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
  [self.dispatcher dismissPopupMenuAnimated:YES];

  if (!self.webState)
    return;
  // The mediator can be destroyed when this callback is executed. So it is not
  // possible to use a weak self.
  __weak id<BrowserCommands> weakDispatcher = self.dispatcher;
  GURL visibleURL = self.webState->GetVisibleURL();
  NSString* title = base::SysUTF16ToNSString(self.webState->GetTitle());
  activity_services::RetrieveCanonicalUrl(self.webState, ^(const GURL& URL) {
    const GURL& pageURL = !URL.is_empty() ? URL : visibleURL;
    if (!pageURL.is_valid() || !pageURL.SchemeIsHTTPOrHTTPS())
      return;

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
  [self.dispatcher requestDesktopSite];
}

// Dismisses the menu and requests the mobile version of the current page
- (void)requestMobileSite {
  RecordAction(UserMetricsAction("MobileMenuRequestMobileSite"));
  [self.dispatcher dismissPopupMenuAnimated:YES];
  [self.dispatcher requestMobileSite];
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
      showSavedPasswordsSettingsFromViewController:self.baseViewController];
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

@end
