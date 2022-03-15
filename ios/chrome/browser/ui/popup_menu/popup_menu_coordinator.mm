// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/popup_menu_coordinator.h"

#include "base/check.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/feature_engagement/tracker_factory.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/overlays/public/overlay_presenter.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/ui/browser_container/browser_container_mediator.h"
#import "ios/chrome/browser/ui/bubble/bubble_presenter.h"
#import "ios/chrome/browser/ui/bubble/bubble_view_controller_presenter.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/find_in_page_commands.h"
#import "ios/chrome/browser/ui/commands/popup_menu_commands.h"
#import "ios/chrome/browser/ui/follow/follow_action_state.h"
#import "ios/chrome/browser/ui/follow/follow_util.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/feature_flags.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_mediator.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_swift.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_action_handler.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_mediator.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_presenter.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_presenter_delegate.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_table_view_controller.h"
#import "ios/chrome/browser/ui/presenters/contained_presenter_delegate.h"
#import "ios/chrome/browser/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/web/web_navigation_browser_agent.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/follow/follow_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Returns the corresponding command type for a Popup menu |type|.
PopupMenuCommandType CommandTypeFromPopupType(PopupMenuType type) {
  if (type == PopupMenuTypeToolsMenu)
    return PopupMenuCommandTypeToolsMenu;
  return PopupMenuCommandTypeDefault;
}
}  // namespace

@interface PopupMenuCoordinator () <PopupMenuCommands,
                                    PopupMenuPresenterDelegate,
                                    UIPopoverPresentationControllerDelegate,
                                    UISheetPresentationControllerDelegate>

// Presenter for the popup menu, managing the animations.
@property(nonatomic, strong) PopupMenuPresenter* presenter;
// Mediator for the popup menu.
@property(nonatomic, strong) PopupMenuMediator* mediator;
// Mediator for the overflow menu
@property(nonatomic, strong) OverflowMenuMediator* overflowMenuMediator;
// Mediator to that alerts the main |mediator| when the web content area
// is blocked by an overlay.
@property(nonatomic, strong) BrowserContainerMediator* contentBlockerMediator;
// ViewController for this mediator.
@property(nonatomic, strong) PopupMenuTableViewController* viewController;
// Handles user interaction with the popup menu items.
@property(nonatomic, strong) PopupMenuActionHandler* actionHandler;
// Time when the presentation of the popup menu is requested.
@property(nonatomic, assign) NSTimeInterval requestStartTime;

@end

@implementation PopupMenuCoordinator

@synthesize mediator = _mediator;
@synthesize presenter = _presenter;
@synthesize requestStartTime = _requestStartTime;
@synthesize UIUpdater = _UIUpdater;
@synthesize bubblePresenter = _bubblePresenter;
@synthesize viewController = _viewController;

#pragma mark - ChromeCoordinator

- (void)start {
  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(PopupMenuCommands)];
  NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
  [defaultCenter addObserver:self
                    selector:@selector(applicationDidEnterBackground:)
                        name:UIApplicationDidEnterBackgroundNotification
                      object:nil];
}

- (void)stop {
  [self.browser->GetCommandDispatcher() stopDispatchingToTarget:self];
  [self.overflowMenuMediator disconnect];
  self.overflowMenuMediator = nil;
  [self.mediator disconnect];
  self.mediator = nil;
  self.viewController = nil;
}

#pragma mark - Public

- (BOOL)isShowingPopupMenu {
  return self.presenter != nil;
}

#pragma mark - PopupMenuCommands

- (void)showNavigationHistoryBackPopupMenu {
  base::RecordAction(
      base::UserMetricsAction("MobileToolbarShowTabHistoryMenu"));
  [self presentPopupOfType:PopupMenuTypeNavigationBackward
            fromNamedGuide:kBackButtonGuide];
}

- (void)showNavigationHistoryForwardPopupMenu {
  base::RecordAction(
      base::UserMetricsAction("MobileToolbarShowTabHistoryMenu"));
  [self presentPopupOfType:PopupMenuTypeNavigationForward
            fromNamedGuide:kForwardButtonGuide];
}

- (void)showToolsMenuPopup {
  // The metric is registered at the toolbar level.
  [self presentPopupOfType:PopupMenuTypeToolsMenu
            fromNamedGuide:kToolsMenuGuide];
}

- (void)showTabGridButtonPopup {
  base::RecordAction(base::UserMetricsAction("MobileToolbarShowTabGridMenu"));
  [self presentPopupOfType:PopupMenuTypeTabGrid
            fromNamedGuide:kTabSwitcherGuide];
}

- (void)showNewTabButtonPopup {
  base::RecordAction(base::UserMetricsAction("MobileToolbarShowNewTabMenu"));
  [self presentPopupOfType:PopupMenuTypeNewTab
            fromNamedGuide:kNewTabButtonGuide];
}

