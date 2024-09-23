// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_view/ui_bundled/key_commands_provider.h"

#import <objc/runtime.h>

#import "base/memory/weak_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/browsing_data/core/browsing_data_utils.h"
#import "components/prefs/pref_service.h"
#import "components/sessions/core/tab_restore_service_helper.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/find_in_page/model/abstract_find_tab_helper.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/reading_list/model/reading_list_browser_agent.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/bookmarks_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/find_in_page_commands.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/quick_delete_commands.h"
#import "ios/chrome/browser/shared/public/commands/reading_list_add_command.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/ui/util/keyboard_observer_helper.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/url_with_title.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/tabs/model/tab_title_util.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/features.h"
#import "ios/chrome/browser/url_loading/model/url_loading_util.h"
#import "ios/chrome/browser/web/model/web_navigation_browser_agent.h"
#import "ios/chrome/browser/window_activities/model/window_activity_helpers.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_api.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_sender.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

using base::RecordAction;
using base::UserMetricsAction;

@interface KeyCommandsProvider () {
  // The current browser object.
  base::WeakPtr<Browser> _browser;
}

// The view controller delegating key command actions handling.
@property(nonatomic, weak) UIViewController* viewController;

// Configures the responder following the receiver in the responder chain.
@property(nonatomic, weak) UIResponder* followingNextResponder;

// The current navigation agent.
@property(nonatomic, assign, readonly)
    WebNavigationBrowserAgent* navigationAgent;

// Whether the Find in Page… UI is currently available.
@property(nonatomic, readonly, getter=isFindInPageAvailable)
    BOOL findInPageAvailable;

// The number of tabs displayed.
@property(nonatomic, readonly) NSUInteger tabsCount;

// Whether text is currently being edited.
@property(nonatomic, readonly, getter=isEditingText) BOOL editingText;

@end

@implementation KeyCommandsProvider

#pragma mark - Public

- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);
  self = [super init];
  if (self) {
    _browser = browser->AsWeakPtr();
  }
  return self;
}

- (void)respondBetweenViewController:(UIViewController*)viewController
                        andResponder:(UIResponder*)nextResponder {
  _viewController = viewController;
  _followingNextResponder = nextResponder;
}

#pragma mark - UIResponder

- (UIResponder*)nextResponder {
  return _followingNextResponder;
}

- (NSArray<UIKeyCommand*>*)keyCommands {
  // On iOS 15+, key commands visible in the app's menu are created in
  // MenuBuilder. Return the key commands that are not already present in the
  // menu.
  return @[
    UIKeyCommand.cr_openNewRegularTab,
    UIKeyCommand.cr_showNextTab_2,
    UIKeyCommand.cr_showPreviousTab_2,
    UIKeyCommand.cr_showNextTab_3,
    UIKeyCommand.cr_showPreviousTab_3,
    UIKeyCommand.cr_back_2,
    UIKeyCommand.cr_forward_2,
    UIKeyCommand.cr_showDownloads_2,
    UIKeyCommand.cr_select2,
    UIKeyCommand.cr_select3,
    UIKeyCommand.cr_select4,
    UIKeyCommand.cr_select5,
    UIKeyCommand.cr_select6,
    UIKeyCommand.cr_select7,
    UIKeyCommand.cr_select8,
    UIKeyCommand.cr_reportAnIssue_2,
  ];
}

