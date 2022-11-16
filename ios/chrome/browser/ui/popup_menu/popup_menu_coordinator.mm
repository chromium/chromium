// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/popup_menu_coordinator.h"

#import "base/check.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/feature_engagement/tracker_factory.h"
#import "ios/chrome/browser/follow/follow_action_state.h"
#import "ios/chrome/browser/follow/follow_browser_agent.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ntp/features.h"
#import "ios/chrome/browser/overlays/public/overlay_presenter.h"
#import "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/ui/browser_container/browser_container_mediator.h"
#import "ios/chrome/browser/ui/bubble/bubble_presenter.h"
#import "ios/chrome/browser/ui/bubble/bubble_view_controller_presenter.h"
#import "ios/chrome/browser/ui/commands/bookmarks_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/find_in_page_commands.h"
#import "ios/chrome/browser/ui/commands/lens_commands.h"
#import "ios/chrome/browser/ui/commands/page_info_commands.h"
#import "ios/chrome/browser/ui/commands/popup_menu_commands.h"
#import "ios/chrome/browser/ui/commands/price_notifications_commands.h"
#import "ios/chrome/browser/ui/commands/qr_scanner_commands.h"
#import "ios/chrome/browser/ui/main/layout_guide_util.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/feature_flags.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_mediator.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_swift.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_action_handler.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_help_coordinator.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_mediator.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_metrics_handler.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_presenter.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_presenter_delegate.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_table_view_controller.h"
#import "ios/chrome/browser/ui/presenters/contained_presenter_delegate.h"
#import "ios/chrome/browser/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/util/util_swift.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/web/web_navigation_browser_agent.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Returns the corresponding command type for a Popup menu `type`.
PopupMenuCommandType CommandTypeFromPopupType(PopupMenuType type) {
  if (type == PopupMenuTypeToolsMenu)
    return PopupMenuCommandTypeToolsMenu;
  return PopupMenuCommandTypeDefault;
}

// Enum for IOS.OverflowMenu.ActionType histogram.
// Entries should not be renumbered and numeric values should never be reused.
enum class IOSOverflowMenuActionType {
  kNoScrollNoAction = 0,
  kScrollNoAction = 1,
  kNoScrollAction = 2,
  kScrollAction = 3,
  kMaxValue = kScrollAction,
};

}  // namespace

@interface PopupMenuCoordinator () <PopupMenuCommands,
                                    PopupMenuMetricsHandler,
                                    PopupMenuPresenterDelegate,
                                    UIPopoverPresentationControllerDelegate,
                                    UISheetPresentationControllerDelegate>

// Presenter for the popup menu, managing the animations.
@property(nonatomic, strong) PopupMenuPresenter* presenter;
// Mediator for the popup menu.
@property(nonatomic, strong) PopupMenuMediator* mediator;
// Mediator for the overflow menu
@property(nonatomic, strong) OverflowMenuMediator* overflowMenuMediator;
// Mediator to that alerts the main `mediator` when the web content area
// is blocked by an overlay.
@property(nonatomic, strong) BrowserContainerMediator* contentBlockerMediator;
// ViewController for this mediator.
@property(nonatomic, strong) PopupMenuTableViewController* viewController;
// Handles user interaction with the popup menu items.
@property(nonatomic, strong) PopupMenuActionHandler* actionHandler;
// Time when the presentation of the popup menu is requested.
@property(nonatomic, assign) NSTimeInterval requestStartTime;

// Time when the tools menu opened.
@property(nonatomic, assign) NSTimeInterval toolsMenuOpenTime;
// Whether the tools menu was scrolled vertically while it was open.
@property(nonatomic, assign) BOOL toolsMenuWasScrolledVertically;
// Whether the tools menu was scrolled horizontally while it was open.
@property(nonatomic, assign) BOOL toolsMenuWasScrolledHorizontally;
// Whether the user took an action on the tools menu while it was open.
@property(nonatomic, assign) BOOL toolsMenuUserTookAction;

@property(nonatomic, strong) PopupMenuHelpCoordinator* popupMenuHelpCoordinator;

@end

@implementation PopupMenuCoordinator

@synthesize mediator = _mediator;
@synthesize presenter = _presenter;
@synthesize requestStartTime = _requestStartTime;
@synthesize UIUpdater = _UIUpdater;
@synthesize bubblePresenter = _bubblePresenter;
@synthesize viewController = _viewController;
@synthesize baseViewController = _baseViewController;

- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);
  return [super initWithBaseViewController:nil browser:browser];
}

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
  if (self.isShowingPopupMenu) {
    [self dismissPopupMenuAnimated:NO];
  }
  [self.popupMenuHelpCoordinator stop];
  [self.browser->GetCommandDispatcher() stopDispatchingToTarget:self];
  [self.overflowMenuMediator disconnect];
  self.overflowMenuMediator = nil;
  [self.mediator disconnect];
  self.mediator = nil;
  self.viewController = nil;
}

#pragma mark - Public

- (BOOL)isShowingPopupMenu {
  return self.presenter != nil || self.overflowMenuMediator != nil;
}

- (void)startPopupMenuHelpCoordinator {
  self.popupMenuHelpCoordinator = [[PopupMenuHelpCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser];
  self.popupMenuHelpCoordinator.UIUpdater = self.UIUpdater;
  [self.popupMenuHelpCoordinator start];
}

#pragma mark - PopupMenuCommands

- (void)showNavigationHistoryBackPopupMenu {
  base::RecordAction(
      base::UserMetricsAction("MobileToolbarShowTabHistoryMenu"));
  [self presentPopupOfType:PopupMenuTypeNavigationBackward
      fromLayoutGuideNamed:kBackButtonGuide];
}

- (void)showNavigationHistoryForwardPopupMenu {
  base::RecordAction(
      base::UserMetricsAction("MobileToolbarShowTabHistoryMenu"));
  [self presentPopupOfType:PopupMenuTypeNavigationForward
      fromLayoutGuideNamed:kForwardButtonGuide];
}

- (void)showToolsMenuPopup {
  // The metric is registered at the toolbar level.
  [self presentPopupOfType:PopupMenuTypeToolsMenu
      fromLayoutGuideNamed:kToolsMenuGuide];
}

- (void)showTabGridButtonPopup {
  base::RecordAction(base::UserMetricsAction("MobileToolbarShowTabGridMenu"));
  [self presentPopupOfType:PopupMenuTypeTabGrid
      fromLayoutGuideNamed:kTabSwitcherGuide];
}

- (void)showNewTabButtonPopup {
  base::RecordAction(base::UserMetricsAction("MobileToolbarShowNewTabMenu"));
  [self presentPopupOfType:PopupMenuTypeNewTab
      fromLayoutGuideNamed:kNewTabButtonGuide];
}

- (void)dismissPopupMenuAnimated:(BOOL)animated {
  [self.UIUpdater updateUIForMenuDismissed];

  if (self.toolsMenuOpenTime != 0) {
    base::TimeDelta elapsed = base::Seconds(
        [NSDate timeIntervalSinceReferenceDate] - self.toolsMenuOpenTime);
    UMA_HISTOGRAM_MEDIUM_TIMES("IOS.OverflowMenu.TimeOpen", elapsed);
    // Reset the start time to ensure that whatever happens, we only record
    // this once.
    self.toolsMenuOpenTime = 0;

    IOSOverflowMenuActionType actionType;
    if (self.toolsMenuWasScrolledVertically) {
      if (self.toolsMenuUserTookAction) {
        actionType = IOSOverflowMenuActionType::kScrollAction;
      } else {
        actionType = IOSOverflowMenuActionType::kScrollNoAction;
      }
    } else {
      if (self.toolsMenuUserTookAction) {
        actionType = IOSOverflowMenuActionType::kNoScrollAction;
      } else {
        actionType = IOSOverflowMenuActionType::kNoScrollNoAction;
      }
    }
    base::UmaHistogramEnumeration("IOS.OverflowMenu.ActionType", actionType);

    if (!self.toolsMenuWasScrolledHorizontally &&
        !self.toolsMenuUserTookAction) {
      [self trackToolsMenuNoHorizontalScrollOrAction];
    }
    self.toolsMenuWasScrolledVertically = NO;
    self.toolsMenuWasScrolledHorizontally = NO;
    self.toolsMenuUserTookAction = NO;
  }

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

- (void)presentationControllerWillDismiss:
    (UIPresentationController*)presentationController {
  // Update the UI before dismissal starts. Technically, on iPhone, the user
  // could be interactively dismissing a sheet, which they could then cancel
  // (leading to state mismatch: visible menu, but UIUpdater with menu
  // dismissed). However, the UIUpdater only modifies the toolbar, which is
  // hidden behind the sheet anyway.
  [self.UIUpdater updateUIForMenuDismissed];
}

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self dismissPopupMenuAnimated:NO];
}

#pragma mark - UISheetPresentationControllerDelegate

- (void)sheetPresentationControllerDidChangeSelectedDetentIdentifier:
    (UISheetPresentationController*)sheetPresentationController
    API_AVAILABLE(ios(15)) {
  [self popupMenuScrolledVertically];
}

