// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui_bundled/adaptive_toolbar_coordinator.h"

#import "base/apple/foundation_util.h"
#import "components/saved_tab_groups/public/tab_group_sync_service.h"
#import "components/saved_tab_groups/public/versioning_message_controller.h"
#import "components/strings/grit/components_strings.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "ios/chrome/browser/bubble/model/tab_based_iph_browser_agent.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_view_controller_presenter.h"
#import "ios/chrome/browser/collaboration/model/messaging/messaging_backend_service_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_ui_updater.h"
#import "ios/chrome/browser/menu/ui_bundled/browser_action_factory.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_web_state_utils.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_commands.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/toolbar/ui_bundled/adaptive_toolbar_coordinator+subclassing.h"
#import "ios/chrome/browser/toolbar/ui_bundled/adaptive_toolbar_mediator.h"
#import "ios/chrome/browser/toolbar/ui_bundled/adaptive_toolbar_view_controller.h"
#import "ios/chrome/browser/toolbar/ui_bundled/adaptive_toolbar_view_controller_delegate.h"
#import "ios/chrome/browser/toolbar/ui_bundled/buttons/toolbar_button.h"
#import "ios/chrome/browser/toolbar/ui_bundled/buttons/toolbar_button_actions_handler.h"
#import "ios/chrome/browser/toolbar/ui_bundled/buttons/toolbar_button_factory.h"
#import "ios/chrome/browser/toolbar/ui_bundled/buttons/toolbar_button_visibility_configuration.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/web/model/web_navigation_browser_agent.h"
#import "services/metrics/public/cpp/ukm_builders.h"
#import "ui/base/l10n/l10n_util.h"

using tab_groups::TabGroupSyncServiceFactory;
using tab_groups::VersioningMessageController;

@interface AdaptiveToolbarCoordinator () <AdaptiveToolbarViewControllerDelegate>

// Whether this coordinator has been started.
@property(nonatomic, assign) BOOL started;
// Mediator for updating the toolbar when the WebState changes.
@property(nonatomic, strong) AdaptiveToolbarMediator* mediator;
// Actions handler for the toolbar buttons.
@property(nonatomic, strong) ToolbarButtonActionsHandler* actionHandler;

@end

@implementation AdaptiveToolbarCoordinator {
  // Observer that updates `toolbarViewController` for fullscreen events.
  std::unique_ptr<FullscreenUIUpdater> _fullscreenUIUpdater;
}

@synthesize baseViewController = _baseViewController;

#pragma mark - ChromeCoordinator

- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);
  return [super initWithBaseViewController:nil browser:browser];
}

- (void)start {
  if (_started) {
    return;
  }
  Browser* browser = self.browser;

  _started = YES;

  self.viewController.overrideUserInterfaceStyle =
      browser->GetProfile()->IsOffTheRecord() ? UIUserInterfaceStyleDark
                                              : UIUserInterfaceStyleUnspecified;
  self.viewController.layoutGuideCenter = LayoutGuideCenterForBrowser(browser);
  self.viewController.adaptiveDelegate = self;

  self.mediator = [[AdaptiveToolbarMediator alloc]
      initWithMessagingService:collaboration::messaging::
                                   MessagingBackendServiceFactory::
                                       GetForProfile(browser->GetProfile())];
  self.mediator.incognito = browser->GetProfile()->IsOffTheRecord();
  self.mediator.consumer = self.viewController;
  self.mediator.navigationBrowserAgent =
      WebNavigationBrowserAgent::FromBrowser(browser);
  self.mediator.webStateList = browser->GetWebStateList();
  self.mediator.webContentAreaOverlayPresenter =
      OverlayPresenter::FromBrowser(browser, OverlayModality::kWebContentArea);
  self.mediator.templateURLService =
      ios::TemplateURLServiceFactory::GetForProfile(browser->GetProfile());
  self.mediator.actionFactory = [[BrowserActionFactory alloc]
      initWithBrowser:browser
             scenario:kMenuScenarioHistogramToolbarMenu];
  self.mediator.commandDispatcher = browser->GetCommandDispatcher();

  _fullscreenUIUpdater = std::make_unique<FullscreenUIUpdater>(
      FullscreenController::FromBrowser(browser), self.viewController);

  self.viewController.menuProvider = self.mediator;

  if ([self hasTabGridButton]) {
    [self displayAppUpdatedIPHIfNeeded];
  }
}

