// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/popup_menu_coordinator.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/check.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/bookmarks/account_bookmark_model_factory.h"
#import "ios/chrome/browser/bookmarks/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/feature_engagement/tracker_factory.h"
#import "ios/chrome/browser/follow/follow_action_state.h"
#import "ios/chrome/browser/follow/follow_browser_agent.h"
#import "ios/chrome/browser/ntp/features.h"
#import "ios/chrome/browser/overlays/public/overlay_presenter.h"
#import "ios/chrome/browser/promos_manager/promos_manager_factory.h"
#import "ios/chrome/browser/reading_list/reading_list_browser_agent.h"
#import "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/default_browser_promo/non_modal_default_browser_promo_scheduler_scene_agent.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/bookmarks_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/find_in_page_commands.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/overflow_menu_customization_commands.h"
#import "ios/chrome/browser/shared/public/commands/page_info_commands.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/price_notifications_commands.h"
#import "ios/chrome/browser/shared/public/commands/qr_scanner_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/supervised_user/supervised_user_service_factory.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/tabs/features.h"
#import "ios/chrome/browser/ui/browser_container/browser_container_mediator.h"
#import "ios/chrome/browser/ui/bubble/bubble_presenter.h"
#import "ios/chrome/browser/ui/bubble/bubble_view_controller_presenter.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/feature_flags.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/menu_customization_coordinator.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_mediator.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_orderer.h"
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
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/web/web_navigation_browser_agent.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"

using base::RecordAction;
using base::UserMetricsAction;

namespace {

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

@interface PopupMenuCoordinator () <OverflowMenuCustomizationCommands,
                                    PopupMenuCommands,
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

@implementation PopupMenuCoordinator {
  OverflowMenuModel* _overflowMenuModel;

  OverflowMenuOrderer* _overflowMenuOrderer;

  MenuCustomizationCoordinator* _menuCustomizationCoordinator;
}

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
  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(OverflowMenuCustomizationCommands)];
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