- (BOOL)canPerformAction:(SEL)action withSender:(id)sender {
  // If the browser disappeared, prevent any command handling.
  if (!_browser) {
    return NO;
  }

  // BVC prevents KeyCommandsProvider from providing key commands when it has
  // `presentedViewController` set. But there is an interval between presenting
  // a view controller and having `presentedViewController` set. In that window,
  // KeyCommandsProvider can register key commands while it shouldn't.
  // To prevent actions from executing, check again if there is a
  // `presentedViewController`.
  if (_viewController.presentedViewController) {
    return NO;
  }

  if (sel_isEqual(action, @selector(keyCommand_back))) {
    BOOL canPerformBack =
        self.tabsCount > 0 && self.navigationAgent->CanGoBack();
    // Since cmd+left is a valid system shortcuts when editing text, register it
    // only if text is not being edited.
    if ([sender isEqual:UIKeyCommand.cr_back_2]) {
      return canPerformBack && !self.editingText;
    }
    return canPerformBack;
  }

  if (sel_isEqual(action, @selector(keyCommand_forward))) {
    BOOL canPerformForward =
        self.tabsCount > 0 && self.navigationAgent->CanGoForward();
    // Since cmd+right is a valid system shortcuts when editing text, register
    // it only if text is not being edited.
    if ([sender isEqual:UIKeyCommand.cr_forward_2]) {
      return canPerformForward && !self.editingText;
    }
    return canPerformForward;
  }
  if (sel_isEqual(action, @selector(keyCommand_showHistory))) {
    return !_browser->GetProfile()->IsOffTheRecord() && self.tabsCount > 0;
  }
  if (sel_isEqual(action, @selector(keyCommand_openLocation)) ||
      sel_isEqual(action, @selector(keyCommand_closeTab)) ||
      sel_isEqual(action, @selector(keyCommand_showBookmarks)) ||
      sel_isEqual(action, @selector(keyCommand_reload)) ||
      sel_isEqual(action, @selector(keyCommand_voiceSearch)) ||
      sel_isEqual(action, @selector(keyCommand_stop)) ||
      sel_isEqual(action, @selector(keyCommand_showHelp)) ||
      sel_isEqual(action, @selector(keyCommand_showDownloads)) ||
      sel_isEqual(action, @selector(keyCommand_select1)) ||
      sel_isEqual(action, @selector(keyCommand_select2)) ||
      sel_isEqual(action, @selector(keyCommand_select3)) ||
      sel_isEqual(action, @selector(keyCommand_select4)) ||
      sel_isEqual(action, @selector(keyCommand_select5)) ||
      sel_isEqual(action, @selector(keyCommand_select6)) ||
      sel_isEqual(action, @selector(keyCommand_select7)) ||
      sel_isEqual(action, @selector(keyCommand_select8)) ||
      sel_isEqual(action, @selector(keyCommand_select9)) ||
      sel_isEqual(action, @selector(keyCommand_showNextTab)) ||
      sel_isEqual(action, @selector(keyCommand_showPreviousTab))) {
    return self.tabsCount > 0;
  }
  if (sel_isEqual(action, @selector(keyCommand_find))) {
    return self.findInPageAvailable;
  }
  if (sel_isEqual(action, @selector(keyCommand_findNext)) ||
      sel_isEqual(action, @selector(keyCommand_findPrevious))) {
    return [self isFindInPageActive];
  }
  if (sel_isEqual(action, @selector(keyCommand_addToBookmarks)) ||
      sel_isEqual(action, @selector(keyCommand_addToReadingList))) {
    return [self isHTTPOrHTTPSPage];
  }
  if (sel_isEqual(action, @selector(keyCommand_reopenLastClosedTab))) {
    sessions::TabRestoreService* const tabRestoreService =
        IOSChromeTabRestoreServiceFactory::GetForProfile(
            _browser->GetProfile());
    return tabRestoreService && !tabRestoreService->entries().empty();
  }
  if (sel_isEqual(action, @selector(keyCommand_reportAnIssue))) {
    return ios::provider::IsUserFeedbackSupported();
  }
  if (sel_isEqual(action, @selector(keyCommand_openNewRegularTab))) {
    // Don't open regular tab if incognito is forced by policy.
    return !IsIncognitoModeForced(_browser->GetProfile()->GetPrefs());
  }
  if (sel_isEqual(action, @selector(keyCommand_openNewIncognitoTab))) {
    // Don't open incognito tab if incognito is disabled by policy.
    return !IsIncognitoModeDisabled(_browser->GetProfile()->GetPrefs());
  }
  if (sel_isEqual(action, @selector(keyCommand_clearBrowsingData))) {
    // Clear Browsing Data shouldn't be available in incognito mode.
    return !_browser->GetProfile()->IsOffTheRecord();
  }

  return [super canPerformAction:action withSender:sender];
}

// Changes the title to display the most appropriate string in the shortcut
// menu.
- (void)validateCommand:(UICommand*)command {
  if (command.action == @selector(keyCommand_find)) {
    command.discoverabilityTitle =
        l10n_util::GetNSStringWithFixup(IDS_IOS_KEYBOARD_FIND_IN_PAGE);
  }
  if (command.action == @selector(keyCommand_select1)) {
    command.discoverabilityTitle =
        l10n_util::GetNSStringWithFixup(IDS_IOS_KEYBOARD_FIRST_TAB);
  }
  if (command.action == @selector(keyCommand_addToBookmarks)) {
    if ([self isBookmarkedPage]) {
      command.discoverabilityTitle =
          l10n_util::GetNSStringWithFixup(IDS_IOS_KEYBOARD_EDIT_BOOKMARK);
    }
  }
  return [super validateCommand:command];
}

#pragma mark - Key Command Actions