- (void)stop {
  [super stop];
  [self.mediator disconnect];
  self.mediator = nil;
  _fullscreenUIUpdater = nullptr;
  _started = NO;
}

#pragma mark - Public

- (void)setLocationBarViewController:
    (UIViewController*)locationBarViewController {
  self.viewController.locationBarViewController = locationBarViewController;
}

- (void)updateToolbarForSideSwipeSnapshot:(web::WebState*)webState {
  BOOL isNonIncognitoNTP =
      !self.isOffTheRecord && IsVisibleURLNewTabPage(webState);

  [self.mediator updateConsumerForWebState:webState];
  [self.viewController updateForSideSwipeSnapshot:isNonIncognitoNTP];
}

- (void)resetToolbarAfterSideSwipeSnapshot {
  [self.mediator updateConsumerForWebState:self.browser->GetWebStateList()
                                               ->GetActiveWebState()];
  [self.viewController resetAfterSideSwipeSnapshot];
}

- (void)showPrerenderingAnimation {
  [self.viewController showPrerenderingAnimation];
}

- (void)setLocationBarHeight:(CGFloat)height {
  [self.viewController setLocationBarHeight:height];
}

#pragma mark - AdaptiveToolbarViewControllerDelegate

- (void)exitFullscreen:(FullscreenExitReason)FullscreenExitReason {
  FullscreenController* fullscreenController =
      FullscreenController::FromBrowser(self.browser);
  fullscreenController->ExitFullscreen(FullscreenExitReason);

  web::WebState* webState =
      self.browser->GetWebStateList()->GetActiveWebState();
  ukm::SourceId sourceID = ukm::GetSourceIdForWebStateDocument(webState);
  if (sourceID != ukm::kInvalidSourceId) {
    ukm::builders::IOS_FullscreenActions(sourceID)
        .SetHasExitedManually(true)
        .Record(ukm::UkmRecorder::Get());
  }
}

- (BOOL)isReaderModeActive {
  web::WebState* webState =
      self.browser->GetWebStateList()->GetActiveWebState();
  return IsReaderModeActiveInWebState(webState);
}

#pragma mark - NewTabPageControllerDelegate

- (void)setScrollProgressForTabletOmnibox:(CGFloat)progress {
  [self.viewController setScrollProgressForTabletOmnibox:progress];
}

- (UIResponder<UITextInput>*)fakeboxScribbleForwardingTarget {
  // Implemented in `ToolbarCoordinator`.
  return nil;
}

- (void)didNavigateToNTPOnActiveWebState {
  // Implemented in `ToolbarCoordinator`.
}

#pragma mark - ToolbarCommands

- (void)triggerToolbarSlideInAnimation {
  // Implemented in primary and secondary toolbars directly.
}

- (void)indicateLensOverlayVisible:(BOOL)lensOverlayVisible {
  // NO-OP
}

#pragma mark - ToolbarCoordinatee

- (id<PopupMenuUIUpdating>)popupMenuUIUpdater {
  return self.viewController;
}

#pragma mark - Protected