- (void)showToolsMenuPopup {
  if (self.presenter || self.overflowMenuMediator) {
    [self dismissPopupMenuAnimated:YES];
  }

  id<OmniboxCommands> omniboxCommandsHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), OmniboxCommands);
  // Dismiss the omnibox (if open).
  [omniboxCommandsHandler cancelOmniboxEdit];

  id<BrowserCommands> callableDispatcher =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), BrowserCommands);
  [callableDispatcher dismissSoftKeyboard];

  id<FindInPageCommands> findInPageCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), FindInPageCommands);
  // Dismiss Find in Page focus.
  [findInPageCommandsHandler defocusFindInPage];

  SceneState* sceneState =
      SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();
  NonModalDefaultBrowserPromoSchedulerSceneAgent* nonModalPromoScheduler =
      [NonModalDefaultBrowserPromoSchedulerSceneAgent
          agentFromScene:sceneState];
  // Allow the non-modal promo scheduler to close the promo.
  [nonModalPromoScheduler logPopupMenuEntered];

  [self.bubblePresenter toolsMenuDisplayed];

  self.requestStartTime = [NSDate timeIntervalSinceReferenceDate];

  PopupMenuTableViewController* tableViewController =
      [[PopupMenuTableViewController alloc] init];
  tableViewController.baseViewController = self.baseViewController;
  tableViewController.tableView.accessibilityIdentifier =
      kPopupMenuToolsMenuTableViewId;

  self.viewController = tableViewController;

  BOOL triggerNewIncognitoTabTip =
      self.bubblePresenter.incognitoTabTipBubblePresenter.triggerFollowUpAction;
  self.bubblePresenter.incognitoTabTipBubblePresenter.triggerFollowUpAction =
      NO;

  OverlayPresenter* overlayPresenter = OverlayPresenter::FromBrowser(
      self.browser, OverlayModality::kWebContentArea);
  self.contentBlockerMediator = [[BrowserContainerMediator alloc]
                initWithWebStateList:self.browser->GetWebStateList()
      webContentAreaOverlayPresenter:overlayPresenter];

  // Create the overflow menu mediator first so the popup mediator isn't created
  // if not needed.
  self.toolsMenuOpenTime = [NSDate timeIntervalSinceReferenceDate];
  self.toolsMenuWasScrolledVertically = NO;
  self.toolsMenuWasScrolledHorizontally = NO;
  self.toolsMenuUserTookAction = NO;
  if (IsNewOverflowMenuEnabled()) {
    if (@available(iOS 15, *)) {
      self.overflowMenuMediator = [[OverflowMenuMediator alloc] init];

      CGFloat screenWidth = self.baseViewController.view.frame.size.width;
      UIContentSizeCategory contentSizeCategory =
          self.baseViewController.traitCollection.preferredContentSizeCategory;

      BOOL isIncognito = self.browser->GetBrowserState()->IsOffTheRecord();
      self.overflowMenuMediator.isIncognito = isIncognito;
      _overflowMenuOrderer =
          [[OverflowMenuOrderer alloc] initWithIsIncognito:isIncognito];
      _overflowMenuOrderer.visibleDestinationsCount =
          [OverflowMenuUIConfiguration
              numDestinationsVisibleWithoutHorizontalScrollingForScreenWidth:
                  screenWidth
                                                      forContentSizeCategory:
                                                          contentSizeCategory];
      _overflowMenuOrderer.localStatePrefs =
          GetApplicationContext()->GetLocalState();

      self.overflowMenuMediator.menuOrderer = _overflowMenuOrderer;
      self.overflowMenuMediator.dispatcher =
          static_cast<id<ActivityServiceCommands, ApplicationCommands,
                         BrowserCoordinatorCommands, FindInPageCommands,
                         OverflowMenuCustomizationCommands,
                         PriceNotificationsCommands, TextZoomCommands>>(
              self.browser->GetCommandDispatcher());
      self.overflowMenuMediator.bookmarksCommandsHandler = HandlerForProtocol(
          self.browser->GetCommandDispatcher(), BookmarksCommands);
      self.overflowMenuMediator.pageInfoCommandsHandler = HandlerForProtocol(
          self.browser->GetCommandDispatcher(), PageInfoCommands);
      self.overflowMenuMediator.popupMenuCommandsHandler = HandlerForProtocol(
          self.browser->GetCommandDispatcher(), PopupMenuCommands);
      self.overflowMenuMediator.webStateList = self.browser->GetWebStateList();
      self.overflowMenuMediator.navigationAgent =
          WebNavigationBrowserAgent::FromBrowser(self.browser);
      self.overflowMenuMediator.baseViewController = self.baseViewController;
      self.overflowMenuMediator.localOrSyncableBookmarkModel =
          ios::LocalOrSyncableBookmarkModelFactory::GetForBrowserState(
              self.browser->GetBrowserState());
      self.overflowMenuMediator.accountBookmarkModel =
          ios::AccountBookmarkModelFactory::GetForBrowserState(
              self.browser->GetBrowserState());
      self.overflowMenuMediator.readingListModel =
          ReadingListModelFactory::GetInstance()->GetForBrowserState(
              self.browser->GetBrowserState());
      self.overflowMenuMediator.browserStatePrefs =
          self.browser->GetBrowserState()->GetPrefs();
      self.overflowMenuMediator.engagementTracker =
          feature_engagement::TrackerFactory::GetForBrowserState(
              self.browser->GetBrowserState());
      self.overflowMenuMediator.webContentAreaOverlayPresenter =
          overlayPresenter;
      self.overflowMenuMediator.browserPolicyConnector =
          GetApplicationContext()->GetBrowserPolicyConnector();
      self.overflowMenuMediator.syncService =
          SyncServiceFactory::GetForBrowserState(
              self.browser->GetBrowserState());
      self.overflowMenuMediator.supervisedUserService =
          SupervisedUserServiceFactory::GetForBrowserState(
              self.browser->GetBrowserState());
      self.overflowMenuMediator.promosManager =
          PromosManagerFactory::GetForBrowserState(
              self.browser->GetBrowserState());
      self.overflowMenuMediator.readingListBrowserAgent =
          ReadingListBrowserAgent::FromBrowser(self.browser);
      if (IsWebChannelsEnabled()) {
        self.overflowMenuMediator.followBrowserAgent =
            FollowBrowserAgent::FromBrowser(self.browser);
      }

      self.contentBlockerMediator.consumer = self.overflowMenuMediator;

      NSInteger highlightDestination =
          [self.popupMenuHelpCoordinator highlightDestination] == nil
              ? -1
              : [[self.popupMenuHelpCoordinator highlightDestination]
                    integerValue];
      OverflowMenuUIConfiguration* uiConfiguration =
          [[OverflowMenuUIConfiguration alloc]
              initWithPresentingViewControllerHorizontalSizeClass:
                  self.baseViewController.traitCollection.horizontalSizeClass
                        presentingViewControllerVerticalSizeClass:
                            self.baseViewController.traitCollection
                                .verticalSizeClass
                                             highlightDestination:
                                                 highlightDestination];

      self.popupMenuHelpCoordinator.uiConfiguration = uiConfiguration;

      _overflowMenuModel = [[OverflowMenuModel alloc] initWithDestinations:@[]
                                                              actionGroups:@[]];

      _overflowMenuOrderer.model = _overflowMenuModel;
      self.overflowMenuMediator.model = _overflowMenuModel;

      UIViewController* menu = [OverflowMenuViewProvider
          makeViewControllerWithModel:_overflowMenuModel
                      uiConfiguration:uiConfiguration
                       metricsHandler:self];

      LayoutGuideCenter* layoutGuideCenter =
          LayoutGuideCenterForBrowser(self.browser);
      UILayoutGuide* layoutGuide =
          [layoutGuideCenter makeLayoutGuideNamed:kToolsMenuGuide];
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
      [self.baseViewController
          presentViewController:menu
                       animated:YES
                     completion:^{
                       [weakSelf.popupMenuHelpCoordinator
                           showHistoryOnOverflowMenuIPHInViewController:menu];
                     }];
      return;
    }
  }

  self.mediator = [[PopupMenuMediator alloc]
            initWithIsIncognito:self.browser->GetBrowserState()
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
  self.mediator.readingListBrowserAgent =
      ReadingListBrowserAgent::FromBrowser(self.browser);
  self.mediator.lensCommandsHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), LensCommands);
  self.mediator.bookmarkModel =
      ios::LocalOrSyncableBookmarkModelFactory::GetForBrowserState(
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
      [layoutGuideCenter makeLayoutGuideNamed:kToolsMenuGuide];
  [self.baseViewController.view addLayoutGuide:layoutGuide];
  self.presenter.layoutGuide = layoutGuide;
  self.presenter.delegate = self;

  [self.presenter prepareForPresentation];
  [self.presenter presentAnimated:YES];

  // Scrolls happen during prepareForPresentation, so only attach the metrics
  // handler after presentation is done.
  tableViewController.metricsHandler = self;
}

