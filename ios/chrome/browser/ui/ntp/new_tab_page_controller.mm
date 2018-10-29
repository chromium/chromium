// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/new_tab_page_controller.h"

#import <QuartzCore/QuartzCore.h>

#include "base/logging.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_coordinator.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/ntp/incognito_view_controller.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/web/web_state/ui/crw_swipe_recognizer_provider.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UserMetricsAction;

@interface NewTabPageController () {
  ios::ChromeBrowserState* _browserState;  // weak.
  __weak id<UrlLoader> _loader;
  IncognitoViewController* _incognitoController;
  // The currently visible controller, one of the above.
  __weak id<CRWNativeContent> _currentController;

  // Delegate to focus and blur the omnibox.
  __weak id<OmniboxFocuser> _focuser;

  // Delegate to fetch the ToolbarModel and current web state from.
  __weak id<NewTabPageControllerDelegate> _toolbarDelegate;

  TabModel* _tabModel;
}

@property(nonatomic, strong) UIView* view;

// To ease modernizing the NTP only the internal panels are being converted
// to UIViewControllers.  This means all the plumbing between the
// BrowserViewController and the internal NTP panels (WebController, NTP)
// hierarchy is skipped.  While normally the logic to push and pop a view
// controller would be owned by a coordinator, in this case the old NTP
// controller adds and removes child view controllers itself when a load
// is initiated, and when WebController calls -willBeDismissed.
@property(nonatomic, weak) UIViewController* parentViewController;

// The command dispatcher.
@property(nonatomic, weak) id<ApplicationCommands,
                              BrowserCommands,
                              OmniboxFocuser,
                              FakeboxFocuser,
                              SnackbarCommands,
                              UrlLoader>
    dispatcher;

// Coordinator for the ContentSuggestions.
@property(nonatomic, strong)
    ContentSuggestionsCoordinator* contentSuggestionsCoordinator;

// Controller for the header of the Home panel.
@property(nonatomic, strong) id<LogoAnimationControllerOwnerOwner>
    headerController;

@end

@implementation NewTabPageController

@synthesize view = _view;
@synthesize swipeRecognizerProvider = _swipeRecognizerProvider;
@synthesize parentViewController = _parentViewController;
@synthesize dispatcher = _dispatcher;
@synthesize contentSuggestionsCoordinator = _contentSuggestionsCoordinator;
@synthesize headerController = _headerController;

- (id)initWithUrl:(const GURL&)url
                  loader:(id<UrlLoader>)loader
                 focuser:(id<OmniboxFocuser>)focuser
            browserState:(ios::ChromeBrowserState*)browserState
         toolbarDelegate:(id<NewTabPageControllerDelegate>)toolbarDelegate
                tabModel:(TabModel*)tabModel
    parentViewController:(UIViewController*)parentViewController
              dispatcher:(id<ApplicationCommands,
                             BrowserCommands,
                             OmniboxFocuser,
                             FakeboxFocuser,
                             SnackbarCommands,
                             UrlLoader>)dispatcher
           safeAreaInset:(UIEdgeInsets)safeAreaInset {
  self = [super initWithNibName:nil url:url];
  if (self) {
    DCHECK(browserState);
    _browserState = browserState;
    _loader = loader;
    _parentViewController = parentViewController;
    _dispatcher = dispatcher;
    _focuser = focuser;
    _toolbarDelegate = toolbarDelegate;
    _tabModel = tabModel;
    self.title = l10n_util::GetNSString(IDS_NEW_TAB_TITLE);

    _view = [[UIView alloc] initWithFrame:CGRectZero];

    bool isIncognito = _browserState->IsOffTheRecord();

    UIViewController* panelController = nil;
    if (isIncognito) {
      _incognitoController =
          [[IncognitoViewController alloc] initWithLoader:_loader];
      panelController = _incognitoController;
      _currentController = self.incognitoController;
    } else {
      self.contentSuggestionsCoordinator = [
          [ContentSuggestionsCoordinator alloc] initWithBaseViewController:nil];
      self.contentSuggestionsCoordinator.URLLoader = _loader;
      self.contentSuggestionsCoordinator.browserState = _browserState;
      self.contentSuggestionsCoordinator.dispatcher = self.dispatcher;
      self.contentSuggestionsCoordinator.webStateList =
          [_tabModel webStateList];
      self.contentSuggestionsCoordinator.toolbarDelegate = _toolbarDelegate;
      [self.contentSuggestionsCoordinator start];
      self.headerController =
          self.contentSuggestionsCoordinator.headerController;
      panelController = [self.contentSuggestionsCoordinator viewController];
      _currentController = self.contentSuggestionsCoordinator;
      base::RecordAction(UserMetricsAction("MobileNTPShowMostVisited"));
    }

    // To ease modernizing the NTP only the internal panels are
    // UIViewControllers.  This means all the plumbing between the
    // BrowserViewController and the internal NTP panels (WebController, NTP)
    // hierarchy is skipped.  While normally the logic to push and pop a view
    // controller would be owned by a coordinator, in this case the old NTP
    // controller adds and removes child view controllers itself when a load
    // is initiated, and when WebController calls -willBeDismissed.
    // TODO(crbug.com/826369): This will be cleaned up when removing the NTP
    // from CRWNativeContent.
    DCHECK(panelController);
    [self.parentViewController addChildViewController:panelController];
    [self.view addSubview:panelController.view];
    [panelController didMoveToParentViewController:self.parentViewController];

    panelController.view.translatesAutoresizingMaskIntoConstraints = NO;
    AddSameConstraints(self.view, panelController.view);

    [_currentController wasShown];
  }
  return self;
}

