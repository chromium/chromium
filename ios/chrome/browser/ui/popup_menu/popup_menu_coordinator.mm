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
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_view_controller_presenter.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/follow/model/follow_action_state.h"
#import "ios/chrome/browser/follow/model/follow_browser_agent.h"
#import "ios/chrome/browser/iph_for_new_chrome_user/model/tab_based_iph_browser_agent.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_availability.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager_factory.h"
#import "ios/chrome/browser/reading_list/model/reading_list_browser_agent.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/default_browser_promo/non_modal_default_browser_promo_scheduler_scene_agent.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_commands.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/bookmarks_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/find_in_page_commands.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/overflow_menu_customization_commands.h"
#import "ios/chrome/browser/shared/public/commands/page_info_commands.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/price_notifications_commands.h"
#import "ios/chrome/browser/shared/public/commands/qr_scanner_commands.h"
#import "ios/chrome/browser/shared/public/commands/quick_delete_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/commands/text_zoom_commands.h"
#import "ios/chrome/browser/shared/public/commands/whats_new_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/browser_container/browser_container_mediator.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/feature_flags.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_mediator.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_metrics.h"
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
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/web/model/web_navigation_browser_agent.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"

using base::RecordAction;
using base::UserMetricsAction;

@interface PopupMenuCoordinator () <MenuCustomizationEventHandler,
                                    OverflowMenuCustomizationCommands,
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

// Time when the tools menu opened.
@property(nonatomic, assign) NSTimeInterval toolsMenuOpenTime;
// Whether the tools menu was scrolled vertically while it was open.
@property(nonatomic, assign) BOOL toolsMenuWasScrolledVertically;
// Whether the tools menu was scrolled horizontally while it was open.
@property(nonatomic, assign) BOOL toolsMenuWasScrolledHorizontally;
// Whether the user took an action on the tools menu while it was open.
@property(nonatomic, assign) BOOL toolsMenuUserTookAction;
// Whether the user selected an Action on the overflow menu (the vertical list).
@property(nonatomic, assign) BOOL overflowMenuUserSelectedAction;
// Whether the user selected a Destination on the overflow menu (the horizontal
// list).
@property(nonatomic, assign) BOOL overflowMenuUserSelectedDestination;
// Whether the user scrolled to the end of the actions section during their
// interaction.
@property(nonatomic, assign) BOOL overflowMenuUserScrolledToEndOfActions;

@property(nonatomic, strong) PopupMenuHelpCoordinator* popupMenuHelpCoordinator;

@end

@implementation PopupMenuCoordinator {
  OverflowMenuModel* _overflowMenuModel;

  OverflowMenuOrderer* _overflowMenuOrderer;

  // Stores whether certain events occured during an overflow menu session for
  // logs.
  OverflowMenuVisitedEvent _event;
}