- (void)keyCommand_openNewTab {
  RecordAction(UserMetricsAction("MobileKeyCommandOpenNewTab"));
  if (_browser->GetProfile()->IsOffTheRecord()) {
    [self openNewIncognitoTab];
  } else {
    [self openNewRegularTab];
  }
}

- (void)keyCommand_openNewRegularTab {
  RecordAction(UserMetricsAction("MobileKeyCommandOpenNewRegularTab"));
  [self openNewRegularTab];
}

- (void)keyCommand_openNewIncognitoTab {
  RecordAction(UserMetricsAction("MobileKeyCommandOpenNewIncognitoTab"));
  [self openNewIncognitoTab];
}

- (void)keyCommand_openNewWindow {
  RecordAction(UserMetricsAction("MobileKeyCommandOpenNewWindow"));
  [_applicationHandler
      openNewWindowWithActivity:ActivityToLoadURL(
                                    WindowActivityKeyCommandOrigin,
                                    GURL(kChromeUINewTabURL))];
}

- (void)keyCommand_openNewIncognitoWindow {
  RecordAction(UserMetricsAction("MobileKeyCommandOpenNewIncognitoWindow"));
  [_applicationHandler
      openNewWindowWithActivity:ActivityToLoadURL(
                                    WindowActivityKeyCommandOrigin,
                                    GURL(kChromeUINewTabURL), web::Referrer(),
                                    /* in_incognito */ true)];
}

- (void)keyCommand_reopenLastClosedTab {
  RecordAction(UserMetricsAction("MobileKeyCommandReopenLastClosedTab"));
  ProfileIOS* profile = _browser->GetProfile();
  sessions::TabRestoreService* const tabRestoreService =
      IOSChromeTabRestoreServiceFactory::GetForProfile(profile);
  if (!tabRestoreService || tabRestoreService->entries().empty()) {
    return;
  }

  const std::unique_ptr<sessions::tab_restore::Entry>& entry =
      tabRestoreService->entries().front();
  // Only handle the TAB type.
  // TODO(crbug.com/40676931) : Support WINDOW restoration under multi-window.
  if (entry->type != sessions::tab_restore::Type::TAB) {
    return;
  }

  [_applicationHandler openURLInNewTab:[OpenNewTabCommand command]];
  RestoreTab(entry->id, WindowOpenDisposition::CURRENT_TAB, _browser.get());
}

- (void)keyCommand_find {
  RecordAction(UserMetricsAction("MobileKeyCommandFind"));
  [_findInPageHandler openFindInPage];
}

- (void)keyCommand_findNext {
  RecordAction(UserMetricsAction("MobileKeyCommandFindNext"));
  [_findInPageHandler findNextStringInPage];
}

- (void)keyCommand_findPrevious {
  RecordAction(UserMetricsAction("MobileKeyCommandFindPrevious"));
  [_findInPageHandler findPreviousStringInPage];
}

- (void)keyCommand_openLocation {
  RecordAction(UserMetricsAction("MobileKeyCommandOpenLocation"));
  [_omniboxHandler focusOmnibox];
}

- (void)keyCommand_closeTab {
  RecordAction(UserMetricsAction("MobileKeyCommandCloseTab"));
  [_browserCoordinatorHandler closeCurrentTab];
}

- (void)keyCommand_showNextTab {
  RecordAction(UserMetricsAction("MobileKeyCommandShowNextTab"));
  WebStateList* webStateList = _browser->GetWebStateList();
  int activeIndex = webStateList->active_index();
  if (activeIndex == WebStateList::kInvalidIndex) {
    return;
  }

  // If the active index isn't the last index, activate the next index.
  // (the last index is always `count() - 1`).
  // Otherwise activate the first index.
  if (activeIndex < (webStateList->count() - 1)) {
    webStateList->ActivateWebStateAt(activeIndex + 1);
  } else {
    webStateList->ActivateWebStateAt(0);
  }
}

- (void)keyCommand_showPreviousTab {
  RecordAction(UserMetricsAction("MobileKeyCommandShowPreviousTab"));
  WebStateList* webStateList = _browser->GetWebStateList();
  int activeIndex = webStateList->active_index();
  if (activeIndex == WebStateList::kInvalidIndex) {
    return;
  }

  // If the active index isn't the first index, activate the prior index.
  // Otherwise index the last index (`count() - 1`).
  if (activeIndex > 0) {
    webStateList->ActivateWebStateAt(activeIndex - 1);
  } else {
    webStateList->ActivateWebStateAt(webStateList->count() - 1);
  }
}