- (void)dismissPopupMenuAnimated:(BOOL)animated {
  [self.UIUpdater updateUIForMenuDismissed];
  if (self.overflowMenuMediator) {
    [self.baseViewController dismissViewControllerAnimated:animated
                                                completion:nil];
    [self.overflowMenuMediator disconnect];
    self.overflowMenuMediator = nil;
  }
  [self.presenter dismissAnimated:animated];
  self.presenter = nil;
  [self.mediator disconnect];
  self.mediator = nil;
  self.viewController = nil;
}

#pragma mark - PopupMenuLongPressDelegate

- (void)longPressFocusPointChangedTo:(CGPoint)point {
  [self.viewController focusRowAtPoint:point];
}

- (void)longPressEndedAtPoint:(CGPoint)point {
  [self.viewController selectRowAtPoint:point];
}

#pragma mark - ContainedPresenterDelegate

- (void)containedPresenterDidPresent:(id<ContainedPresenter>)presenter {
  if (presenter != self.presenter)
    return;

  if (self.requestStartTime != 0) {
    base::TimeDelta elapsed = base::Seconds(
        [NSDate timeIntervalSinceReferenceDate] - self.requestStartTime);
    UMA_HISTOGRAM_TIMES("Toolbar.ShowToolsMenuResponsiveness", elapsed);
    // Reset the start time to ensure that whatever happens, we only record
    // this once.
    self.requestStartTime = 0;
  }
}

#pragma mark - PopupMenuPresenterDelegate

- (void)popupMenuPresenterWillDismiss:(PopupMenuPresenter*)presenter {
  [self dismissPopupMenuAnimated:NO];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self dismissPopupMenuAnimated:NO];
}

#pragma mark - Notification callback

- (void)applicationDidEnterBackground:(NSNotification*)note {
  [self dismissPopupMenuAnimated:NO];
}

#pragma mark - Private