#pragma mark - PopupMenuMetricsHandler

- (void)popupMenuScrolledVertically {
  self.toolsMenuWasScrolledVertically = YES;
}

- (void)popupMenuScrolledHorizontally {
  self.toolsMenuWasScrolledHorizontally = YES;
}

- (void)popupMenuTookAction {
  self.toolsMenuUserTookAction = YES;
}

#pragma mark - Notification callback

- (void)applicationDidEnterBackground:(NSNotification*)note {
  [self dismissPopupMenuAnimated:NO];
}

#pragma mark - Private

// Presents a popup menu of type `type` with an animation starting from the
// layout named `guideName`.
- (void)presentPopupOfType:(PopupMenuType)type
      fromLayoutGuideNamed:(GuideName*)guideName {
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
  if (type == PopupMenuTypeToolsMenu) {
    self.toolsMenuOpenTime = [NSDate timeIntervalSinceReferenceDate];
    self.toolsMenuWasScrolledVertically = NO;
    self.toolsMenuWasScrolledHorizontally = NO;
    self.toolsMenuUserTookAction = NO;
    if (IsNewOverflowMenuEnabled()) {
      if (@available(iOS 15, *)) {
        self.overflowMenuMediator = [[OverflowMenuMediator alloc] init];
        self.overflowMenuMediator.dispatcher = static_cast<
            id<ActivityServiceCommands, ApplicationCommands, BrowserCommands,
               BrowserCoordinatorCommands, FindInPageCommands,
               PriceNotificationsCommands, TextZoomCommands>>(
            self.browser->GetCommandDispatcher());
        self.overflowMenuMediator.bookmarksCommandsHandler = HandlerForProtocol(
            self.browser->GetCommandDispatcher(), BookmarksCommands);
        self.overflowMenuMediator.pageInfoCommandsHandler = HandlerForProtocol(
            self.browser->GetCommandDispatcher(), PageInfoCommands);
        self.overflowMenuMediator.popupMenuCommandsHandler = HandlerForProtocol(
            self.browser->GetCommandDispatcher(), PopupMenuCommands);
        self.overflowMenuMediator.webStateList =
            self.browser->GetWebStateList();
        self.overflowMenuMediator.navigationAgent =
            WebNavigationBrowserAgent::FromBrowser(self.browser);
        self.overflowMenuMediator.baseViewController = self.baseViewController;
        self.overflowMenuMediator.isIncognito =
            self.browser->GetBrowserState()->IsOffTheRecord();
        self.overflowMenuMediator.bookmarkModel =
            ios::BookmarkModelFactory::GetForBrowserState(
                self.browser->GetBrowserState());
        self.overflowMenuMediator.browserStatePrefs =
            self.browser->GetBrowserState()->GetPrefs();
        self.overflowMenuMediator.localStatePrefs =
            GetApplicationContext()->GetLocalState();
        self.overflowMenuMediator.engagementTracker =
            feature_engagement::TrackerFactory::GetForBrowserState(
                self.browser->GetBrowserState());
        self.overflowMenuMediator.webContentAreaOverlayPresenter =
            overlayPresenter;
        self.overflowMenuMediator.browserPolicyConnector =
            GetApplicationContext()->GetBrowserPolicyConnector();

        if (IsWebChannelsEnabled()) {
          self.overflowMenuMediator.followBrowserAgent =
              FollowBrowserAgent::FromBrowser(self.browser);
        }

        self.contentBlockerMediator.consumer = self.overflowMenuMediator;

        OverflowMenuUIConfiguration* uiConfiguration =
            [[OverflowMenuUIConfiguration alloc]
                initWithPresentingViewControllerHorizontalSizeClass:
                    self.baseViewController.traitCollection.horizontalSizeClass
                          presentingViewControllerVerticalSizeClass:
                              self.baseViewController.traitCollection
                                  .verticalSizeClass];

        self.popupMenuHelpCoordinator.uiConfiguration = uiConfiguration;

        UIViewController* menu = [OverflowMenuViewProvider
            makeViewControllerWithModel:self.overflowMenuMediator
                                            .overflowMenuModel
                        uiConfiguration:uiConfiguration
                         metricsHandler:self
                carouselMetricsDelegate:self.overflowMenuMediator];

        LayoutGuideCenter* layoutGuideCenter =
            LayoutGuideCenterForBrowser(self.browser);
        UILayoutGuide* layoutGuide =
            [layoutGuideCenter makeLayoutGuideNamed:guideName];
        [self.baseViewController.view addLayoutGuide:layoutGuide];

        menu.modalPresentationStyle = UIModalPresentationPopover;

        UIPopoverPresentationController* popoverPresentationController =
            menu.popoverPresentationController;
        popoverPresentationController.sourceView = self.baseViewController.view;
        popoverPresentationController.sourceRect = layoutGuide.layoutFrame;
        popoverPresentationController.permittedArrowDirections =
            UIPopoverArrowDirectionUp;
        popoverPresentationController.delegate = self;
        popoverPresentationController.backgroundColor =
            [UIColor colorNamed:kBackgroundColor];

        // The adaptive controller adjusts styles based on window size: sheet
        // for slim windows on iPhone and iPad, popover for larger windows on
        // ipad.
        UISheetPresentationController* sheetPresentationController =
            popoverPresentationController.adaptiveSheetPresentationController;
        if (sheetPresentationController) {
          sheetPresentationController.delegate = self;
          sheetPresentationController.prefersGrabberVisible = YES;
          sheetPresentationController.prefersEdgeAttachedInCompactHeight = YES;
          sheetPresentationController
              .widthFollowsPreferredContentSizeWhenEdgeAttached = YES;

          NSArray<UISheetPresentationControllerDetent*>* regularDetents = @[
            [UISheetPresentationControllerDetent mediumDetent],
            [UISheetPresentationControllerDetent largeDetent]
          ];

          NSArray<UISheetPresentationControllerDetent*>* largeTextDetents =
              @[ [UISheetPresentationControllerDetent largeDetent] ];

          BOOL hasLargeText = UIContentSizeCategoryIsAccessibilityCategory(
              menu.traitCollection.preferredContentSizeCategory);
          sheetPresentationController.detents =
              hasLargeText ? largeTextDetents : regularDetents;
        }

        __weak __typeof(self) weakSelf = self;
        [self.UIUpdater updateUIForMenuDisplayed:type];
        [self.baseViewController
            presentViewController:menu
                         animated:YES
                       completion:^{
                         [weakSelf.popupMenuHelpCoordinator
                             showOverflowMenuIPHInViewController:menu];
                       }];
        return;
      }
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
  self.mediator.browserCommandsHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), BrowserCommands);
  self.mediator.lensCommandsHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), LensCommands);
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
  if (IsWebChannelsEnabled()) {
    self.mediator.followBrowserAgent =
        FollowBrowserAgent::FromBrowser(self.browser);
  }

  self.contentBlockerMediator.consumer = self.mediator;

  self.actionHandler = [[PopupMenuActionHandler alloc] init];
  self.actionHandler.baseViewController = self.baseViewController;
  self.actionHandler.dispatcher = static_cast<
      id<ApplicationCommands, BrowserCommands, FindInPageCommands,
         LoadQueryCommands, PriceNotificationsCommands, TextZoomCommands>>(
      self.browser->GetCommandDispatcher());
  self.actionHandler.bookmarksCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), BookmarksCommands);
  self.actionHandler.browserCoordinatorCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), BrowserCoordinatorCommands);
  self.actionHandler.pageInfoCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), PageInfoCommands);
  self.actionHandler.popupMenuCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), PopupMenuCommands);
  self.actionHandler.qrScannerCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), QRScannerCommands);
  self.actionHandler.delegate = self.mediator;
  self.actionHandler.navigationAgent =
      WebNavigationBrowserAgent::FromBrowser(self.browser);
  tableViewController.delegate = self.actionHandler;

  self.presenter = [[PopupMenuPresenter alloc] init];
  self.presenter.baseViewController = self.baseViewController;
  self.presenter.presentedViewController = tableViewController;
  LayoutGuideCenter* layoutGuideCenter =
      LayoutGuideCenterForBrowser(self.browser);
  UILayoutGuide* layoutGuide =
      [layoutGuideCenter makeLayoutGuideNamed:guideName];
  [self.baseViewController.view addLayoutGuide:layoutGuide];
  self.presenter.layoutGuide = layoutGuide;
  self.presenter.delegate = self;

  [self.UIUpdater updateUIForMenuDisplayed:type];

  [self.presenter prepareForPresentation];
  [self.presenter presentAnimated:YES];

  // Scrolls happen during prepareForPresentation, so only attach the metrics
  // handler after presentation is done.
  if (type == PopupMenuTypeToolsMenu) {
    tableViewController.metricsHandler = self;
  }
}

- (void)trackToolsMenuNoHorizontalScrollOrAction {
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  if (!browserState) {
    return;
  }
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForBrowserState(browserState);
  if (!tracker) {
    return;
  }

  tracker->NotifyEvent(
      feature_engagement::events::kOverflowMenuNoHorizontalScrollOrAction);
}

@end
