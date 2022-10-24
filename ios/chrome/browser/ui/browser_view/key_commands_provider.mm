// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_view/key_commands_provider.h"

#import "components/sessions/core/tab_restore_service_helper.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/find_in_page/find_tab_helper.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/ui/commands/bookmarks_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/keyboard/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/ui/keyboard/key_command_actions.h"
#import "ios/chrome/browser/ui/main/layout_guide_util.h"
#import "ios/chrome/browser/ui/util/keyboard_observer_helper.h"
#import "ios/chrome/browser/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/util/util_swift.h"
#import "ios/chrome/browser/url_loading/url_loading_util.h"
#import "ios/chrome/browser/web/web_navigation_browser_agent.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface KeyCommandsProvider () <KeyCommandActions>

// The current browser object.
@property(nonatomic, assign) Browser* browser;

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
    _browser = browser;
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
  NSMutableArray<UIKeyCommand*>* keyCommands = [NSMutableArray array];

  // List the commands that always appear in the HUD. They appear in the HUD
  // since they have titles.
  [keyCommands addObjectsFromArray:@[
    UIKeyCommand.cr_openNewTab,
    UIKeyCommand.cr_openNewIncognitoTab,
    UIKeyCommand.cr_reopenClosedTab,
  ]];

  // List the commands that only appear when there is at least a tab. When they
  // appear, they are in the HUD since they have titles.
  const BOOL hasTabs = self.tabsCount > 0;
  if (hasTabs) {
    if (self.findInPageAvailable) {
      [keyCommands addObjectsFromArray:@[
        UIKeyCommand.cr_openFindInPage,
        UIKeyCommand.cr_findNextStringInPage,
        UIKeyCommand.cr_findPreviousStringInPage,
      ]];
    }

    [keyCommands addObjectsFromArray:@[
      UIKeyCommand.cr_focusOmnibox,
      UIKeyCommand.cr_closeTab,
      UIKeyCommand.cr_showNextTab,
      UIKeyCommand.cr_showPreviousTab,
      UIKeyCommand.cr_showNextTab_2,
      UIKeyCommand.cr_showPreviousTab_2,
      UIKeyCommand.cr_showNextTab_3,
      UIKeyCommand.cr_showPreviousTab_3,
      UIKeyCommand.cr_addToBookmarks,
      UIKeyCommand.cr_reload,
      UIKeyCommand.cr_goBack,
      UIKeyCommand.cr_goForward,
    ]];

    // Since cmd+left and cmd+right are valid system shortcuts when editing
    // text, register those only if text is not being edited.
    if (!self.editingText) {
      [keyCommands addObjectsFromArray:@[
        UIKeyCommand.cr_goBack_2,
        UIKeyCommand.cr_goForward_2,
      ]];
    }

    [keyCommands addObjectsFromArray:@[
      UIKeyCommand.cr_showHistory,
      UIKeyCommand.cr_startVoiceSearch,
    ]];
  }

  if (self.canDismissModals) {
    [keyCommands addObject:UIKeyCommand.cr_dismissModalDialogs];
  }

  // List the commands that don't appear in the HUD but are always present.
  [keyCommands addObjectsFromArray:@[
    UIKeyCommand.cr_openNewTab_2,
    UIKeyCommand.cr_showSettings,
  ]];

  // List the commands that don't appear in the HUD and only appear when there
  // is at least a tab.
  if (hasTabs) {
    [keyCommands addObjectsFromArray:@[
      UIKeyCommand.cr_stop,
      UIKeyCommand.cr_showHelp,
      UIKeyCommand.cr_showDownloadsFolder,
      UIKeyCommand.cr_showDownloadsFolder_2,
      UIKeyCommand.cr_showTab0,
      UIKeyCommand.cr_showTab1,
      UIKeyCommand.cr_showTab2,
      UIKeyCommand.cr_showTab3,
      UIKeyCommand.cr_showTab4,
      UIKeyCommand.cr_showTab5,
      UIKeyCommand.cr_showTab6,
      UIKeyCommand.cr_showTab7,
      UIKeyCommand.cr_showLastTab,
    ]];
  }

  return keyCommands;
}

#pragma mark - Key Command Actions

- (void)keyCommand_openNewTab {
  OpenNewTabCommand* newTabCommand = [OpenNewTabCommand command];
  newTabCommand.shouldFocusOmnibox = YES;
  [_dispatcher openURLInNewTab:newTabCommand];
}

- (void)keyCommand_openNewIncognitoTab {
  OpenNewTabCommand* newIncognitoTabCommand =
      [OpenNewTabCommand incognitoTabCommand];
  newIncognitoTabCommand.shouldFocusOmnibox = YES;
  [_dispatcher openURLInNewTab:newIncognitoTabCommand];
}

- (void)keyCommand_reopenClosedTab {
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  sessions::TabRestoreService* const tabRestoreService =
      IOSChromeTabRestoreServiceFactory::GetForBrowserState(browserState);
  if (!tabRestoreService || tabRestoreService->entries().empty())
    return;

  const std::unique_ptr<sessions::TabRestoreService::Entry>& entry =
      tabRestoreService->entries().front();
  // Only handle the TAB type.
  // TODO(crbug.com/1056596) : Support WINDOW restoration under multi-window.
  if (entry->type != sessions::TabRestoreService::TAB)
    return;

  [self.dispatcher openURLInNewTab:[OpenNewTabCommand command]];
  RestoreTab(entry->id, WindowOpenDisposition::CURRENT_TAB, self.browser);
}