// Presents a popup menu of type |type| with an animation starting from
// |guideName|.
- (void)presentPopupOfType:(PopupMenuType)type
            fromNamedGuide:(GuideName*)guideName {
  if (self.presenter || self.overflowMenuMediator)
    [self dismissPopupMenuAnimated:YES];

  // TODO(crbug.com/1045047): Use HandlerForProtocol after commands protocol
  // clean up.
  id<BrowserCommands> callableDispatcher =
      static_cast<id<BrowserCommands>>(self.browser->GetCommandDispatcher());
  [callableDispatcher
      prepareForPopupMenuPresentation:CommandTypeFromPopupType(type)];

  self.requestStartTime = [NSDate timeIntervalSinceReferenceDate];

  PopupMenuTableViewController* tableViewController =
      [[PopupMenuTableViewController alloc] init];
  tableViewController.baseViewController = self.baseViewController;
  if (type == PopupMenuTypeToolsMenu) {
    tableViewController.tableView.accessibilityIdentifier =
        kPopupMenuToolsMenuTableViewId;
  } else if (type == PopupMenuTypeNavigationBackward ||
             type == PopupMenuTypeNavigationForward) {
    tableViewController.tableView.accessibilityIdentifier =
        kPopupMenuNavigationTableViewId;
  } else if (type == PopupMenuTypeTabGrid) {
    tableViewController.tableView.accessibilityIdentifier =
        kPopupMenuTabGridMenuTableViewId;
  }

  self.viewController = tableViewController;

  BOOL triggerNewIncognitoTabTip = NO;
  if (type == PopupMenuTypeToolsMenu) {
    triggerNewIncognitoTabTip =
        self.bubblePresenter.incognitoTabTipBubblePresenter
            .triggerFollowUpAction;
    self.bubblePresenter.incognitoTabTipBubblePresenter.triggerFollowUpAction =
        NO;
  }

  OverlayPresenter* overlayPresenter = OverlayPresenter::FromBrowser(
      self.browser, OverlayModality::kWebContentArea);
  self.contentBlockerMediator = [[BrowserContainerMediator alloc]
                initWithWebStateList:self.browser->GetWebStateList()
      webContentAreaOverlayPresenter:overlayPresenter];

  // Create the overflow menu mediator first so the popup mediator isn't created
  // if not needed.
  if (type == PopupMenuTypeToolsMenu && IsNewOverflowMenuEnabled()) {
    if (@available(iOS 15, *)) {
      self.overflowMenuMediator = [[OverflowMenuMediator alloc] init];
      self.overflowMenuMediator.dispatcher =
          static_cast<id<ApplicationCommands, BrowserCommands,
                         FindInPageCommands, TextZoomCommands>>(
              self.browser->GetCommandDispatcher());
      self.overflowMenuMediator.webStateList = self.browser->GetWebStateList();
      self.overflowMenuMediator.navigationAgent =
          WebNavigationBrowserAgent::FromBrowser(self.browser);
      self.overflowMenuMediator.baseViewController = self.baseViewController;
      self.overflowMenuMediator.isIncognito =
          self.browser->GetBrowserState()->IsOffTheRecord();
      self.overflowMenuMediator.bookmarkModel =
          ios::BookmarkModelFactory::GetForBrowserState(
              self.browser->GetBrowserState());
      self.overflowMenuMediator.prefService =
          self.browser->GetBrowserState()->GetPrefs();
      self.overflowMenuMediator.engagementTracker =
          feature_engagement::TrackerFactory::GetForBrowserState(
              self.browser->GetBrowserState());
      self.overflowMenuMediator.webContentAreaOverlayPresenter =
          overlayPresenter;
      self.overflowMenuMediator.browserPolicyConnector =
          GetApplicationContext()->GetBrowserPolicyConnector();

      if (IsWebChannelsEnabled()) {
        self.overflowMenuMediator.followActionState = GetFollowActionState(
            self.browser->GetWebStateList()->GetActiveWebState(),
            self.browser->GetBrowserState());
        ios::GetChromeBrowserProvider()
            .GetFollowProvider()
            ->SetFollowEventDelegate(self.browser);
      } else {
        self.overflowMenuMediator.followActionState = FollowActionStateHidden;
      }

      self.contentBlockerMediator.consumer = self.overflowMenuMediator;

      UIViewController* menu = [OverflowMenuViewProvider
          makeViewControllerWithModel:self.overflowMenuMediator
                                          .overflowMenuModel];

      NamedGuide* guide =
          [NamedGuide guideWithName:guideName
                               view:self.baseViewController.view];
      menu.modalPresentationStyle = UIModalPresentationPopover;

      UIPopoverPresentationController* popoverPresentationController =
          menu.popoverPresentationController;
      popoverPresentationController.sourceView = guide.constrainedView;
      popoverPresentationController.sourceRect = guide.constrainedView.bounds;
      popoverPresentationController.permittedArrowDirections =
          UIPopoverArrowDirectionUp;
      popoverPresentationController.delegate = self;
      popoverPresentationController.backgroundColor =
          [UIColor colorNamed:kBackgroundColor];

      // The adaptive controller adjusts styles based on window size: sheet for
      // slim windows on iPhone and iPad, popover for larger windows on ipad.
      UISheetPresentationController* sheetPresentationController =
          popoverPresentationController.adaptiveSheetPresentationController;
      if (sheetPresentationController) {
        sheetPresentationController.delegate = self;
        sheetPresentationController.prefersGrabberVisible = YES;
        sheetPresentationController.prefersEdgeAttachedInCompactHeight = YES;
        sheetPresentationController
            .widthFollowsPreferredContentSizeWhenEdgeAttached = YES;

        sheetPresentationController.detents = @[
          [UISheetPresentationControllerDetent mediumDetent],
          [UISheetPresentationControllerDetent largeDetent]
        ];
      }

      [self.UIUpdater updateUIForMenuDisplayed:type];
      [self.baseViewController presentViewController:menu
                                            animated:YES
                                          completion:nil];
      return;
    }
  }

  self.mediator = [[PopupMenuMediator alloc]
                   initWithType:type
                    isIncognito:self.browser->GetBrowserState()
                                    ->IsOffTheRecord()
               readingListModel:ReadingListModelFactory::GetForBrowserState(
                                    self.browser->GetBrowserState())
      triggerNewIncognitoTabTip:triggerNewIncognitoTabTip
         browserPolicyConnector:GetApplicationContext()
                                    ->GetBrowserPolicyConnector()];
  self.mediator.engagementTracker =
      feature_engagement::TrackerFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  self.mediator.webStateList = self.browser->GetWebStateList();
  self.mediator.dispatcher =
      static_cast<id<BrowserCommands>>(self.browser->GetCommandDispatcher());
  self.mediator.bookmarkModel = ios::BookmarkModelFactory::GetForBrowserState(
      self.browser->GetBrowserState());
  self.mediator.prefService = self.browser->GetBrowserState()->GetPrefs();
  self.mediator.templateURLService =
      ios::TemplateURLServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  self.mediator.popupMenu = tableViewController;
  self.mediator.webContentAreaOverlayPresenter = overlayPresenter;
  self.mediator.URLLoadingBrowserAgent =
      UrlLoadingBrowserAgent::FromBrowser(self.browser);

  self.contentBlockerMediator.consumer = self.mediator;

  self.actionHandler = [[PopupMenuActionHandler alloc] init];
  self.actionHandler.baseViewController = self.baseViewController;
  self.actionHandler.dispatcher =
      static_cast<id<ApplicationCommands, BrowserCommands, FindInPageCommands,
                     LoadQueryCommands, TextZoomCommands>>(
          self.browser->GetCommandDispatcher());
  self.actionHandler.delegate = self.mediator;
  self.actionHandler.navigationAgent =
      WebNavigationBrowserAgent::FromBrowser(self.browser);
  tableViewController.delegate = self.actionHandler;

  self.presenter = [[PopupMenuPresenter alloc] init];
  self.presenter.baseViewController = self.baseViewController;
  self.presenter.presentedViewController = tableViewController;
  self.presenter.guideName = guideName;
  self.presenter.delegate = self;

  [self.UIUpdater updateUIForMenuDisplayed:type];

  [self.presenter prepareForPresentation];
  [self.presenter presentAnimated:YES];
}

@end