- (void)focusFakebox {
  [self.contentSuggestionsCoordinator.headerController focusFakebox];
}

- (void)dealloc {
  // This is not an ideal place to put view controller contaimnent, rather a
  // //web -wasDismissed method on CRWNativeContent would be more accurate. If
  // CRWNativeContent leaks, this will not be called.
  [_incognitoController removeFromParentViewController];
  [[self.contentSuggestionsCoordinator viewController]
      removeFromParentViewController];

  [self.contentSuggestionsCoordinator stop];
}

#pragma mark - Properties

- (UIEdgeInsets)contentInset {
  return self.contentSuggestionsCoordinator.viewController.collectionView
      .contentInset;
}

- (void)setContentInset:(UIEdgeInsets)contentInset {
  // UIKit will adjust the contentOffset sometimes when changing the
  // contentInset.bottom.  We don't want the NTP to scroll, so store and re-set
  // the contentOffset after setting the contentInset.
  CGPoint contentOffset = self.contentSuggestionsCoordinator.viewController
                              .collectionView.contentOffset;
  self.contentSuggestionsCoordinator.viewController.collectionView
      .contentInset = contentInset;
  self.contentSuggestionsCoordinator.viewController.collectionView
      .contentOffset = contentOffset;
}

#pragma mark - CRWNativeContent

- (void)willBeDismissed {
  // This methods is called by //web immediately before |self|'s view is removed
  // from the view hierarchy, making it an ideal spot to intiate view controller
  // containment methods.
  [[self.contentSuggestionsCoordinator viewController]
      willMoveToParentViewController:nil];
  [_incognitoController willMoveToParentViewController:nil];
}

- (void)reload {
  [_currentController reload];
  [super reload];
}

- (void)wasShown {
  [_currentController wasShown];
  if (_currentController != self.contentSuggestionsCoordinator) {
    // Ensure that the NTP has the latest data when it is shown, except for
    // Home.
    [self reload];
  }
}

- (void)wasHidden {
  [_currentController wasHidden];
}

- (void)dismissModals {
  [_currentController dismissModals];
}

- (void)willUpdateSnapshot {
  [_currentController willUpdateSnapshot];
}

- (CGPoint)scrollOffset {
  return [_currentController scrollOffset];
}

#pragma mark - LogoAnimationControllerOwnerOwner

- (id<LogoAnimationControllerOwner>)logoAnimationControllerOwner {
  return [self.headerController logoAnimationControllerOwner];
}

@end

@implementation NewTabPageController (TestSupport)

- (id<CRWNativeContent>)currentController {
  return _currentController;
}

- (id<CRWNativeContent>)incognitoController {
  return _incognitoController;
}

@end