- (void)keyCommand_openFindInPage {
  [_dispatcher openFindInPage];
}

- (void)keyCommand_findNextStringInPage {
  [_dispatcher findNextStringInPage];
}

- (void)keyCommand_findPreviousStringInPage {
  [_dispatcher findPreviousStringInPage];
}

- (void)keyCommand_focusOmnibox {
  [_omniboxHandler focusOmnibox];
}

- (void)keyCommand_closeTab {
  [_browserCoordinatorCommandsHandler closeCurrentTab];
}

- (void)keyCommand_showNextTab {
  WebStateList* webStateList = self.browser->GetWebStateList();
  if (!webStateList)
    return;

  int activeIndex = webStateList->active_index();
  if (activeIndex == WebStateList::kInvalidIndex)
    return;

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
  WebStateList* webStateList = self.browser->GetWebStateList();
  if (!webStateList)
    return;

  int activeIndex = webStateList->active_index();
  if (activeIndex == WebStateList::kInvalidIndex)
    return;

  // If the active index isn't the first index, activate the prior index.
  // Otherwise index the last index (`count() - 1`).
  if (activeIndex > 0) {
    webStateList->ActivateWebStateAt(activeIndex - 1);
  } else {
    webStateList->ActivateWebStateAt(webStateList->count() - 1);
  }
}

- (void)keyCommand_addToBookmarks {
  web::WebState* currentWebState =
      _browser->GetWebStateList()->GetActiveWebState();
  if (!currentWebState) {
    return;
  }

  BookmarkAddCommand* command =
      [[BookmarkAddCommand alloc] initWithWebState:currentWebState
                              presentFolderChooser:NO];
  [_bookmarksCommandsHandler bookmark:command];
}

- (void)keyCommand_reload {
  self.navigationAgent->Reload();
}

- (void)keyCommand_goBack {
  if (self.navigationAgent->CanGoBack())
    self.navigationAgent->GoBack();
}

- (void)keyCommand_goForward {
  if (self.navigationAgent->CanGoForward())
    self.navigationAgent->GoForward();
}

- (void)keyCommand_showHistory {
  [_dispatcher showHistory];
}

- (void)keyCommand_startVoiceSearch {
  [LayoutGuideCenterForBrowser(_browser) referenceView:nil
                                             underName:kVoiceSearchButtonGuide];
  [_dispatcher startVoiceSearch];
}

- (void)keyCommand_dismissModalDialogs {
  [_dispatcher dismissModalDialogs];
}

- (void)keyCommand_showSettings {
  [_dispatcher showSettingsFromViewController:_viewController];
}

- (void)keyCommand_stop {
  self.navigationAgent->StopLoading();
}

- (void)keyCommand_showHelp {
  [_browserCoordinatorCommandsHandler showHelpPage];
}

- (void)keyCommand_showDownloadsFolder {
  [_browserCoordinatorCommandsHandler showDownloadsFolder];
}

- (void)keyCommand_showTab0 {
  [self showTabAtIndex:0];
}

- (void)keyCommand_showTab1 {
  [self showTabAtIndex:1];
}

- (void)keyCommand_showTab2 {
  [self showTabAtIndex:2];
}

- (void)keyCommand_showTab3 {
  [self showTabAtIndex:3];
}

- (void)keyCommand_showTab4 {
  [self showTabAtIndex:4];
}

- (void)keyCommand_showTab5 {
  [self showTabAtIndex:5];
}

- (void)keyCommand_showTab6 {
  [self showTabAtIndex:6];
}

- (void)keyCommand_showTab7 {
  [self showTabAtIndex:7];
}

- (void)keyCommand_showLastTab {
  [self showTabAtIndex:self.tabsCount - 1];
}

#pragma mark - Private

- (WebNavigationBrowserAgent*)navigationAgent {
  return WebNavigationBrowserAgent::FromBrowser(self.browser);
}

- (BOOL)isFindInPageAvailable {
  web::WebState* currentWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  if (!currentWebState) {
    return NO;
  }

  FindTabHelper* helper = FindTabHelper::FromWebState(currentWebState);
  return (helper && helper->CurrentPageSupportsFindInPage());
}

- (NSUInteger)tabsCount {
  return self.browser->GetWebStateList()->count();
}

- (BOOL)isEditingText {
  UIResponder* firstResponder = GetFirstResponder();
  return [firstResponder isKindOfClass:[UITextField class]] ||
         [firstResponder isKindOfClass:[UITextView class]] ||
         [[KeyboardObserverHelper sharedKeyboardObserver] isKeyboardVisible];
}

- (void)showTabAtIndex:(NSUInteger)index {
  WebStateList* webStateList = self.browser->GetWebStateList();
  if (webStateList->ContainsIndex(index)) {
    webStateList->ActivateWebStateAt(static_cast<int>(index));
  }
}

@end