- (void)dismissPopupMenuAnimated:(BOOL)animated {
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
    __weak __typeof(self) weakSelf = self;
    [self.baseViewController
        dismissViewControllerAnimated:animated
                           completion:^{
                             [weakSelf.bubblePresenter presentTabPinnedBubble];
                           }];
    _overflowMenuModel = nil;
    [_overflowMenuOrderer disconnect];
    _overflowMenuOrderer = nil;
    [self.overflowMenuMediator disconnect];
    self.overflowMenuMediator = nil;
  }
  [self.presenter dismissAnimated:animated];
  self.presenter = nil;
  [self.mediator disconnect];
  self.mediator = nil;
  self.viewController = nil;
}

- (void)showSnackbarForPinnedState:(BOOL)pinnedState
                          webState:(web::WebState*)webState {
  DCHECK(IsPinnedTabsOverflowEnabled());
  int messageId = pinnedState ? IDS_IOS_SNACKBAR_MESSAGE_PINNED_TAB
                              : IDS_IOS_SNACKBAR_MESSAGE_UNPINNED_TAB;

  base::WeakPtr<web::WebState> weakWebState = webState->GetWeakPtr();
  base::WeakPtr<Browser> weakBrowser = self.browser->AsWeakPtr();

  void (^undoAction)() = ^{
    if (pinnedState) {
      RecordAction(UserMetricsAction("MobileSnackbarUndoPinAction"));
    } else {
      RecordAction(UserMetricsAction("MobileSnackbarUndoUnpinAction"));
    }

    Browser* browser = weakBrowser.get();
    if (!browser) {
      return;
    }
    [OverflowMenuMediator setTabPinned:!pinnedState
                              webState:weakWebState.get()
                          webStateList:browser->GetWebStateList()];
  };

  MDCSnackbarMessage* message =
      [MDCSnackbarMessage messageWithText:l10n_util::GetNSString(messageId)];

  MDCSnackbarMessageAction* action = [[MDCSnackbarMessageAction alloc] init];
  action.handler = undoAction;
  action.title = l10n_util::GetNSString(IDS_IOS_SNACKBAR_ACTION_UNDO);
  message.action = action;

  id<SnackbarCommands> snackbarCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SnackbarCommands);
  [snackbarCommandsHandler showSnackbarMessage:message];
}

#pragma mark - OverflowMenuCustomizationCommands

- (void)showActionCustomization {
  _menuCustomizationCoordinator = [[MenuCustomizationCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser];
  _menuCustomizationCoordinator.menuOrderer = _overflowMenuOrderer;
  [_menuCustomizationCoordinator start];
}

- (void)hideActionCustomization {
  [_menuCustomizationCoordinator stop];
  _menuCustomizationCoordinator = nil;
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