@synthesize mediator = _mediator;
@synthesize presenter = _presenter;
@synthesize UIUpdater = _UIUpdater;
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

  SceneState* sceneState = self.browser->GetSceneState();
  NonModalDefaultBrowserPromoSchedulerSceneAgent* nonModalPromoScheduler =
      [NonModalDefaultBrowserPromoSchedulerSceneAgent
          agentFromScene:sceneState];
  // Allow the non-modal promo scheduler to close the promo.
  [nonModalPromoScheduler logPopupMenuEntered];

  PopupMenuTableViewController* tableViewController =
      [[PopupMenuTableViewController alloc] init];
  tableViewController.baseViewController = self.baseViewController;
  tableViewController.tableView.accessibilityIdentifier =
      kPopupMenuToolsMenuTableViewId;

  self.viewController = tableViewController;

  OverlayPresenter* overlayPresenter = OverlayPresenter::FromBrowser(
      self.browser, OverlayModality::kWebContentArea);
  self.contentBlockerMediator = [[BrowserContainerMediator alloc]
                initWithWebStateList:self.browser->GetWebStateList()
      webContentAreaOverlayPresenter:overlayPresenter];

  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForProfile(
          self.browser->GetProfile());

  // Create the overflow menu mediator first so the popup mediator isn't created
  // if not needed.
  self.toolsMenuOpenTime = [NSDate timeIntervalSinceReferenceDate];
  self.toolsMenuWasScrolledVertically = NO;
  self.toolsMenuWasScrolledHorizontally = NO;
  self.toolsMenuUserTookAction = NO;
  if (IsNewOverflowMenuEnabled()) {
    Browser* browser = self.browser;
    ProfileIOS* profile = browser->GetProfile();

    OverflowMenuMediator* mediator = [[OverflowMenuMediator alloc] init];

    CGFloat screenWidth = self.baseViewController.view.frame.size.width;
    UIContentSizeCategory contentSizeCategory =
        self.baseViewController.traitCollection.preferredContentSizeCategory;

    BOOL isIncognito = profile->IsOffTheRecord();
    mediator.isIncognito = isIncognito;
    _overflowMenuOrderer =
        [[OverflowMenuOrderer alloc] initWithIsIncognito:isIncognito];
    _overflowMenuOrderer.visibleDestinationsCount = [OverflowMenuUIConfiguration
        numDestinationsVisibleWithoutHorizontalScrollingForScreenWidth:
            screenWidth
                                                forContentSizeCategory:
                                                    contentSizeCategory];
    _overflowMenuOrderer.localStatePrefs =
        GetApplicationContext()->GetLocalState();

    mediator.menuOrderer = _overflowMenuOrderer;

    CommandDispatcher* dispatcher = browser->GetCommandDispatcher();

    mediator.activityServiceHandler =
        HandlerForProtocol(dispatcher, ActivityServiceCommands);
    mediator.applicationHandler =
        HandlerForProtocol(dispatcher, ApplicationCommands);
    mediator.settingsHandler = HandlerForProtocol(dispatcher, SettingsCommands);
    mediator.bookmarksHandler =
        HandlerForProtocol(dispatcher, BookmarksCommands);
    if (IsLensOverlayAvailable()) {
      mediator.lensOverlayHandler =
          HandlerForProtocol(dispatcher, LensOverlayCommands);
    }
    mediator.browserCoordinatorHandler =
        HandlerForProtocol(dispatcher, BrowserCoordinatorCommands);
    mediator.findInPageHandler =
        HandlerForProtocol(dispatcher, FindInPageCommands);
    mediator.helpHandler = HandlerForProtocol(dispatcher, HelpCommands);
    mediator.overflowMenuCustomizationHandler =
        HandlerForProtocol(dispatcher, OverflowMenuCustomizationCommands);
    mediator.pageInfoHandler = HandlerForProtocol(dispatcher, PageInfoCommands);
    mediator.popupMenuHandler =
        HandlerForProtocol(dispatcher, PopupMenuCommands);
    mediator.priceNotificationHandler =
        HandlerForProtocol(dispatcher, PriceNotificationsCommands);
    mediator.textZoomHandler = HandlerForProtocol(dispatcher, TextZoomCommands);
    mediator.quickDeleteHandler =
        HandlerForProtocol(dispatcher, QuickDeleteCommands);
    mediator.whatsNewHandler = HandlerForProtocol(dispatcher, WhatsNewCommands);

    mediator.webStateList = browser->GetWebStateList();
    mediator.navigationAgent = WebNavigationBrowserAgent::FromBrowser(browser);
    mediator.baseViewController = self.baseViewController;
    mediator.bookmarkModel = ios::BookmarkModelFactory::GetForProfile(profile);
    mediator.readingListModel =
        ReadingListModelFactory::GetInstance()->GetForProfile(profile);
    mediator.profilePrefs = profile->GetPrefs();
    mediator.engagementTracker = tracker;
    mediator.webContentAreaOverlayPresenter = overlayPresenter;
    mediator.browserPolicyConnector =
        GetApplicationContext()->GetBrowserPolicyConnector();
    mediator.syncService = SyncServiceFactory::GetForProfile(profile);
    mediator.templateURLService =
        ios::TemplateURLServiceFactory::GetForProfile(profile);
    mediator.promosManager = PromosManagerFactory::GetForProfile(profile);
    mediator.readingListBrowserAgent =
        ReadingListBrowserAgent::FromBrowser(browser);
    if (IsWebChannelsEnabled()) {
      mediator.followBrowserAgent = FollowBrowserAgent::FromBrowser(browser);
    }
    // Set the AuthenticationService with the one from the original
    // ProfileIOS as the incognito one doesn't have that service.
    mediator.authenticationService =
        AuthenticationServiceFactory::GetForProfile(
            profile->GetOriginalProfile());
    mediator.tabBasedIPHBrowserAgent =
        TabBasedIPHBrowserAgent::FromBrowser(browser);
    mediator.hasSettingsBlueDot =
        [self.popupMenuHelpCoordinator hasBlueDotForOverflowMenu];
    self.contentBlockerMediator.consumer = mediator;

    NSInteger highlightDestination =
        [self.popupMenuHelpCoordinator highlightDestination] == nil
            ? -1
            : [[self.popupMenuHelpCoordinator highlightDestination]
                  integerValue];

    UITraitCollection* traits = self.baseViewController.traitCollection;
    OverflowMenuUIConfiguration* uiConfiguration =
        [[OverflowMenuUIConfiguration alloc]
            initWithPresentingViewControllerHorizontalSizeClass:
                traits.horizontalSizeClass
                      presentingViewControllerVerticalSizeClass:
                          traits.verticalSizeClass
                                           highlightDestination:
                                               highlightDestination];

    self.popupMenuHelpCoordinator.uiConfiguration = uiConfiguration;

    _overflowMenuModel = [[OverflowMenuModel alloc] initWithDestinations:@[]
                                                            actionGroups:@[]];

    _overflowMenuOrderer.model = _overflowMenuModel;
    mediator.model = _overflowMenuModel;
    self.popupMenuHelpCoordinator.actionProvider = mediator;

    self.overflowMenuMediator = mediator;

    UIViewController* menu =
        [OverflowMenuViewProvider makeViewControllerWithModel:_overflowMenuModel
                                              uiConfiguration:uiConfiguration
                                               metricsHandler:self
                                    customizationEventHandler:self];

    LayoutGuideCenter* layoutGuideCenter = LayoutGuideCenterForBrowser(browser);
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

    [self setupSheetForMenu:menu isCustomizationScreen:NO animated:NO];

    // Reset event before presenting.
    _event = OverflowMenuVisitedEvent();

    __weak __typeof(self) weakSelf = self;
    [self.baseViewController
        presentViewController:menu
                     animated:YES
                   completion:^{
                     [weakSelf.popupMenuHelpCoordinator
                         showIPHAfterOpenOfOverflowMenu:menu];
                   }];

    // Log to FET overflow menu opened if opened with blue dot.
    if (IsBlueDotOnToolsMenuButtoneEnabled() &&
        [self.popupMenuHelpCoordinator hasBlueDotForOverflowMenu] && tracker) {
      tracker->NotifyEvent(
          feature_engagement::events::kBlueDotPromoOverflowMenuOpened);
      [self updateToolsMenuBlueDotVisibility];
    }

    return;
  }

  self.mediator = [[PopupMenuMediator alloc]
         initWithIsIncognito:self.browser->GetProfile()->IsOffTheRecord()
            readingListModel:ReadingListModelFactory::GetForProfile(
                                 self.browser->GetProfile())
      browserPolicyConnector:GetApplicationContext()
                                 ->GetBrowserPolicyConnector()];
  self.mediator.engagementTracker = tracker;
  self.mediator.webStateList = self.browser->GetWebStateList();
  self.mediator.readingListBrowserAgent =
      ReadingListBrowserAgent::FromBrowser(self.browser);
  self.mediator.lensCommandsHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), LensCommands);
  self.mediator.bookmarkModel =
      ios::BookmarkModelFactory::GetForProfile(self.browser->GetProfile());
  self.mediator.prefService = self.browser->GetProfile()->GetPrefs();
  self.mediator.templateURLService =
      ios::TemplateURLServiceFactory::GetForProfile(self.browser->GetProfile());
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
  self.actionHandler.helpHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), HelpCommands);
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
    OverflowMenuVisitedEvent event;
    base::TimeDelta elapsed = base::Seconds(
        [NSDate timeIntervalSinceReferenceDate] - self.toolsMenuOpenTime);
    UMA_HISTOGRAM_MEDIUM_TIMES("IOS.OverflowMenu.TimeOpen", elapsed);
    if (self.toolsMenuUserTookAction && self.overflowMenuUserSelectedAction) {
      UMA_HISTOGRAM_MEDIUM_TIMES("IOS.OverflowMenu.TimeOpen.ActionChosen",
                                 elapsed);
    } else if (self.toolsMenuUserTookAction &&
               self.overflowMenuUserSelectedDestination) {
      UMA_HISTOGRAM_MEDIUM_TIMES("IOS.OverflowMenu.TimeOpen.DestinationChosen",
                                 elapsed);
    }

    event.PutOrRemove(OverflowMenuVisitedEventFields::kUserSelectedDestination,
                      self.overflowMenuUserSelectedDestination);
    event.PutOrRemove(OverflowMenuVisitedEventFields::kUserSelectedAction,
                      self.overflowMenuUserSelectedAction);

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

    RecordOverflowMenuVisitedEvent(_event);

    if (IsNewOverflowMenuEnabled() &&
        self.overflowMenuUserScrolledToEndOfActions) {
      base::UmaHistogramBoolean(
          "IOS.OverflowMenu.UserScrolledToEndAndStartedCustomization",
          _event.Has(
              OverflowMenuVisitedEventFields::kUserStartedCustomization));
    }

    _event = OverflowMenuVisitedEvent();

    self.toolsMenuWasScrolledVertically = NO;
    self.toolsMenuWasScrolledHorizontally = NO;
    self.toolsMenuUserTookAction = NO;
    self.overflowMenuUserSelectedAction = NO;
    self.overflowMenuUserSelectedDestination = NO;
    self.overflowMenuUserScrolledToEndOfActions = NO;
  }

  if (self.overflowMenuMediator) {
    [self.baseViewController dismissViewControllerAnimated:animated
                                                completion:nil];
    _overflowMenuModel = nil;
    [_overflowMenuOrderer updateForMenuDisappearance];
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

- (void)adjustPopupSize {
  if (self.overflowMenuMediator) {
    UIViewController* menu = self.baseViewController.presentedViewController;
    UIPopoverPresentationController* popoverPresentationController =
        menu.popoverPresentationController;

    LayoutGuideCenter* layoutGuideCenter =
        LayoutGuideCenterForBrowser(self.browser);
    UILayoutGuide* layoutGuide =
        [layoutGuideCenter makeLayoutGuideNamed:kToolsMenuGuide];
    [self.baseViewController.view addLayoutGuide:layoutGuide];

    // Re-anchor the popover if necessary, when the parent view's size changes.
    popoverPresentationController.sourceRect = layoutGuide.layoutFrame;
  }
}

- (void)updateToolsMenuBlueDotVisibility {
  [self.popupMenuHelpCoordinator updateBlueDotVisibility];
}

- (void)notifyIPHBubblePresenting {
  [self.popupMenuHelpCoordinator notifyIPHBubblePresenting];
}

- (BOOL)hasBlueDotForOverflowMenu {
  return [self.popupMenuHelpCoordinator hasBlueDotForOverflowMenu];
}

#pragma mark - OverflowMenuCustomizationCommands

- (void)showMenuCustomization {
  _event.Put(OverflowMenuVisitedEventFields::kUserStartedCustomization);
  [self logFeatureEngagementCustomizationStarted];
  [_overflowMenuModel
      startCustomizationWithActions:_overflowMenuOrderer
                                        .actionCustomizationModel
                       destinations:_overflowMenuOrderer
                                        .destinationCustomizationModel];

  [self setupSheetForMenu:self.baseViewController.presentedViewController
      isCustomizationScreen:YES
                   animated:YES];
}

- (void)showMenuCustomizationFromActionType:
    (overflow_menu::ActionType)actionType {
  for (OverflowMenuAction* action in _overflowMenuOrderer
           .actionCustomizationModel.actionsGroup.actions) {
    if (action.actionType == static_cast<NSInteger>(actionType)) {
      action.highlighted = YES;
    }
  }

  [self showMenuCustomization];
}

- (void)hideMenuCustomization {
  [self setupSheetForMenu:self.baseViewController.presentedViewController
      isCustomizationScreen:NO
                   animated:YES];

  [_overflowMenuModel endCustomization];
}

#pragma mark - MenuCustomizationEventHandler

- (void)doneWasTapped {
  if (_overflowMenuOrderer.destinationCustomizationModel.hasChanged) {
    _event.Put(OverflowMenuVisitedEventFields::kUserCustomizedDestinations);
  }
  if (_overflowMenuOrderer.actionCustomizationModel.hasChanged) {
    _event.Put(OverflowMenuVisitedEventFields::kUserCustomizedActions);
  }

  [_overflowMenuOrderer commitActionsUpdate];
  [_overflowMenuOrderer commitDestinationsUpdate];

  [self hideMenuCustomization];
}

- (void)cancelWasTapped {
  [_overflowMenuOrderer cancelActionsUpdate];
  [_overflowMenuOrderer cancelDestinationsUpdate];

  _event.Put(OverflowMenuVisitedEventFields::kUserCancelledCustomization);

  [self hideMenuCustomization];
}

#pragma mark - ContainedPresenterDelegate

- (void)containedPresenterDidPresent:(id<ContainedPresenter>)presenter {
  if (presenter != self.presenter)
    return;
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

- (BOOL)presentationControllerShouldDismiss:
    (UIPresentationController*)presentationController {
  return _overflowMenuModel.isCustomizationActive ? NO : YES;
}

#pragma mark - UISheetPresentationControllerDelegate

- (void)sheetPresentationControllerDidChangeSelectedDetentIdentifier:
    (UISheetPresentationController*)sheetPresentationController {
  [self popupMenuScrolledVertically];
}

#pragma mark - PopupMenuMetricsHandler

- (void)popupMenuScrolledVertically {
  self.toolsMenuWasScrolledVertically = YES;
  _event.Put(OverflowMenuVisitedEventFields::kUserScrolledVertically);
}

- (void)popupMenuScrolledHorizontally {
  self.toolsMenuWasScrolledHorizontally = YES;
  _event.Put(OverflowMenuVisitedEventFields::kUserScrolledHorizontally);
}

- (void)popupMenuTookAction {
  self.toolsMenuUserTookAction = YES;
}

- (void)popupMenuUserSelectedAction {
  self.overflowMenuUserSelectedAction = YES;
  _event.Put(OverflowMenuVisitedEventFields::kUserSelectedAction);
}

- (void)popupMenuUserSelectedDestination {
  self.overflowMenuUserSelectedDestination = YES;
  _event.Put(OverflowMenuVisitedEventFields::kUserSelectedDestination);
}

- (void)popupMenuUserScrolledToEndOfActions {
  self.overflowMenuUserScrolledToEndOfActions = YES;
}

#pragma mark - Notification callback

- (void)applicationDidEnterBackground:(NSNotification*)note {
  [self dismissPopupMenuAnimated:NO];
}

#pragma mark - Private

- (void)trackToolsMenuNoHorizontalScrollOrAction {
  ProfileIOS* profile = self.browser->GetProfile();
  if (!profile) {
    return;
  }
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForProfile(profile);
  if (!tracker) {
    return;
  }

  tracker->NotifyEvent(
      feature_engagement::events::kOverflowMenuNoHorizontalScrollOrAction);
}

- (void)logFeatureEngagementCustomizationStarted {
  ProfileIOS* profile = self.browser->GetProfile();
  if (!profile) {
    return;
  }

  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForProfile(profile);
  if (!tracker) {
    return;
  }

  tracker->NotifyEvent(
      feature_engagement::events::kIOSOverflowMenuCustomizationUsed);
}

- (void)setupSheetForMenu:(UIViewController*)menu
    isCustomizationScreen:(BOOL)isCustomizationScreen
                 animated:(BOOL)animated {
  // The adaptive controller adjusts styles based on window size: sheet
  // for slim windows on iPhone and iPad, popover for larger windows on
  // iPad.
  UISheetPresentationController* sheetPresentationController =
      menu.popoverPresentationController.adaptiveSheetPresentationController;
  if (!sheetPresentationController) {
    return;
  }

  sheetPresentationController.delegate = self;

  void (^changes)(void) = ^{
    sheetPresentationController.prefersEdgeAttachedInCompactHeight = YES;
    sheetPresentationController
        .widthFollowsPreferredContentSizeWhenEdgeAttached = YES;

    if (isCustomizationScreen) {
      sheetPresentationController.prefersGrabberVisible = NO;
      sheetPresentationController.detents =
          @[ [UISheetPresentationControllerDetent largeDetent] ];
    } else {
      sheetPresentationController.prefersGrabberVisible = YES;

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
  };

  if (animated) {
    [sheetPresentationController animateChanges:changes];
  } else {
    changes();
  }
}

@end