- (void)keyCommand_showBookmarks {
  RecordAction(UserMetricsAction("MobileKeyCommandShowBookmarks"));
  [_browserCoordinatorHandler showBookmarksManager];
}

- (void)keyCommand_addToBookmarks {
  RecordAction(UserMetricsAction("MobileKeyCommandAddToBookmarks"));
  web::WebState* currentWebState =
      _browser->GetWebStateList()->GetActiveWebState();
  if (!currentWebState) {
    return;
  }
  GURL URL = currentWebState->GetLastCommittedURL();
  if (!URL.is_valid()) {
    return;
  }

  NSString* title = tab_util::GetTabTitle(currentWebState);
  [_bookmarksHandler
      createOrEditBookmarkWithURL:[[URLWithTitle alloc] initWithURL:URL
                                                              title:title]];
}

- (void)keyCommand_reload {
  RecordAction(UserMetricsAction("MobileKeyCommandReload"));
  self.navigationAgent->Reload();
}

- (void)keyCommand_back {
  RecordAction(UserMetricsAction("MobileKeyCommandBack"));
  if (self.navigationAgent->CanGoBack()) {
    self.navigationAgent->GoBack();
  }
}

- (void)keyCommand_forward {
  RecordAction(UserMetricsAction("MobileKeyCommandForward"));
  if (self.navigationAgent->CanGoForward()) {
    self.navigationAgent->GoForward();
  }
}

- (void)keyCommand_showHistory {
  RecordAction(UserMetricsAction("MobileKeyCommandShowHistory"));
  [_applicationHandler showHistory];
}

- (void)keyCommand_voiceSearch {
  RecordAction(UserMetricsAction("MobileKeyCommandVoiceSearch"));
  [LayoutGuideCenterForBrowser(_browser.get())
      referenceView:nil
          underName:kVoiceSearchButtonGuide];
  [_applicationHandler startVoiceSearch];
}

- (void)keyCommand_showSettings {
  RecordAction(UserMetricsAction("MobileKeyCommandShowSettings"));
  [_applicationHandler showSettingsFromViewController:_viewController];
}

- (void)keyCommand_stop {
  RecordAction(UserMetricsAction("MobileKeyCommandStop"));
  self.navigationAgent->StopLoading();
}

- (void)keyCommand_showHelp {
  RecordAction(UserMetricsAction("MobileKeyCommandShowHelp"));
  [_browserCoordinatorHandler showHelpPage];
}

- (void)keyCommand_showDownloads {
  RecordAction(UserMetricsAction("MobileKeyCommandShowDownloads"));
  [_browserCoordinatorHandler showDownloadsFolder];
}

- (void)keyCommand_select1 {
  RecordAction(UserMetricsAction("MobileKeyCommandShowFirstTab"));
  [self showTabAtIndex:0];
}

- (void)keyCommand_select2 {
  RecordAction(UserMetricsAction("MobileKeyCommandShowTab2"));
  [self showTabAtIndex:1];
}

- (void)keyCommand_select3 {
  RecordAction(UserMetricsAction("MobileKeyCommandShowTab3"));
  [self showTabAtIndex:2];
}

- (void)keyCommand_select4 {
  RecordAction(UserMetricsAction("MobileKeyCommandShowTab4"));
  [self showTabAtIndex:3];
}

- (void)keyCommand_select5 {
  RecordAction(UserMetricsAction("MobileKeyCommandShowTab5"));
  [self showTabAtIndex:4];
}

- (void)keyCommand_select6 {
  RecordAction(UserMetricsAction("MobileKeyCommandShowTab6"));
  [self showTabAtIndex:5];
}

- (void)keyCommand_select7 {
  RecordAction(UserMetricsAction("MobileKeyCommandShowTab7"));
  [self showTabAtIndex:6];
}

- (void)keyCommand_select8 {
  RecordAction(UserMetricsAction("MobileKeyCommandShowTab8"));
  [self showTabAtIndex:7];
}

- (void)keyCommand_select9 {
  RecordAction(UserMetricsAction("MobileKeyCommandShowLastTab"));
  [self showTabAtIndex:self.tabsCount - 1];
}

- (void)keyCommand_reportAnIssue {
  RecordAction(UserMetricsAction("MobileKeyCommandReportAnIssue"));
  [_applicationHandler
      showReportAnIssueFromViewController:_viewController
                                   sender:UserFeedbackSender::KeyCommand];
}