- (ToolbarButtonFactory*)buttonFactoryWithType:(ToolbarType)type {
  BOOL isIncognito = self.isOffTheRecord;
  ToolbarStyle style =
      isIncognito ? ToolbarStyle::kIncognito : ToolbarStyle::kNormal;

  ToolbarButtonActionsHandler* actionHandler =
      [[ToolbarButtonActionsHandler alloc] init];

  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();

  actionHandler.applicationHandler =
      HandlerForProtocol(dispatcher, ApplicationCommands);
  actionHandler.activityHandler =
      HandlerForProtocol(dispatcher, ActivityServiceCommands);
  actionHandler.menuHandler = HandlerForProtocol(dispatcher, PopupMenuCommands);
  actionHandler.omniboxHandler =
      HandlerForProtocol(dispatcher, OmniboxCommands);

  actionHandler.incognito = isIncognito;
  actionHandler.navigationAgent =
      WebNavigationBrowserAgent::FromBrowser(self.browser);
  actionHandler.tabBasedIPHAgent =
      TabBasedIPHBrowserAgent::FromBrowser(self.browser);

  self.actionHandler = actionHandler;

  ToolbarButtonFactory* buttonFactory =
      [[ToolbarButtonFactory alloc] initWithStyle:style];
  buttonFactory.actionHandler = actionHandler;
  buttonFactory.visibilityConfiguration =
      [[ToolbarButtonVisibilityConfiguration alloc] initWithType:type];

  return buttonFactory;
}

- (BOOL)hasTabGridButton {
  // Implemented in primary and secondary toolbars directly.
  return NO;
}

- (BOOL)shouldPointArrowDownForTabGridIPH {
  return YES;
}

#pragma mark - Private

// Returns the versioning message controller, if any.
- (VersioningMessageController*)versioningMessageController {
  CHECK(self.browser);
  auto* tabGroupSyncService =
      TabGroupSyncServiceFactory::GetForProfile(self.browser->GetProfile());
  if (!tabGroupSyncService) {
    return nullptr;
  }
  return tabGroupSyncService->GetVersioningMessageController();
}

// Called to check whether to show the app-updated IPH and show it if needed.
- (void)displayAppUpdatedIPHIfNeeded {
  CHECK([self hasTabGridButton]);

  auto* versioningMessageController = [self versioningMessageController];
  if (!versioningMessageController) {
    return;
  }

  __weak __typeof(self) weakSelf = self;
  versioningMessageController->ShouldShowMessageUiAsync(
      VersioningMessageController::MessageType::VERSION_UPDATED_MESSAGE,
      base::BindOnce(
          [](AdaptiveToolbarCoordinator* coordinator, bool shouldShow) {
            if (shouldShow) {
              [coordinator displayAppUpdatedIPH];
            }
          },
          weakSelf));
}

// Called when the app-updated IPH should actually be shown.
- (void)displayAppUpdatedIPH {
  CHECK([self hasTabGridButton]);

  BOOL shouldPointArrowDown = [self shouldPointArrowDownForTabGridIPH];

  // Present the IPH.
  NSString* IPHTitle = l10n_util::GetNSString(
      IDS_COLLABORATION_SHARED_TAB_GROUPS_AVAILABLE_AGAIN_IPH_MESSAGE);
  BubbleViewControllerPresenter* presenter =
      [[BubbleViewControllerPresenter alloc]
               initWithText:IPHTitle
                      title:nil
             arrowDirection:shouldPointArrowDown ? BubbleArrowDirectionDown
                                                 : BubbleArrowDirectionUp
                  alignment:BubbleAlignmentBottomOrTrailing
                 bubbleType:BubbleViewTypeDefault
            pageControlPage:BubblePageControlPageNone
          dismissalCallback:nil];
  presenter.voiceOverAnnouncement = IPHTitle;

  UIView* button = [self.viewController tabGridButton];
  CGRect frameInWindow = [button convertRect:button.bounds toView:nil];
  CGPoint anchorPoint =
      CGPointMake(CGRectGetMidX(frameInWindow),
                  shouldPointArrowDown ? CGRectGetMinY(frameInWindow)
                                       : CGRectGetMaxY(frameInWindow));
  if (![presenter canPresentInView:self.baseViewController.view
                       anchorPoint:anchorPoint]) {
    return;
  }

  [presenter presentInViewController:self.baseViewController
                         anchorPoint:anchorPoint];

  // Notify that the message has been displayed.
  auto* versioningMessageController = [self versioningMessageController];
  CHECK(versioningMessageController);
  versioningMessageController->OnMessageUiShown(
      VersioningMessageController::MessageType::VERSION_UPDATED_MESSAGE);
}

@end