- (void)keyCommand_addToReadingList {
  RecordAction(UserMetricsAction("MobileKeyCommandAddToReadingList"));
  web::WebState* currentWebState =
      _browser->GetWebStateList()->GetActiveWebState();
  if (!currentWebState) {
    return;
  }
  GURL URL = currentWebState->GetLastCommittedURL();
  if (!URL.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  NSString* title = tab_util::GetTabTitle(currentWebState);
  ReadingListAddCommand* command =
      [[ReadingListAddCommand alloc] initWithURL:URL title:title];
  ReadingListBrowserAgent* readingListBrowserAgent =
      ReadingListBrowserAgent::FromBrowser(_browser.get());
  readingListBrowserAgent->AddURLsToReadingList(command.URLs);
}

- (void)keyCommand_showReadingList {
  RecordAction(UserMetricsAction("MobileKeyCommandShowReadingList"));
  [_browserCoordinatorHandler showReadingList];
}

- (void)keyCommand_goToTabGrid {
  RecordAction(UserMetricsAction("MobileKeyCommandGoToTabGrid"));
  [_applicationHandler prepareTabSwitcher];
  [_applicationHandler displayTabGridInMode:TabGridOpeningMode::kDefault];
}

- (void)keyCommand_clearBrowsingData {
  RecordAction(UserMetricsAction("MobileKeyCommandClearBrowsingData"));
  base::UmaHistogramEnumeration(
      browsing_data::kDeleteBrowsingDataDialogHistogram,
      browsing_data::DeleteBrowsingDataDialogAction::
          kKeyboardEntryPointSelected);

  if (IsIosQuickDeleteEnabled()) {
    [_quickDeleteHandler showQuickDeleteAndCanPerformTabsClosureAnimation:YES];
  } else {
    [_settingsHandler showClearBrowsingDataSettings];
  }
}

#pragma mark - Private

- (WebNavigationBrowserAgent*)navigationAgent {
  return WebNavigationBrowserAgent::FromBrowser(_browser.get());
}

- (BOOL)isFindInPageAvailable {
  web::WebState* currentWebState =
      _browser->GetWebStateList()->GetActiveWebState();
  if (!currentWebState) {
    return NO;
  }

  auto* helper = GetConcreteFindTabHelperFromWebState(currentWebState);
  return (helper && helper->CurrentPageSupportsFindInPage());
}

- (BOOL)isFindInPageActive {
  web::WebState* currentWebState =
      _browser->GetWebStateList()->GetActiveWebState();
  if (!currentWebState) {
    return NO;
  }

  auto* helper = GetConcreteFindTabHelperFromWebState(currentWebState);
  return (helper && helper->IsFindUIActive());
}

- (NSUInteger)tabsCount {
  return _browser->GetWebStateList()->count();
}

- (BOOL)isEditingText {
  UIResponder* firstResponder = GetFirstResponder();
  return [firstResponder isKindOfClass:[UITextField class]] ||
         [firstResponder isKindOfClass:[UITextView class]] ||
         [[KeyboardObserverHelper sharedKeyboardObserver] isKeyboardVisible];
}

- (void)openNewRegularTab {
  OpenNewTabCommand* newTabCommand = [OpenNewTabCommand command];
  newTabCommand.shouldFocusOmnibox = YES;
  [_applicationHandler openURLInNewTab:newTabCommand];
}

- (void)openNewIncognitoTab {
  OpenNewTabCommand* newIncognitoTabCommand =
      [OpenNewTabCommand incognitoTabCommand];
  newIncognitoTabCommand.shouldFocusOmnibox = YES;
  [_applicationHandler openURLInNewTab:newIncognitoTabCommand];
}

- (void)showTabAtIndex:(NSUInteger)index {
  WebStateList* webStateList = _browser->GetWebStateList();
  if (webStateList->ContainsIndex(index)) {
    webStateList->ActivateWebStateAt(static_cast<int>(index));
  }
}

- (BOOL)isHTTPOrHTTPSPage {
  web::WebState* currentWebState =
      _browser->GetWebStateList()->GetActiveWebState();
  if (!currentWebState) {
    return NO;
  }

  const GURL& url = currentWebState->GetLastCommittedURL();
  return url.is_valid() && url.SchemeIsHTTPOrHTTPS();
}

- (BOOL)isBookmarkedPage {
  web::WebState* currentWebState =
      _browser->GetWebStateList()->GetActiveWebState();
  if (!currentWebState) {
    return NO;
  }

  const GURL& url = currentWebState->GetLastCommittedURL();
  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForProfile(_browser->GetProfile());
  return bookmarkModel->IsBookmarked(url);
}

@end
