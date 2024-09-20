// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_view/ui_bundled/browser_view_controller.h"

#import "base/apple/bundle_locations.h"
#import "base/apple/foundation_util.h"
#import "base/ios/ios_util.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "components/enterprise/idle/idle_pref_names.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/strings/grit/components_strings.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/home/bookmarks_coordinator.h"
#import "ios/chrome/browser/browser_view/ui_bundled/browser_view_controller+private.h"
#import "ios/chrome/browser/browser_view/ui_bundled/browser_view_visibility_consumer.h"
#import "ios/chrome/browser/browser_view/ui_bundled/key_commands_provider.h"
#import "ios/chrome/browser/browser_view/ui_bundled/safe_area_provider.h"
#import "ios/chrome/browser/crash_report/model/crash_keys_helper.h"
#import "ios/chrome/browser/default_promo/ui_bundled/default_promo_non_modal_presentation_delegate.h"
#import "ios/chrome/browser/discover_feed/model/feed_constants.h"
#import "ios/chrome/browser/find_in_page/model/util.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_util.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_view.h"
#import "ios/chrome/browser/intents/intents_donation_helper.h"
#import "ios/chrome/browser/metrics/model/tab_usage_recorder_browser_agent.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_coordinator.h"
#import "ios/chrome/browser/reading_list/model/reading_list_browser_agent.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/find_in_page_commands.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/reading_list_add_command.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/text_zoom_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/features_utils.h"
#import "ios/chrome/browser/shared/ui/util/named_guide.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/url_with_title.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_mediator.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/swipe_view.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/tabs/ui_bundled/background_tab_animation_view.h"
#import "ios/chrome/browser/tabs/ui_bundled/foreground_tab_animation_view.h"
#import "ios/chrome/browser/tabs/ui_bundled/requirements/tab_strip_presentation.h"
#import "ios/chrome/browser/tabs/ui_bundled/switch_to_tab_animation_view.h"
#import "ios/chrome/browser/tabs/ui_bundled/tab_strip_constants.h"
#import "ios/chrome/browser/tabs/ui_bundled/tab_strip_legacy_coordinator.h"
#import "ios/chrome/browser/ui/authentication/re_signin_infobar_delegate.h"
#import "ios/chrome/browser/ui/browser_container/browser_container_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_animator.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_element.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_updater.h"
#import "ios/chrome/browser/ui/main_content/main_content_ui.h"
#import "ios/chrome/browser/ui/main_content/main_content_ui_broadcasting_util.h"
#import "ios/chrome/browser/ui/main_content/main_content_ui_state.h"
#import "ios/chrome/browser/ui/main_content/web_scroll_view_main_content_ui_forwarder.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/feature_flags.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_coordinator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/coordinator/tab_strip_coordinator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/tab_strip_utils.h"
#import "ios/chrome/browser/ui/toolbar/accessory/toolbar_accessory_presenter.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/fullscreen/toolbar_ui.h"
#import "ios/chrome/browser/ui/toolbar/fullscreen/toolbar_ui_broadcasting_util.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_coordinator.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/web/model/page_placeholder_browser_agent.h"
#import "ios/chrome/browser/web/model/page_placeholder_tab_helper.h"
#import "ios/chrome/browser/web/model/web_navigation_browser_agent.h"
#import "ios/chrome/browser/web/model/web_navigation_util.h"
#import "ios/chrome/browser/web_state_list/model/web_usage_enabler/web_usage_enabler_browser_agent.h"
#import "ios/chrome/browser/webui/model/show_mail_composer_context.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/fullscreen/fullscreen_api.h"
#import "ios/public/provider/chrome/browser/voice_search/voice_search_controller.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "net/base/apple/url_conversions.h"
#import "services/metrics/public/cpp/ukm_builders.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// When the tab strip moves beyond this origin offset, switch the status bar
// appearance from light to dark.
const CGFloat kTabStripAppearanceOffset = -29;

enum HeaderBehaviour {
  // The header moves completely out of the screen.
  Hideable = 0,
  // This header stay on screen and covers part of the content.
  Overlap
};
}  // namespace

#pragma mark - HeaderDefinition helper

// Class used to define a header, an object displayed at the top of the browser.
@interface HeaderDefinition : NSObject

// The header view.
@property(nonatomic, strong) UIView* view;
// How to place the view, and its behaviour when the headers move.
@property(nonatomic, assign) HeaderBehaviour behaviour;

- (instancetype)initWithView:(UIView*)view
             headerBehaviour:(HeaderBehaviour)behaviour;

+ (instancetype)definitionWithView:(UIView*)view
                   headerBehaviour:(HeaderBehaviour)behaviour;

@end

@implementation HeaderDefinition
@synthesize view = _view;
@synthesize behaviour = _behaviour;

+ (instancetype)definitionWithView:(UIView*)view
                   headerBehaviour:(HeaderBehaviour)behaviour {
  return [[self alloc] initWithView:view headerBehaviour:behaviour];
}

- (instancetype)initWithView:(UIView*)view
             headerBehaviour:(HeaderBehaviour)behaviour {
  self = [super init];
  if (self) {
    _view = view;
    _behaviour = behaviour;
  }
  return self;
}

@end

#pragma mark - BVC

// Note other delegates defined in the Delegates category header.
@interface BrowserViewController () <FullscreenUIElement,
                                     MainContentUI,
                                     SideSwipeMediatorDelegate,
                                     TabStripPresentation,
                                     UIGestureRecognizerDelegate> {
  // Identifier for each animation of an NTP opening.
  NSInteger _NTPAnimationIdentifier;

  // Mediator for edge swipe gestures for page and tab navigation.
  SideSwipeMediator* _sideSwipeMediator;

  // Keyboard commands provider.  It offloads most of the keyboard commands
  // management off of the BVC.
  KeyCommandsProvider* _keyCommandsProvider;

  // Used to display the Voice Search UI.  Nil if not visible.
  id<VoiceSearchController> _voiceSearchController;

  // YES if Voice Search should be started when the new tab animation is
  // finished.
  BOOL _startVoiceSearchAfterNewTabAnimation;

  // Whether or not -shutdown has been called.
  BOOL _isShutdown;

  // Whether or not Incognito* is enabled.
  BOOL _isOffTheRecord;

  // The Browser's WebStateList.
  base::WeakPtr<WebStateList> _webStateList;

  // Whether the current content is incognito and requires biometric
  // authentication from the user before it can be accessed.
  BOOL _itemsRequireAuthentication;

  // The last point within `contentArea` that's received a touch.
  CGPoint _lastTapPoint;

  // The time at which `_lastTapPoint` was most recently set.
  CFTimeInterval _lastTapTime;

  // The coordinator that shows the bookmarking UI after the user taps the star
  // button.
  BookmarksCoordinator* _bookmarksCoordinator;

  // Toolbar state that broadcasts changes to min and max heights.
  ToolbarUIState* _toolbarUIState;

  // The main content UI updater for the content displayed by this BVC.
  MainContentUIStateUpdater* _mainContentUIUpdater;

  // The forwarder for web scroll view interation events.
  WebScrollViewMainContentUIForwarder* _webMainContentUIForwarder;

  // The updater that adjusts the toolbar's layout for fullscreen events.
  std::unique_ptr<FullscreenUIUpdater> _fullscreenUIUpdater;

  // Fake status bar view used to blend the toolbar into the status bar.
  UIView* _fakeStatusBarView;

  // The service used to load url parameters in current or new tab.
  raw_ptr<UrlLoadingBrowserAgent> _urlLoadingBrowserAgent;

  // Used to report usage of a single Browser's tab.
  raw_ptr<TabUsageRecorderBrowserAgent> _tabUsageRecorderBrowserAgent;

  // Used to get the layout guide center.
  LayoutGuideCenter* _layoutGuideCenter;

  // Used to add or cancel a page placeholder for next navigation.
  raw_ptr<PagePlaceholderBrowserAgent> _pagePlaceholderBrowserAgent;
}

// Activates/deactivates the object. This will enable/disable the ability for
// this object to browse, and to have live UIWebViews associated with it. While
// not active, the UI will not react to changes in the active web state, so
// generally an inactive BVC should not be visible.
@property(nonatomic, assign, getter=isActive) BOOL active;
// Browser container view controller.
@property(nonatomic, strong)
    BrowserContainerViewController* browserContainerViewController;
// Invisible button used to dismiss the keyboard.
@property(nonatomic, strong) UIButton* typingShield;
// Whether the controller's view is currently available.
// YES from viewWillAppear to viewWillDisappear.
@property(nonatomic, assign, getter=isVisible) BOOL visible;
// Whether the controller's view is currently visible.
// YES from viewDidAppear to viewWillDisappear.
@property(nonatomic, assign) BOOL viewVisible;
// Whether the controller should broadcast its UI.
@property(nonatomic, assign, getter=isBroadcasting) BOOL broadcasting;
// A view to obscure incognito content when the user isn't authorized to
// see it.
@property(nonatomic, strong) IncognitoReauthView* blockingView;
// Whether the controller is currently dismissing a presented view controller.
@property(nonatomic, assign, getter=isDismissingModal) BOOL dismissingModal;
// Whether a new tab animation is occurring.
@property(nonatomic, assign, getter=isInNewTabAnimation) BOOL inNewTabAnimation;
// Whether BVC prefers to hide the status bar. This value is used to determine
// the response from the `prefersStatusBarHidden` method.
@property(nonatomic, assign) BOOL hideStatusBar;
// A block to be run when the `tabWasAdded:` method completes the animation
// for the presentation of a new tab. Can be used to record performance metrics.
@property(nonatomic, strong, nullable)
    ProceduralBlock foregroundTabWasAddedCompletionBlock;
// Coordinator for tablet tab strip.
@property(nonatomic, strong)
    TabStripLegacyCoordinator* legacyTabStripCoordinator;
// Coordinator for the new tablet tab strip.
@property(nonatomic, strong) TabStripCoordinator* tabStripCoordinator;
// A weak reference to the view of the tab strip on tablet.
@property(nonatomic, weak) UIView* tabStripView;

// Returns the header views, all the chrome on top of the page, including the
// ones that cannot be scrolled off screen by full screen.
@property(nonatomic, strong, readonly) NSArray<HeaderDefinition*>* headerViews;

// Coordinator for the popup menus.
@property(nonatomic, strong) PopupMenuCoordinator* popupMenuCoordinator;

@property(nonatomic, strong) BubblePresenter* bubblePresenter;

// Presenter used to display accessories over the toolbar (e.g. Find In Page).
@property(nonatomic, strong)
    ToolbarAccessoryPresenter* toolbarAccessoryPresenter;

// Command handler for text zoom commands.
@property(nonatomic, weak) id<TextZoomCommands> textZoomHandler;

// Command handler for help commands.
@property(nonatomic, weak) id<HelpCommands> helpHandler;

// Command handler for popup menu commands.
@property(nonatomic, weak) id<PopupMenuCommands> popupMenuCommandsHandler;

// Command handler for application commands.
@property(nonatomic, weak) id<ApplicationCommands> applicationCommandsHandler;

// Command handler for find in page commands.
@property(nonatomic, weak) id<FindInPageCommands> findInPageCommandsHandler;

// The FullscreenController.
@property(nonatomic, assign) FullscreenController* fullscreenController;

// Coordinator of primary and secondary toolbars.
@property(nonatomic, strong) ToolbarCoordinator* toolbarCoordinator;

// Vertical offset for the primary toolbar, used for fullscreen.
@property(nonatomic, strong) NSLayoutConstraint* primaryToolbarOffsetConstraint;
// Height constraint for the primary toolbar.
@property(nonatomic, strong) NSLayoutConstraint* primaryToolbarHeightConstraint;
// Height constraint for the secondary toolbar.
@property(nonatomic, strong)
    NSLayoutConstraint* secondaryToolbarHeightConstraint;
// Current Fullscreen progress for the footers.
@property(nonatomic, assign) CGFloat footerFullscreenProgress;
// Y-dimension offset for placement of the header.
@property(nonatomic, readonly) CGFloat headerOffset;
// Height of the header view.
@property(nonatomic, readonly) CGFloat headerHeight;

// The webState of the active tab.
@property(nonatomic, readonly) web::WebState* currentWebState;

// A gesture recognizer to track the last tapped window and the coordinates of
// the last tap.
@property(nonatomic, strong) UIGestureRecognizer* contentAreaGestureRecognizer;

// The coordinator for all NTPs in the BVC. Only used if kSingleNtp is enabled.
@property(nonatomic, strong) NewTabPageCoordinator* ntpCoordinator;

// Provider used to offload SceneStateBrowserAgent usage from BVC.
@property(nonatomic, strong) SafeAreaProvider* safeAreaProvider;

@end

@implementation BrowserViewController

#pragma mark - Object lifecycle

- (instancetype)
    initWithBrowserContainerViewController:
        (BrowserContainerViewController*)browserContainerViewController
                       keyCommandsProvider:
                           (KeyCommandsProvider*)keyCommandsProvider
                              dependencies:(BrowserViewControllerDependencies)
                                               dependencies {
  self = [super initWithNibName:nil bundle:base::apple::FrameworkBundle()];
  if (self) {
    _browserContainerViewController = browserContainerViewController;
    _keyCommandsProvider = keyCommandsProvider;
    _sideSwipeMediator = dependencies.sideSwipeMediator;
    [_sideSwipeMediator setSwipeDelegate:self];
    _bookmarksCoordinator = dependencies.bookmarksCoordinator;
    self.toolbarAccessoryPresenter = dependencies.toolbarAccessoryPresenter;
    self.ntpCoordinator = dependencies.ntpCoordinator;
    self.popupMenuCoordinator = dependencies.popupMenuCoordinator;
    self.toolbarCoordinator = dependencies.toolbarCoordinator;
    self.tabStripCoordinator = dependencies.tabStripCoordinator;
    self.legacyTabStripCoordinator = dependencies.legacyTabStripCoordinator;

    self.textZoomHandler = dependencies.textZoomHandler;
    self.helpHandler = dependencies.helpHandler;
    self.popupMenuCommandsHandler = dependencies.popupMenuCommandsHandler;
    self.applicationCommandsHandler = dependencies.applicationCommandsHandler;
    self.findInPageCommandsHandler = dependencies.findInPageCommandsHandler;
    _isOffTheRecord = dependencies.isOffTheRecord;
    _urlLoadingBrowserAgent = dependencies.urlLoadingBrowserAgent;
    _tabUsageRecorderBrowserAgent = dependencies.tabUsageRecorderBrowserAgent;
    _layoutGuideCenter = dependencies.layoutGuideCenter;
    _webStateList = dependencies.webStateList;
    _voiceSearchController = dependencies.voiceSearchController;
    self.safeAreaProvider = dependencies.safeAreaProvider;
    _pagePlaceholderBrowserAgent = dependencies.pagePlaceholderBrowserAgent;

    self.inNewTabAnimation = NO;
    self.fullscreenController = dependencies.fullscreenController;
    _footerFullscreenProgress = 1.0;

    // When starting the browser with an open tab, it is necessary to reset the
    // clipsToBounds property of the WKWebView so the page can bleed behind the
    // toolbar.
    if (self.currentWebState) {
      self.currentWebState->GetWebViewProxy().scrollViewProxy.clipsToBounds =
          NO;
    }
  }
  return self;
}

- (void)dealloc {
  DCHECK(_isShutdown) << "-shutdown must be called before dealloc.";
}

#pragma mark - Public Properties

- (UIView*)contentArea {
  return self.browserContainerViewController.view;
}

- (void)setInfobarBannerOverlayContainerViewController:
    (UIViewController*)infobarBannerOverlayContainerViewController {
  if (_infobarBannerOverlayContainerViewController ==
      infobarBannerOverlayContainerViewController) {
    return;
  }

  _infobarBannerOverlayContainerViewController =
      infobarBannerOverlayContainerViewController;
  if (!_infobarBannerOverlayContainerViewController)
    return;

  DCHECK_EQ(_infobarBannerOverlayContainerViewController.parentViewController,
            self);
  DCHECK_EQ(_infobarBannerOverlayContainerViewController.view.superview,
            self.view);
  [self updateOverlayContainerOrder];
}

- (void)setInfobarModalOverlayContainerViewController:
    (UIViewController*)infobarModalOverlayContainerViewController {
  if (_infobarModalOverlayContainerViewController ==
      infobarModalOverlayContainerViewController) {
    return;
  }

  _infobarModalOverlayContainerViewController =
      infobarModalOverlayContainerViewController;
  if (!_infobarModalOverlayContainerViewController)
    return;

  DCHECK_EQ(_infobarModalOverlayContainerViewController.parentViewController,
            self);
  DCHECK_EQ(_infobarModalOverlayContainerViewController.view.superview,
            self.view);
  [self updateOverlayContainerOrder];
}

#pragma mark - Private Properties

- (void)setVisible:(BOOL)visible {
  if (_visible == visible)
    return;

  _visible = visible;
}

- (void)setViewVisible:(BOOL)viewVisible {
  if (_viewVisible == viewVisible)
    return;
  _viewVisible = viewVisible;
  self.visible = viewVisible;
  [self.browserViewVisibilityConsumer browserViewDidChangeVisibility];
  [self updateBroadcastState];
}

- (void)setBroadcasting:(BOOL)broadcasting {
  if (_broadcasting == broadcasting)
    return;
  _broadcasting = broadcasting;

  ChromeBroadcaster* broadcaster = self.fullscreenController->broadcaster();
  if (_broadcasting) {
    _toolbarUIState = [[ToolbarUIState alloc] init];
    // Must update _toolbarUIState with current toolbar height state before
    // starting broadcasting.
    [self updateToolbarState];
    StartBroadcastingToolbarUI(_toolbarUIState, broadcaster);

    _mainContentUIUpdater = [[MainContentUIStateUpdater alloc]
        initWithState:[[MainContentUIState alloc] init]];
    _webMainContentUIForwarder = [[WebScrollViewMainContentUIForwarder alloc]
        initWithUpdater:_mainContentUIUpdater
           webStateList:self.webStateList];
    StartBroadcastingMainContentUI(self, broadcaster);

    _fullscreenUIUpdater =
        std::make_unique<FullscreenUIUpdater>(self.fullscreenController, self);
    [self updateForFullscreenProgress:self.fullscreenController->GetProgress()];
  } else {
    StopBroadcastingToolbarUI(broadcaster);
    StopBroadcastingMainContentUI(broadcaster);
    _mainContentUIUpdater = nil;
    _toolbarUIState = nil;
    [_webMainContentUIForwarder disconnect];
    _webMainContentUIForwarder = nil;

    _fullscreenUIUpdater = nullptr;
  }
}

- (void)setInNewTabAnimation:(BOOL)inNewTabAnimation {
  if (_inNewTabAnimation == inNewTabAnimation) {
    return;
  }
  _inNewTabAnimation = inNewTabAnimation;
  [self updateBroadcastState];
}

- (void)setHideStatusBar:(BOOL)hideStatusBar {
  if (_hideStatusBar == hideStatusBar)
    return;
  _hideStatusBar = hideStatusBar;
  [self setNeedsStatusBarAppearanceUpdate];
}

- (NSArray<HeaderDefinition*>*)headerViews {
  NSMutableArray<HeaderDefinition*>* results = [[NSMutableArray alloc] init];
  if (![self isViewLoaded])
    return results;

  if (!IsRegularXRegularSizeClass(self)) {
    if (self.toolbarCoordinator.primaryToolbarViewController.view) {
      [results
          addObject:[HeaderDefinition
                        definitionWithView:self.toolbarCoordinator
                                               .primaryToolbarViewController
                                               .view
                           headerBehaviour:Hideable]];
    }
  } else {
    if (self.tabStripView) {
      [results addObject:[HeaderDefinition definitionWithView:self.tabStripView
                                              headerBehaviour:Hideable]];
    }
    if (self.toolbarCoordinator.primaryToolbarViewController.view) {
      [results
          addObject:[HeaderDefinition
                        definitionWithView:self.toolbarCoordinator
                                               .primaryToolbarViewController
                                               .view
                           headerBehaviour:Hideable]];
    }
    if (self.toolbarAccessoryPresenter.isPresenting) {
      [results addObject:[HeaderDefinition
                             definitionWithView:self.toolbarAccessoryPresenter
                                                    .backgroundView
                                headerBehaviour:Overlap]];
    }
  }
  return [results copy];
}

// Returns the safeAreaInsets of the root window for self.view. In some cases,
// the self.view.safeAreaInsets are cleared when the view is unattached ( for
// example on the incognito BVC when the normal BVC is the one active or vice
// versa). Attached or unattached, going to the window through the SceneState
// for the self.browser solves both issues.
- (UIEdgeInsets)rootSafeAreaInsets {
  if (_isShutdown) {
    return UIEdgeInsetsZero;
  }
  UIEdgeInsets safeArea = self.safeAreaProvider.safeArea;

  return UIEdgeInsetsEqualToEdgeInsets(safeArea, UIEdgeInsetsZero)
             ? self.view.safeAreaInsets
             : safeArea;
}

- (CGFloat)headerOffset {
  CGFloat headerOffset = self.rootSafeAreaInsets.top;
  return IsRegularXRegularSizeClass(self) ? headerOffset : 0.0;
}

- (CGFloat)headerHeight {
  NSArray<HeaderDefinition*>* views = [self headerViews];

  CGFloat height = self.headerOffset;
  for (HeaderDefinition* header in views) {
    if (header.view && header.behaviour == Hideable) {
      height += CGRectGetHeight([header.view frame]);
    }
  }

  CGFloat statusBarOffset = 0;
  return height - statusBarOffset;
}

- (UIView*)viewForCurrentWebState {
  return [self viewForWebState:self.currentWebState];
}

- (void)updateWebStateVisibility:(BOOL)isVisible {
  if (isVisible) {
    // TODO(crbug.com/40630853): The webState is not necessarily added to the
    // view hierarchy, even though the bookkeeping says that the WebState is
    // visible. Do not DCHECK([webState->GetView() window]) here since this is a
    // known issue.
    self.currentWebState->WasShown();
  } else {
    self.currentWebState->WasHidden();
  }
}

- (web::WebState*)currentWebState {
  return self.webStateList ? _webStateList->GetActiveWebState() : nullptr;
}

- (WebStateList*)webStateList {
  WebStateList* webStateList = _webStateList.get();
  return webStateList ? webStateList : nullptr;
}

#pragma mark - Public methods

- (void)shieldWasTapped:(id)sender {
  [self.omniboxCommandsHandler cancelOmniboxEdit];
}

- (void)openNewTabFromOriginPoint:(CGPoint)originPoint
                     focusOmnibox:(BOOL)focusOmnibox
                    inheritOpener:(BOOL)inheritOpener {
  const BOOL offTheRecord = _isOffTheRecord;
  ProceduralBlock oldForegroundTabWasAddedCompletionBlock =
      self.foregroundTabWasAddedCompletionBlock;
  id<OmniboxCommands> omniboxCommandHandler = self.omniboxCommandsHandler;
  self.foregroundTabWasAddedCompletionBlock = ^{
    if (oldForegroundTabWasAddedCompletionBlock) {
      oldForegroundTabWasAddedCompletionBlock();
    }
    if (focusOmnibox) {
      [omniboxCommandHandler focusOmnibox];
    }
  };

  [self setLastTapPointFromCommand:originPoint];

  // In most cases, we want to take a snapshot of the current tab before opening
  // a new tab. However, if the current tab is not fully visible (did not finish
  // `-viewDidAppear:`, then we must not take an empty snapshot, replacing an
  // existing snapshot for the tab. This can happen when a new regular tab is
  // opened from an incognito tab. A different BVC is displayed, which may not
  // have enough time to finish appearing before a snapshot is requested.
  if (self.currentWebState && self.viewVisible) {
    SnapshotTabHelper::FromWebState(self.currentWebState)
        ->UpdateSnapshotWithCallback(nil);
  }

  UrlLoadParams params = UrlLoadParams::InNewTab(GURL(kChromeUINewTabURL));
  params.web_params.transition_type = ui::PAGE_TRANSITION_TYPED;
  params.in_incognito = offTheRecord;
  params.inherit_opener = inheritOpener;
  _urlLoadingBrowserAgent->Load(params);
}

- (void)appendTabAddedCompletion:(ProceduralBlock)tabAddedCompletion {
  if (tabAddedCompletion) {
    if (self.foregroundTabWasAddedCompletionBlock) {
      ProceduralBlock oldForegroundTabWasAddedCompletionBlock =
          self.foregroundTabWasAddedCompletionBlock;
      self.foregroundTabWasAddedCompletionBlock = ^{
        oldForegroundTabWasAddedCompletionBlock();
        tabAddedCompletion();
      };
    } else {
      self.foregroundTabWasAddedCompletionBlock = tabAddedCompletion;
    }
  }
}

- (void)startVoiceSearch {
  // Delay Voice Search until new tab animations have finished.
  if (self.inNewTabAnimation) {
    _startVoiceSearchAfterNewTabAnimation = YES;
    return;
  }

  // Keyboard shouldn't overlay the ecoutez window, so dismiss find in page and
  // dismiss the keyboard.
  [self.findInPageCommandsHandler closeFindInPage];
  [self.textZoomHandler closeTextZoom];
  [self.viewForCurrentWebState endEditing:NO];

  // Present voice search.
  [_voiceSearchController
      startRecognitionOnViewController:self
                              webState:self.currentWebState];
  [self.omniboxCommandsHandler cancelOmniboxEdit];
}

#pragma mark - browser_view_controller+private.h

- (void)setActive:(BOOL)active {
  if (_active == active) {
    return;
  }
  _active = active;

  [self updateBroadcastState];

  if (active) {
    // Force loading the view in case it was not loaded yet.
    [self loadViewIfNeeded];
    _pagePlaceholderBrowserAgent->AddPagePlaceholder();
    if (self.viewForCurrentWebState) {
      [self displayTabView];
    }
  }
  [self setNeedsStatusBarAppearanceUpdate];
}

// TODO(crbug.com/40842434): Federate ClearPresentedState.
- (void)clearPresentedStateWithCompletion:(ProceduralBlock)completion
                           dismissOmnibox:(BOOL)dismissOmnibox {
  [_bookmarksCoordinator dismissBookmarkModalControllerAnimated:NO];
  [_bookmarksCoordinator dismissSnackbar];
  if (dismissOmnibox) {
    [self.omniboxCommandsHandler cancelOmniboxEdit];
  }
  [self.helpHandler hideAllHelpBubbles];
  [_voiceSearchController dismissMicPermissionHelp];
  [self.findInPageCommandsHandler closeFindInPage];
  [self.textZoomHandler closeTextZoom];

  [self.popupMenuCommandsHandler dismissPopupMenuAnimated:NO];

  if (self.presentedViewController) {
    // Dismisses any other modal controllers that may be present, e.g. Recent
    // Tabs.
    //
    // Note that currently, some controllers like the bookmark ones were already
    // dismissed (in this example in -dismissBookmarkModalControllerAnimated:),
    // but are still reported as the presentedViewController.  Calling
    // `dismissViewControllerAnimated:completion:` again would dismiss the BVC
    // itself, so instead check the value of `self.dismissingModal` and only
    // call dismiss if one of the above calls has not already triggered a
    // dismissal.
    //
    // To ensure the completion is called, nil is passed to the call to dismiss,
    // and the completion is called explicitly below.
    if (!self.dismissingModal) {
      [self dismissViewControllerAnimated:NO completion:nil];
    }
    // Dismissed controllers will be so after a delay. Queue the completion
    // callback after that.
    if (completion) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, base::BindOnce(completion), base::Milliseconds(400));
    }
  } else if (completion) {
    // If no view controllers are presented, we should be ok with dispatching
    // the completion block directly.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(completion));
  }
}

- (void)animateOpenBackgroundTabFromOriginPoint:(CGPoint)originPoint
                                     completion:(void (^)())completion {
  if (IsRegularXRegularSizeClass(self) ||
      CGPointEqualToPoint(originPoint, CGPointZero)) {
    completion();
  } else {
    self.inNewTabAnimation = YES;
    // Exit fullscreen if needed.
    self.fullscreenController->ExitFullscreen();
    const CGFloat kAnimatedViewSize = 50;
    BackgroundTabAnimationView* animatedView =
        [[BackgroundTabAnimationView alloc]
            initWithFrame:CGRectMake(0, 0, kAnimatedViewSize, kAnimatedViewSize)
                incognito:_isOffTheRecord];
    animatedView.layoutGuideCenter = _layoutGuideCenter;
    __weak UIView* weakAnimatedView = animatedView;
    auto completionBlock = ^() {
      self.inNewTabAnimation = NO;
      [weakAnimatedView removeFromSuperview];
      completion();
    };
    [self.view addSubview:animatedView];
    [animatedView animateFrom:originPoint
        toTabGridButtonWithCompletion:completionBlock];
  }
}

- (void)shutdown {
  DCHECK(!_isShutdown);
  _isShutdown = YES;

  // Disconnect child coordinators.
  if (IsModernTabStripOrRaccoonEnabled()) {
    [self.tabStripCoordinator stop];
    self.tabStripCoordinator = nil;
  } else {
    [self.legacyTabStripCoordinator stop];
    self.legacyTabStripCoordinator = nil;
  }
  self.tabStripView = nil;

  _bubblePresenter = nil;

  [self.contentArea removeGestureRecognizer:self.contentAreaGestureRecognizer];

  [self.toolbarCoordinator stop];
  self.toolbarCoordinator = nil;
  _sideSwipeMediator = nil;
  [_voiceSearchController disconnect];
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  _bookmarksCoordinator = nil;
}

#pragma mark - NSObject

- (BOOL)accessibilityPerformEscape {
  [self dismissPopups];
  return YES;
}

#pragma mark - UIResponder

// To always be able to register key commands, the VC must be able to become
// first responder.
- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (UIResponder*)nextResponder {
  UIResponder* nextResponder = [super nextResponder];
  if (_keyCommandsProvider && [self shouldSupportKeyCommands]) {
    [_keyCommandsProvider respondBetweenViewController:self
                                          andResponder:nextResponder];
    return _keyCommandsProvider;
  } else {
    return nextResponder;
  }
}

#pragma mark - UIResponder Helpers

// Whether the BVC should declare keyboard commands.
// Since `-keyCommands` can be called by UIKit at any time, no assumptions
// about the state of `self` can be made; accordingly, if there's anything
// not initialized (or being torn down), this method should return NO.
- (BOOL)shouldSupportKeyCommands {
  if (_isShutdown)
    return NO;

  if (self.presentedViewController)
    return NO;

  if (_voiceSearchController.visible)
    return NO;

  return self.viewVisible;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  CGRect initialViewsRect = self.view.bounds;
  UIViewAutoresizing initialViewAutoresizing =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

  self.contentArea.frame = initialViewsRect;

  // Create the typing shield.  It is initially hidden, and is made visible when
  // the keyboard appears.
  self.typingShield = [[UIButton alloc] initWithFrame:initialViewsRect];
  self.typingShield.hidden = YES;
  self.typingShield.autoresizingMask = initialViewAutoresizing;
  self.typingShield.accessibilityIdentifier = @"Typing Shield";
  self.typingShield.accessibilityLabel = l10n_util::GetNSString(IDS_CANCEL);
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    self.typingShield.backgroundColor =
        [UIColor colorNamed:kOmniboxPopoutOverlayColor];
  }
  [self.typingShield addTarget:self
                        action:@selector(shieldWasTapped:)
              forControlEvents:UIControlEventTouchUpInside];
  self.view.autoresizingMask = initialViewAutoresizing;

  [self addChildViewController:self.browserContainerViewController];
  [self.view addSubview:self.contentArea];
  [self.browserContainerViewController didMoveToParentViewController:self];
  [self.view addSubview:self.typingShield];
  [super viewDidLoad];

  // Install fake status bar for iPad iOS7
  [self installFakeStatusBar];

  [self buildToolbarAndTabStrip];
  [self setUpViewLayout:YES];
  [self addConstraintsToToolbar];

  [_sideSwipeMediator addHorizontalGesturesToView:self.view];

  // Add a tap gesture recognizer to save the last tap location for the source
  // location of the new tab animation.
  self.contentAreaGestureRecognizer = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(saveContentAreaTapLocation:)];
  [self.contentAreaGestureRecognizer setDelegate:self];
  [self.contentAreaGestureRecognizer setCancelsTouchesInView:NO];
  [self.contentArea addGestureRecognizer:self.contentAreaGestureRecognizer];

  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];
}

- (void)viewSafeAreaInsetsDidChange {
  [super viewSafeAreaInsetsDidChange];
  [self setUpViewLayout:NO];
  // Update the heights of the toolbars to account for the new insets.
  self.primaryToolbarHeightConstraint.constant =
      [self primaryToolbarHeightWithInset];
  self.secondaryToolbarHeightConstraint.constant =
      [self secondaryToolbarHeightWithInset];

  // Update the tab strip placement.
  if (self.tabStripView) {
    [self showTabStripView:self.tabStripView];
  }
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  // Update the toolbar height to account for `topLayoutGuide` changes.
  self.primaryToolbarHeightConstraint.constant =
      [self primaryToolbarHeightWithInset];

  if (self.ntpCoordinator.isNTPActiveForCurrentWebState &&
      self.webUsageEnabled) {
    self.ntpCoordinator.viewController.view.frame =
        [self ntpFrameForCurrentWebState];
  }
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  self.viewVisible = YES;
  [self updateBroadcastState];
  [self updateToolbarState];

  // If there is no first responder, try to make the webview the first
  // responder to have it answer keyboard commands (e.g. space bar to scroll
  // and respond to gampad controllers). The WKContentView must be the first
  // responder before (or very shortly after) a load starts in order for
  // gamepads to work. (Ref: crbug.com/325307469)
  web::WebState* activeWebState = self.currentWebState;
  if (activeWebState && !GetFirstResponder()) {
    NewTabPageTabHelper* NTPHelper =
        NewTabPageTabHelper::FromWebState(activeWebState);
    if (!NTPHelper || !NTPHelper->IsActive()) {
      [activeWebState->GetWebViewProxy() becomeFirstResponder];
    }
  }
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  self.visible = YES;

  // If the controller is suspended, or has been paged out due to low memory,
  // updating the view will be handled when it's displayed again.
  if (!self.webUsageEnabled || !self.contentArea) {
    return;
  }
  // Update the displayed view (if any; the switcher may not have created
  // one yet) in case it changed while showing the switcher.
  if (self.viewForCurrentWebState) {
    [self displayTabView];
  }
}

- (void)viewWillDisappear:(BOOL)animated {
  self.viewVisible = NO;
  [self updateBroadcastState];
  web::WebState* activeWebState = self.currentWebState;
  if (activeWebState) {
    [self updateWebStateVisibility:NO];
    if (!self.presentedViewController)
      activeWebState->SetKeepRenderProcessAlive(false);
  }

  [_bookmarksCoordinator dismissSnackbar];
  [super viewWillDisappear:animated];
}

- (BOOL)prefersStatusBarHidden {
  return self.hideStatusBar || [super prefersStatusBarHidden];
}

// Called when in the foreground and the OS needs more memory. Release as much
// as possible.
- (void)didReceiveMemoryWarning {
  // Releases the view if it doesn't have a superview.
  [super didReceiveMemoryWarning];

  if (![self isViewLoaded]) {
    self.typingShield = nil;
    _voiceSearchController.dispatcher = nil;
    [self.toolbarCoordinator stop];
    self.toolbarCoordinator = nil;
    _toolbarUIState = nil;
    if (IsModernTabStripOrRaccoonEnabled()) {
      [self.tabStripCoordinator stop];
      self.tabStripCoordinator = nil;
    } else {
      [self.legacyTabStripCoordinator stop];
      self.legacyTabStripCoordinator = nil;
    }
    self.tabStripView = nil;
    _sideSwipeMediator = nil;
  }
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];

  if (@available(iOS 17.0, *)) {
    if (base::FeatureList::IsEnabled(kEnableTraitCollectionWorkAround)) {
      [self updateTraitsIfNeeded];
    }
  }

  // After `-shutdown` is called, profile is invalid and will cause a
  // crash.
  if (_isShutdown) {
    return;
  }

  if (self.traitCollection.horizontalSizeClass ==
          previousTraitCollection.horizontalSizeClass &&
      self.traitCollection.verticalSizeClass ==
          previousTraitCollection.verticalSizeClass) {
    return;
  }

  self.fullscreenController->BrowserTraitCollectionChangedBegin();

  // TODO(crbug.com/41198852): - traitCollectionDidChange: is not always
  // forwarded because in some cases the presented view controller isn't a child
  // of the BVC in the view controller hierarchy (some intervening object isn't
  // a view controller).
  [self.presentedViewController
      traitCollectionDidChange:previousTraitCollection];

  if (self.currentWebState) {
    UIEdgeInsets contentPadding =
        self.currentWebState->GetWebViewProxy().contentInset;
    contentPadding.bottom = AlignValueToPixel(
        self.footerFullscreenProgress * [self secondaryToolbarHeightWithInset]);
    self.currentWebState->GetWebViewProxy().contentInset = contentPadding;
  }

  // Toolbar state must be updated before `updateFootersForFullscreenProgress`
  // as the later uses the insets from fullscreen model.
  [self updateToolbarState];

  // Change the height of the secondary toolbar to show/hide it.
  self.secondaryToolbarHeightConstraint.constant =
      [self secondaryToolbarHeightWithInset];
  [self updateFootersForFullscreenProgress:self.footerFullscreenProgress];

  // If the device's size class has changed from RegularXRegular to another and
  // vice-versa, the find bar should switch between regular mode and compact
  // mode accordingly. Hide the findbar here and it will be reshown in [self
  // updateToobar];
  if (ShouldShowCompactToolbar(previousTraitCollection) !=
      ShouldShowCompactToolbar(self)) {
    if (!IsNativeFindInPageAvailable()) {
      [self.findInPageCommandsHandler hideFindUI];
    }
    [self.textZoomHandler hideTextZoomUI];
  }

  // Update the toolbar visibility.
  // TODO(crbug.com/40842406): Remove this and let
  // `PrimaryToolbarViewController` or `ToolbarCoordinator` call the update ?
  [self.toolbarCoordinator updateToolbar];

  // Update the tab strip visibility.
  if (self.tabStripView) {
    [self showTabStripView:self.tabStripView];
    [self.tabStripView layoutSubviews];
    const bool canShowTabStrip = IsRegularXRegularSizeClass(self);
    if (IsModernTabStripOrRaccoonEnabled()) {
      [self.tabStripCoordinator hideTabStrip:!canShowTabStrip];
    } else {
      [self.legacyTabStripCoordinator hideTabStrip:!canShowTabStrip];
    }
    _fakeStatusBarView.hidden = !canShowTabStrip;
    [self addConstraintsToPrimaryToolbar];
    // If tabstrip is coming back due to a window resize or screen rotation,
    // reset the full screen controller to adjust the tabstrip position.
    if (ShouldShowCompactToolbar(previousTraitCollection) &&
        !ShouldShowCompactToolbar(self)) {
      [self
          updateForFullscreenProgress:self.fullscreenController->GetProgress()];
    }
  }

  [self setNeedsStatusBarAppearanceUpdate];

  self.fullscreenController->BrowserTraitCollectionChangedEnd();
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];

  // After `-shutdown` is called, browser is invalid and will cause a crash.
  if (_isShutdown)
    return;

  // TODO(crbug.com/40432185): Support size changes for all popups and modal
  // dialogs.
  [self.helpHandler hideAllHelpBubbles];
  if (!IsNewOverflowMenuEnabled()) {
    [self.popupMenuCommandsHandler dismissPopupMenuAnimated:NO];
  }

  __weak BrowserViewController* weakSelf = self;

  [coordinator
      animateAlongsideTransition:^(
          id<UIViewControllerTransitionCoordinatorContext>) {
        [weakSelf animateTransition];
      }
      completion:^(id<UIViewControllerTransitionCoordinatorContext>) {
        [weakSelf completedTransition];
      }];

  crash_keys::SetCurrentOrientation(GetInterfaceOrientation(),
                                    [[UIDevice currentDevice] orientation]);
}

- (void)animateTransition {
  // Force updates of the toolbar state as the toolbar height might
  // change on rotation.
  [self updateToolbarState];
  // Resize horizontal viewport if Smooth Scrolling is on.
  if (ios::provider::IsFullscreenSmoothScrollingSupported()) {
    self.fullscreenController->ResizeHorizontalViewport();
  }

  [self.popupMenuCommandsHandler adjustPopupSize];
}

- (void)completedTransition {
  if (!IsModernTabStripOrRaccoonEnabled()) {
    if (self.tabStripView) {
      [self.legacyTabStripCoordinator tabStripSizeDidChange];
    }
  }
}

- (void)dismissViewControllerAnimated:(BOOL)flag
                           completion:(void (^)())completion {
  if (!self.presentedViewController) {
    // TODO(crbug.com/41364311): On iOS10, UIDocumentMenuViewController and
    // WKFileUploadPanel somehow combine to call dismiss twice instead of once.
    // The second call would dismiss the BVC itself, so look for that case and
    // return early.
    //
    // TODO(crbug.com/41370278): A similar bug exists on all iOS versions with
    // WKFileUploadPanel and UIDocumentPickerViewController.
    //
    // To make M65 as safe as possible, return early whenever this method is
    // invoked but no VC appears to be presented.  These cases will always end
    // up dismissing the BVC itself, which would put the app into an
    // unresponsive state.
    return;
  }

  // Some calling code invokes `dismissViewControllerAnimated:completion:`
  // multiple times. Because the BVC is presented, subsequent calls end up
  // dismissing the BVC itself. This is never what should happen, so check for
  // this case and return early.  It is not enough to check
  // `self.dismissingModal` because some dismissals do not go through
  // -[BrowserViewController dismissViewControllerAnimated:completion:`.
  // TODO(crbug.com/40548564): Fix callers and remove this early return.
  if (self.dismissingModal || self.presentedViewController.isBeingDismissed) {
    return;
  }

  self.dismissingModal = YES;
  __weak BrowserViewController* weakSelf = self;
  [super dismissViewControllerAnimated:flag
                            completion:^{
                              BrowserViewController* strongSelf = weakSelf;
                              strongSelf.dismissingModal = NO;
                              if (completion)
                                completion();
                            }];
}

// The BVC does not define its own presentation context, so any presentation
// here ultimately travels up the chain for presentation.
- (void)presentViewController:(UIViewController*)viewControllerToPresent
                     animated:(BOOL)flag
                   completion:(void (^)())completion {
  ProceduralBlock finalCompletionHandler = [completion copy];
  // TODO(crbug.com/41235932) This is an interim fix for the flicker between the
  // launch screen and the FRE Animation. The fix is, if the FRE is about to be
  // presented, to show a temporary view of the launch screen and then remove it
  // when the controller for the FRE has been presented. This fix should be
  // removed when the FRE startup code is rewritten.
  const bool firstRunLaunch = ShouldPresentFirstRunExperience();
  // These if statements check that `presentViewController` is being called for
  // the FRE case.
  if (firstRunLaunch &&
      [viewControllerToPresent isKindOfClass:[UINavigationController class]]) {
    UINavigationController* navController =
        base::apple::ObjCCastStrict<UINavigationController>(
            viewControllerToPresent);
    if ([navController.topViewController
            isKindOfClass:[PromoStyleViewController class]]) {
      self.hideStatusBar = YES;

      // Load view from Launch Screen and add it to window.
      NSBundle* mainBundle = base::apple::FrameworkBundle();
      NSArray* topObjects = [mainBundle loadNibNamed:@"LaunchScreen"
                                               owner:self
                                             options:nil];
      UIViewController* launchScreenController =
          base::apple::ObjCCastStrict<UIViewController>(
              [topObjects lastObject]);
      // `launchScreenView` is loaded as an autoreleased object, and is retained
      // by the `completion` block below.
      UIView* launchScreenView = launchScreenController.view;
      launchScreenView.userInteractionEnabled = NO;
      // TODO(crbug.com/40101769): Displaying the launch screen is a hack to
      // hide the build up of the UI from the user. To implement the hack, this
      // view controller uses information that it should not know or care about:
      // this BVC is contained and its parent bounds to the full screen.
      launchScreenView.frame = self.parentViewController.view.bounds;
      [self.parentViewController.view addSubview:launchScreenView];
      [launchScreenView setNeedsLayout];
      [launchScreenView layoutIfNeeded];

      // Replace the completion handler sent to the superclass with one which
      // removes `launchScreenView` and resets the status bar. If `completion`
      // exists, it is called from within the new completion handler.
      __weak BrowserViewController* weakSelf = self;
      finalCompletionHandler = ^{
        [launchScreenView removeFromSuperview];
        weakSelf.hideStatusBar = NO;
        if (completion)
          completion();
      };
    }
  }
  [_sideSwipeMediator resetContentView];

  void (^superCall)() = ^{
    [super presentViewController:viewControllerToPresent
                        animated:flag
                      completion:finalCompletionHandler];
  };
  // TODO(crbug.com/40628488): The Default Browser Promo is
  // currently the only presented controller that allows interaction with the
  // rest of the App while they are being presented. Dismiss it in case the user
  // or system has triggered another presentation.
  if ([self.nonModalPromoPresentationDelegate defaultNonModalPromoIsShowing]) {
    [self.nonModalPromoPresentationDelegate
        dismissDefaultNonModalPromoAnimated:NO
                                 completion:superCall];

  } else {
    superCall();
  }
}

- (BOOL)shouldAutorotate {
  if (self.presentedViewController.beingPresented ||
      self.presentedViewController.beingDismissed) {
    // Don't rotate while a presentation or dismissal animation is occurring.
    return NO;
  } else if (_sideSwipeMediator && ![_sideSwipeMediator shouldAutorotate]) {
    // Don't auto rotate if side swipe controller view says not to.
    return NO;
  } else {
    return [super shouldAutorotate];
  }
}

- (UIStatusBarStyle)preferredStatusBarStyle {
  if (IsRegularXRegularSizeClass(self) && !_isOffTheRecord &&
      !IsModernTabStripOrRaccoonEnabled()) {
    return self.tabStripView.frame.origin.y < kTabStripAppearanceOffset
               ? UIStatusBarStyleDefault
               : UIStatusBarStyleLightContent;
  }
  return _isOffTheRecord ? UIStatusBarStyleLightContent
                         : UIStatusBarStyleDefault;
}

#pragma mark - ** Private BVC Methods **

// On iOS7, iPad should match iOS6 status bar.  Install a simple black bar under
// the status bar to mimic this layout.
- (void)installFakeStatusBar {
  // This method is called when the view is loaded.

  // Remove the _fakeStatusBarView if present.
  [_fakeStatusBarView removeFromSuperview];
  _fakeStatusBarView = nil;

  CGRect statusBarFrame = CGRectMake(0, 0, CGRectGetWidth(self.view.bounds), 0);
  _fakeStatusBarView = [[UIView alloc] initWithFrame:statusBarFrame];
  [_fakeStatusBarView setAutoresizingMask:UIViewAutoresizingFlexibleWidth];
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    if (IsModernTabStripOrRaccoonEnabled()) {
      _fakeStatusBarView.backgroundColor = [TabStripHelper backgroundColor];
      // Force the UserInterfaceStyle update in incognito.
      _fakeStatusBarView.overrideUserInterfaceStyle =
          _isOffTheRecord ? UIUserInterfaceStyleDark
                          : UIUserInterfaceStyleUnspecified;
      const bool canShowTabStrip = IsRegularXRegularSizeClass(self);
      _fakeStatusBarView.hidden = !canShowTabStrip;
    } else {
      _fakeStatusBarView.backgroundColor = UIColor.blackColor;
    }
    _fakeStatusBarView.autoresizingMask = UIViewAutoresizingFlexibleWidth;
    DCHECK(self.contentArea);
    [self.view insertSubview:_fakeStatusBarView aboveSubview:self.contentArea];
  } else {
    // Add a white bar when there is no tab strip so that the status bar on the
    // NTP is white.
    _fakeStatusBarView.backgroundColor = ntp_home::NTPBackgroundColor();
    [self.view insertSubview:_fakeStatusBarView atIndex:0];
  }
}

// Builds the UI parts of tab strip and the toolbar. Does not matter whether
// or not profile and browser are valid.
- (void)buildToolbarAndTabStrip {
  DCHECK([self isViewLoaded]);

  [self updateBroadcastState];
  if (_voiceSearchController) {
    _voiceSearchController.dispatcher = self.loadQueryCommandsHandler;
  }

  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    const bool canShowTabStrip = IsRegularXRegularSizeClass(self);
    if (IsModernTabStripOrRaccoonEnabled()) {
      [self.tabStripCoordinator start];
      [self.tabStripCoordinator hideTabStrip:!canShowTabStrip];
    } else {
      self.legacyTabStripCoordinator.presentationProvider = self;
      [self.legacyTabStripCoordinator start];
      [self.legacyTabStripCoordinator hideTabStrip:!canShowTabStrip];
    }
  }
}

// The height of the primary toolbar with the top safe area inset included.
- (CGFloat)primaryToolbarHeightWithInset {
  CGFloat height = self.toolbarCoordinator.expandedPrimaryToolbarHeight;
  // If the primary toolbar is not the topmost header, it does not overlap with
  // the unsafe area.
  // TODO(crbug.com/41367346): Update implementation such that this calculates
  // the topmost header's height.
  UIView* primaryToolbar =
      self.toolbarCoordinator.primaryToolbarViewController.view;
  UIView* topmostHeader = [self.headerViews firstObject].view;
  if (primaryToolbar != topmostHeader)
    return height;
  // If the primary toolbar is topmost, subtract the height of the portion of
  // the unsafe area.
  CGFloat unsafeHeight = self.rootSafeAreaInsets.top;

  // The topmost header is laid out `headerOffset` from the top of `view`, so
  // subtract that from the unsafe height.
  unsafeHeight -= self.headerOffset;
  return height + unsafeHeight;
}

// The height of the secondary toolbar with the bottom safe area inset included.
// Returns 0 if the toolbar should be hidden.
- (CGFloat)secondaryToolbarHeightWithInset {
  CGFloat height = self.toolbarCoordinator.expandedSecondaryToolbarHeight;
  if (!height) {
    return 0.0;
  }
  // Add the safe area inset to the toolbar height.
  CGFloat unsafeHeight = self.rootSafeAreaInsets.bottom;
  return height + unsafeHeight;
}

// Sets up the constraints on the toolbar.
- (void)addConstraintsToPrimaryToolbar {
  NSLayoutYAxisAnchor* topAnchor;
  // On iPhone, the toolbar is underneath the top of the screen.
  // On iPad, it depends:
  // - if the window is compact, it is like iPhone, underneath the top of the
  // screen.
  // - if the window is regular, it is underneath the tab strip.
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE ||
      !IsRegularXRegularSizeClass(self)) {
    topAnchor = self.view.topAnchor;
  } else {
    topAnchor = self.tabStripView.bottomAnchor;
  }

  // Only add leading and trailing constraints once as they are never updated.
  // This uses the existence of `primaryToolbarOffsetConstraint` as a proxy for
  // whether we've already added the leading and trailing constraints.
  if (!self.primaryToolbarOffsetConstraint) {
    [NSLayoutConstraint activateConstraints:@[
      [self.toolbarCoordinator.primaryToolbarViewController.view.leadingAnchor
          constraintEqualToAnchor:[self view].leadingAnchor],
      [self.toolbarCoordinator.primaryToolbarViewController.view.trailingAnchor
          constraintEqualToAnchor:[self view].trailingAnchor],
    ]];
  }

  // Offset and Height can be updated, so reset first.
  self.primaryToolbarOffsetConstraint.active = NO;
  self.primaryToolbarHeightConstraint.active = NO;

  // Create a constraint for the vertical positioning of the toolbar.
  UIView* primaryView =
      self.toolbarCoordinator.primaryToolbarViewController.view;
  self.primaryToolbarOffsetConstraint =
      [primaryView.topAnchor constraintEqualToAnchor:topAnchor];

  // Create a constraint for the height of the toolbar to include the unsafe
  // area height.
  self.primaryToolbarHeightConstraint = [primaryView.heightAnchor
      constraintEqualToConstant:[self primaryToolbarHeightWithInset]];

  self.primaryToolbarOffsetConstraint.active = YES;
  self.primaryToolbarHeightConstraint.active = YES;
}

- (void)addConstraintsToSecondaryToolbar {
  // Create a constraint for the height of the toolbar to include the unsafe
  // area height.
  UIView* toolbarView =
      self.toolbarCoordinator.secondaryToolbarViewController.view;
  self.secondaryToolbarHeightConstraint = [toolbarView.heightAnchor
      constraintEqualToConstant:[self secondaryToolbarHeightWithInset]];
  // The bottom toolbar can be constraint to the keyboard in some cases.
  self.secondaryToolbarHeightConstraint.priority = UILayoutPriorityRequired - 1;
  self.secondaryToolbarHeightConstraint.active = YES;
  AddSameConstraintsToSides(
      self.view, toolbarView,
      LayoutSides::kBottom | LayoutSides::kLeading | LayoutSides::kTrailing);
}

// Adds constraints to the primary and secondary toolbars, anchoring them to the
// top and bottom of the browser view.
- (void)addConstraintsToToolbar {
  [self addConstraintsToPrimaryToolbar];
  [self addConstraintsToSecondaryToolbar];
  [[self view] layoutIfNeeded];
}

// Sets up the frame for the fake status bar. View must be loaded.
- (void)setupStatusBarLayout {
  CGFloat topInset = self.rootSafeAreaInsets.top;

  // Update the fake toolbar background height.
  CGRect fakeStatusBarFrame = _fakeStatusBarView.frame;
  fakeStatusBarFrame.size.height = topInset;
  _fakeStatusBarView.frame = fakeStatusBarFrame;
}

// Sets the correct frame and hierarchy for subviews and helper views.  Only
// insert views on `initialLayout`.
- (void)setUpViewLayout:(BOOL)initialLayout {
  DCHECK([self isViewLoaded]);

  [self setupStatusBarLayout];

  if (initialLayout) {
    // Add the toolbars as child view controllers.
    [self addChildViewController:self.toolbarCoordinator
                                     .primaryToolbarViewController];
    [self addChildViewController:self.toolbarCoordinator
                                     .secondaryToolbarViewController];

    // Add the primary toolbar. On iPad, it should be in front of the tab strip
    // because the tab strip slides behind it when showing the thumb strip.
    UIView* primaryToolbarView =
        self.toolbarCoordinator.primaryToolbarViewController.view;
    if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
      if (IsModernTabStripOrRaccoonEnabled() && self.tabStripCoordinator) {
        UIViewController* tabStripViewController =
            self.tabStripCoordinator.viewController;
        [self addChildViewController:tabStripViewController];
        self.tabStripView = tabStripViewController.view;
        [self.view addSubview:self.tabStripView];
        [tabStripViewController didMoveToParentViewController:self];
        CGRect tabStripFrame =
            CGRectMake(0, self.headerOffset, self.view.bounds.size.width,
                       kModernTabStripHeight);
        self.tabStripView.frame = tabStripFrame;
        self.tabStripView.autoresizingMask =
            (UIViewAutoresizingFlexibleWidth |
             UIViewAutoresizingFlexibleBottomMargin);
      }
      if (IsModernTabStripOrRaccoonEnabled()) {
        [self.view insertSubview:primaryToolbarView
                    aboveSubview:self.tabStripView];
      } else {
        [self.view insertSubview:primaryToolbarView
                    belowSubview:self.tabStripView];
      }

    } else {
      [self.view addSubview:primaryToolbarView];
    }
    [self.view insertSubview:self.toolbarCoordinator
                                 .secondaryToolbarViewController.view
                aboveSubview:primaryToolbarView];

    // TODO(crbug.com/40270239): Migrate kContentAreaGuide to LayoutGuideCenter.
    // Add guide kContentAreaGuide to the browser view.
    [self.view
        addLayoutGuide:[[NamedGuide alloc] initWithName:kContentAreaGuide]];

    // Configure the content area guide.
    NamedGuide* contentAreaGuide = [NamedGuide guideWithName:kContentAreaGuide
                                                        view:self.view];

    // TODO(crbug.com/40724393): Sometimes, `contentAreaGuide` and
    // `primaryToolbarView` aren't in the same view hierarchy; this seems to be
    // impossible,  but it does still happen. This will cause an exception in
    // when activiating these constraints. To gather more information about this
    // state, explciitly check the view hierarchy roots. Local variables are
    // used so that the CHECK message is cleared.
    UIView* rootViewForToolbar = ViewHierarchyRootForView(primaryToolbarView);
    UIView* rootViewForContentGuide =
        ViewHierarchyRootForView(contentAreaGuide.owningView);
    CHECK_EQ(rootViewForToolbar, rootViewForContentGuide);

    // Constrain top to bottom of top toolbar.
    [contentAreaGuide.topAnchor
        constraintEqualToAnchor:primaryToolbarView.bottomAnchor]
        .active = YES;

    LayoutSides contentSides = LayoutSides::kLeading | LayoutSides::kTrailing;
    // If there's a bottom toolbar, the content area guide is constrained to
    // its top.
    UIView* secondaryToolbarView =
        self.toolbarCoordinator.secondaryToolbarViewController.view;
    [contentAreaGuide.bottomAnchor
        constraintEqualToAnchor:secondaryToolbarView.topAnchor]
        .active = YES;

    AddSameConstraintsToSides(self.view, contentAreaGuide, contentSides);

    // Complete child UIViewController containment flow now that the views are
    // finished being added.
    [self.tabStripCoordinator.viewController
        didMoveToParentViewController:self];
    [self.toolbarCoordinator.primaryToolbarViewController
        didMoveToParentViewController:self];
    [self.toolbarCoordinator.secondaryToolbarViewController
        didMoveToParentViewController:self];
  }

  // Resize the typing shield to cover the entire browser view and bring it to
  // the front.
  self.typingShield.frame = self.contentArea.frame;
  if (initialLayout) {
    [self.view bringSubviewToFront:self.typingShield];
  }

  // Move the overlay containers in front of the hierarchy.
  [self updateOverlayContainerOrder];
}

// Displays the current webState view.
- (void)displayTabView {
  UIView* view = self.viewForCurrentWebState;
  DCHECK(view);
  [self loadViewIfNeeded];

  if (!self.inNewTabAnimation) {
    // TODO(crbug.com/40842406): -updateToolbar will move out of the BVC; make
    // sure this comment remains accurate. Hide findbar.  `updateToolbar` will
    // restore the findbar later.
    [self.findInPageCommandsHandler hideFindUI];
    [self.textZoomHandler hideTextZoomUI];

    // Make new content visible, resizing it first as the orientation may
    // have changed from the last time it was displayed.
    CGRect viewFrame = self.contentArea.bounds;
    if (!ios::provider::IsFullscreenSmoothScrollingSupported()) {
      // If the Smooth Scrolling is on, the WebState view is not
      // resized, and should always match the bounds of the content area.  When
      // the provider is not initialized, viewport insets resize the webview, so
      // they should be accounted for here to prevent animation jitter.
      UIEdgeInsets viewportInsets =
          self.fullscreenController->GetCurrentViewportInsets();
      viewFrame = UIEdgeInsetsInsetRect(viewFrame, viewportInsets);
    }
    view.frame = viewFrame;

    [self updateToolbarState];

    NewTabPageCoordinator* NTPCoordinator = self.ntpCoordinator;
    if (NTPCoordinator.isNTPActiveForCurrentWebState) {
      UIViewController* viewController = NTPCoordinator.viewController;
      viewController.view.frame = [self ntpFrameForCurrentWebState];
      [viewController.view layoutIfNeeded];
      // TODO(crbug.com/41407753): For a newly created WebState, the session
      // will not be restored until LoadIfNecessary call. Remove when fixed.
      self.currentWebState->GetNavigationManager()->LoadIfNecessary();
      self.browserContainerViewController.contentView = nil;
      self.browserContainerViewController.contentViewController =
          viewController;
      [NTPCoordinator constrainNamedGuideForFeedIPH];
    } else {
      self.browserContainerViewController.contentView = view;
    }
    // Resize horizontal viewport if Smooth Scrolling is on.
    if (ios::provider::IsFullscreenSmoothScrollingSupported()) {
      self.fullscreenController->ResizeHorizontalViewport();
    }
  }

  // TODO(crbug.com/40842406): Remove this and let `ToolbarCoordinator` call the
  // update, somehow. Toolbar needs to know when NTP isActive state changes.
  [self.toolbarCoordinator updateToolbar];

  [self updateWebStateVisibility:YES];
}

- (void)updateOverlayContainerOrder {
  // Both infobar overlay container views should exist in front of the entire
  // browser UI, and the banner container should appear behind the modal
  // container.
  [self bringOverlayContainerToFront:
            self.infobarBannerOverlayContainerViewController];
  [self bringOverlayContainerToFront:
            self.infobarModalOverlayContainerViewController];
}

- (void)bringOverlayContainerToFront:
    (UIViewController*)containerViewController {
  [self.view bringSubviewToFront:containerViewController.view];
  // If `containerViewController` is presenting a view over its current context,
  // its presentation container view is added as a sibling to
  // `containerViewController`'s view. This presented view should be brought in
  // front of the container view.
  UIView* presentedContainerView =
      containerViewController.presentedViewController.presentationController
          .containerView;
  if (presentedContainerView.superview == self.view)
    [self.view bringSubviewToFront:presentedContainerView];
}

#pragma mark - Private Methods: UI Configuration, update and Layout

// Starts or stops broadcasting the toolbar UI and main content UI depending on
// whether the BVC is visible and active.
- (void)updateBroadcastState {
  self.broadcasting = self.active && self.viewVisible;
}

// Dismisses popups and modal dialogs that are displayed above the BVC when the
// accessibility escape gesture is performed.
- (void)dismissPopups {
  // The dispatcher may not be fully connected during shutdown, so selectors may
  // be unrecognized.
  if (_isShutdown)
    return;

  [self.popupMenuCommandsHandler dismissPopupMenuAnimated:NO];
  [self.helpHandler hideAllHelpBubbles];
  [self.omniboxCommandsHandler cancelOmniboxEdit];
}

// Returns the appropriate frame for the NTP.
- (CGRect)ntpFrameForCurrentWebState {
  DCHECK(self.ntpCoordinator.isNTPActiveForCurrentWebState);
  // NTP is laid out only in the visible part of the screen.
  UIEdgeInsets viewportInsets = UIEdgeInsetsZero;
  if (!IsRegularXRegularSizeClass(self)) {
    viewportInsets.bottom = [self secondaryToolbarHeightWithInset];
  }

  // Add toolbar margin to the frame for every scenario except compact-width
  // non-otr, as that is the only case where there isn't a primary toolbar.
  // (see crbug.com/1063173)
  if (!IsSplitToolbarMode(self) || _isOffTheRecord) {
    viewportInsets.top = [self expandedTopToolbarHeight];
  }
  return UIEdgeInsetsInsetRect(self.contentArea.bounds, viewportInsets);
}

// Sets the frame for the headers.
- (void)setFramesForHeaders:(NSArray<HeaderDefinition*>*)headers
                   atOffset:(CGFloat)headerOffset {
  CGFloat height = self.headerOffset;
  for (HeaderDefinition* header in headers) {
    CGFloat yOrigin = height - headerOffset;
    BOOL isPrimaryToolbar =
        header.view ==
        self.toolbarCoordinator.primaryToolbarViewController.view;
    // Make sure the toolbarView's constraints are also updated.  Leaving the
    // -setFrame call to minimize changes in this CL -- otherwise the way
    // toolbar_view manages it's alpha changes would also need to be updated.
    // TODO(crbug.com/40546808): This can be cleaned up when the new fullscreen
    // is enabled.
    if (isPrimaryToolbar && !IsRegularXRegularSizeClass(self)) {
      self.primaryToolbarOffsetConstraint.constant = yOrigin;
    }
    CGRect frame = [header.view frame];
    frame.origin.y = yOrigin;
    [header.view setFrame:frame];
    if (header.behaviour != Overlap)
      height += CGRectGetHeight(frame);

    if (header.view == self.tabStripView)
      [self setNeedsStatusBarAppearanceUpdate];
  }
}

- (UIView*)viewForWebState:(web::WebState*)webState {
  if (!webState)
    return nil;
  if (self.ntpCoordinator.isNTPActiveForCurrentWebState) {
    return self.ntpCoordinator.started ? self.ntpCoordinator.viewController.view
                                       : nil;
  }
  DCHECK(self.webStateList->GetIndexOfWebState(webState) !=
         WebStateList::kInvalidIndex);
  if (webState->IsEvicted() && _tabUsageRecorderBrowserAgent) {
    _tabUsageRecorderBrowserAgent->RecordPageLoadStart(webState);
  }
  if (!webState->IsCrashed()) {
    // Load the page if it was evicted by browsing data clearing logic or if
    // page was never loaded yet after launch.
    webState->GetNavigationManager()->LoadIfNecessary();
  }
  return webState->GetView();
}

#pragma mark - Private Methods: Tap handling

// Record the last tap point based on the `originPoint` (if any) passed in
// command.
- (void)setLastTapPointFromCommand:(CGPoint)originPoint {
  if (CGPointEqualToPoint(originPoint, CGPointZero)) {
    _lastTapPoint = CGPointZero;
  } else {
    _lastTapPoint = [self.view.window convertPoint:originPoint
                                            toView:self.view];
  }
  _lastTapTime = CACurrentMediaTime();
}

// Returns the last stored `_lastTapPoint` if it's been set within the past
// second.
- (CGPoint)lastTapPoint {
  if (CACurrentMediaTime() - _lastTapTime < 1) {
    return _lastTapPoint;
  }
  return CGPointZero;
}

// Store the tap CGPoint in `_lastTapPoint` and the current timestamp.
- (void)saveContentAreaTapLocation:(UIGestureRecognizer*)gestureRecognizer {
  if (_isShutdown) {
    return;
  }
  UIView* view = gestureRecognizer.view;
  CGPoint viewCoordinate = [gestureRecognizer locationInView:view];
  _lastTapPoint = [[view superview] convertPoint:viewCoordinate
                                          toView:self.view];
  _lastTapTime = CACurrentMediaTime();
  // Last tap timestamp will be consumed by `IdleService` if IdleTimeout
  // policy is set.
  GetApplicationContext()->GetLocalState()->SetTime(
      enterprise_idle::prefs::kLastActiveTimestamp, base::Time::Now());
}

#pragma mark - ** Protocol Implementations and Helpers **

#pragma mark - Helpers

- (UIEdgeInsets)snapshotEdgeInsetsForNTPHelper:(NewTabPageTabHelper*)NTPHelper {
  UIEdgeInsets maxViewportInsets =
      self.fullscreenController->GetMaxViewportInsets();

  if (NTPHelper && NTPHelper->IsActive()) {
    // If the NTP is active, then it's used as the base view for snapshotting.
    // When the tab strip is visible, or for the incognito NTP, the NTP is laid
    // out between the toolbars, so it should not be inset while snapshotting.
    if (IsRegularXRegularSizeClass(self) || _isOffTheRecord) {
      return UIEdgeInsetsZero;
    }

    // For the regular NTP without tab strip, it sits above the bottom toolbar
    // but, since it is displayed as full-screen at the top, it requires maximum
    // viewport insets.
    maxViewportInsets.bottom = 0;
    return maxViewportInsets;
  } else {
    // If the NTP is inactive, the WebState's view is used as the base view for
    // snapshotting.  If fullscreen is implemented by resizing the scroll view,
    // then the WebState view is already laid out within the visible viewport
    // and doesn't need to be inset.  If fullscreen uses the content inset, then
    // the WebState view is laid out fullscreen and should be inset by the
    // viewport insets.
    return self.fullscreenController->ResizesScrollView() ? UIEdgeInsetsZero
                                                          : maxViewportInsets;
  }
}

#pragma mark - WebStateContainerViewProvider

- (UIView*)containerView {
  return self.contentArea;
}

- (CGPoint)dialogLocation {
  CGRect bounds = self.view.bounds;
  return CGPointMake(CGRectGetMidX(bounds),
                     CGRectGetMinY(bounds) + self.headerHeight);
}

#pragma mark - OmniboxPopupPresenterDelegate methods.

- (UIView*)popupParentViewForPresenter:(OmniboxPopupPresenter*)presenter {
  return self.view;
}

- (UIViewController*)popupParentViewControllerForPresenter:
    (OmniboxPopupPresenter*)presenter {
  return self;
}

- (UIColor*)popupBackgroundColorForPresenter:(OmniboxPopupPresenter*)presenter {
  ToolbarConfiguration* configuration = [[ToolbarConfiguration alloc]
      initWithStyle:_isOffTheRecord ? ToolbarStyle::kIncognito
                                    : ToolbarStyle::kNormal];
  return configuration.backgroundColor;
}

- (GuideName*)omniboxGuideNameForPresenter:(OmniboxPopupPresenter*)presenter {
  return kTopOmniboxGuide;
}

- (void)popupDidOpenForPresenter:(OmniboxPopupPresenter*)presenter {
  self.contentArea.accessibilityElementsHidden = YES;
  self.toolbarCoordinator.secondaryToolbarViewController.view
      .accessibilityElementsHidden = YES;
}

- (void)popupDidCloseForPresenter:(OmniboxPopupPresenter*)presenter {
  self.contentArea.accessibilityElementsHidden = NO;
  self.toolbarCoordinator.secondaryToolbarViewController.view
      .accessibilityElementsHidden = NO;
}

#pragma mark - FullscreenUIElement methods

- (void)updateForFullscreenProgress:(CGFloat)progress {
  [self updateHeadersForFullscreenProgress:progress];
  [self updateFootersForFullscreenProgress:progress];
  if (!ios::provider::IsFullscreenSmoothScrollingSupported()) {
    [self updateBrowserViewportForFullscreenProgress:progress];
  }
}

- (void)updateForFullscreenEnabled:(BOOL)enabled {
  if (!enabled)
    [self updateForFullscreenProgress:1.0];
}

- (void)animateFullscreenWithAnimator:(FullscreenAnimator*)animator {
  // If the headers are being hidden, it's possible that this will reveal a
  // portion of the webview beyond the top of the page's rendered content.  In
  // order to prevent that, update the top padding and content before the
  // animation begins.
  CGFloat finalProgress = animator.finalProgress;
  BOOL hidingHeaders = animator.finalProgress < animator.startProgress;
  if (hidingHeaders) {
    id<CRWWebViewProxy> webProxy = self.currentWebState->GetWebViewProxy();
    CRWWebViewScrollViewProxy* scrollProxy = webProxy.scrollViewProxy;
    CGPoint contentOffset = scrollProxy.contentOffset;
    if (contentOffset.y - scrollProxy.contentInset.top <
        webProxy.contentInset.top) {
      [self updateBrowserViewportForFullscreenProgress:finalProgress];
      contentOffset.y = -scrollProxy.contentInset.top;
      scrollProxy.contentOffset = contentOffset;
    }
  }

  // Add animations to update the headers and footers.
  __weak BrowserViewController* weakSelf = self;
  [animator addAnimations:^{
    [weakSelf updateHeadersForFullscreenProgress:finalProgress];
    [weakSelf updateFootersForFullscreenProgress:finalProgress];
  }];

  // Animating layout changes of the rendered content in the WKWebView is not
  // supported, so update the content padding in the completion block of the
  // animator to trigger a rerender in the page's new viewport.
  __weak FullscreenAnimator* weakAnimator = animator;
  [animator addCompletion:^(UIViewAnimatingPosition finalPosition) {
    [weakSelf updateBrowserViewportForFullscreenProgress:
                  [weakAnimator progressForAnimatingPosition:finalPosition]];
  }];
}

- (void)updateForFullscreenMinViewportInsets:(UIEdgeInsets)minViewportInsets
                           maxViewportInsets:(UIEdgeInsets)maxViewportInsets {
  [self updateForFullscreenProgress:self.fullscreenController->GetProgress()];
}

#pragma mark - FullscreenUIElement helpers

// The minimum amount by which the top toolbar overlaps the browser content
// area.
- (CGFloat)collapsedTopToolbarHeight {
  return self.rootSafeAreaInsets.top +
         self.toolbarCoordinator.collapsedPrimaryToolbarHeight;
}

// The minimum amount by which the bottom toolbar overlaps the browser content
// area.
- (CGFloat)collapsedBottomToolbarHeight {
  CGFloat height = self.toolbarCoordinator.collapsedSecondaryToolbarHeight;
  if (!height) {
    return 0.0;
  }
  // Height is non-zero only when bottom omnibox is enabled.
  return self.rootSafeAreaInsets.bottom + height;
}

// The maximum amount by which the top toolbar overlaps the browser content
// area.
- (CGFloat)expandedTopToolbarHeight {
  return [self primaryToolbarHeightWithInset] +
         (IsRegularXRegularSizeClass(self) ? self.tabStripView.frame.size.height
                                           : 0.0) +
         self.headerOffset;
}

// Updates the ToolbarUIState, which broadcasts any changes to registered
// listeners.
- (void)updateToolbarState {
  _toolbarUIState.collapsedTopToolbarHeight = [self collapsedTopToolbarHeight];
  _toolbarUIState.expandedTopToolbarHeight = [self expandedTopToolbarHeight];
  _toolbarUIState.collapsedBottomToolbarHeight =
      [self collapsedBottomToolbarHeight];
  _toolbarUIState.expandedBottomToolbarHeight =
      [self secondaryToolbarHeightWithInset];
}

// Returns the height difference between the fully expanded and fully collapsed
// primary toolbar.
- (CGFloat)primaryToolbarHeightDelta {
  CGFloat fullyExpandedHeight =
      self.fullscreenController->GetMaxViewportInsets().top;
  CGFloat fullyCollapsedHeight =
      self.fullscreenController->GetMinViewportInsets().top;
  return std::max(0.0, fullyExpandedHeight - fullyCollapsedHeight);
}

// Returns the height difference between the fully expanded and fully collapsed
// secondary toolbar.
- (CGFloat)secondaryToolbarHeightDelta {
  CGFloat fullyExpandedHeight =
      self.fullscreenController->GetMaxViewportInsets().bottom;
  CGFloat fullyCollapsedHeight =
      self.fullscreenController->GetMinViewportInsets().bottom;
  return std::max(0.0, fullyExpandedHeight - fullyCollapsedHeight);
}

// Translates the header views up and down according to `progress`, where a
// progress of 1.0 fully shows the headers and a progress of 0.0 fully hides
// them.
- (void)updateHeadersForFullscreenProgress:(CGFloat)progress {
  CGFloat offset =
      AlignValueToPixel((1.0 - progress) * [self primaryToolbarHeightDelta]);
  [self setFramesForHeaders:[self headerViews] atOffset:offset];
}

// Translates the footer view up and down according to `progress`, where a
// progress of 1.0 fully shows the footer and a progress of 0.0 fully hides it.
- (void)updateFootersForFullscreenProgress:(CGFloat)progress {
  self.footerFullscreenProgress = progress;

  // Don't update the height of the secondary toolbar if it is hidden.
  if (!IsSplitToolbarMode(self)) {
    return;
  }

  const CGFloat expandedToolbarHeight =
      self.fullscreenController->GetMaxViewportInsets().bottom;
  if (!expandedToolbarHeight) {
    // If `expandedToolbarHeight` is 0, secondary toolbar is hidden. In that
    // case don't update it's height on fullscreen progress.
    return;
  }

  const CGFloat offset =
      AlignValueToPixel((1.0 - progress) * [self secondaryToolbarHeightDelta]);
  // Update the height constraint and force a layout on the container view
  // so that the update is animatable.
  const CGFloat height = expandedToolbarHeight - offset;
  // Check that the computed height has a realistic value (crbug.com/1446068).
  DUMP_WILL_BE_CHECK(height >= (0.0 - FLT_EPSILON) &&
                     height <= (expandedToolbarHeight + FLT_EPSILON));

  self.secondaryToolbarHeightConstraint.constant = height;
}

// Updates the browser container view such that its viewport is the space
// between the primary and secondary toolbars.
- (void)updateBrowserViewportForFullscreenProgress:(CGFloat)progress {
  if (!self.currentWebState)
    return;

  // Calculate the heights of the toolbars for `progress`.  `-toolbarHeight`
  // returns the height of the toolbar extending below this view controller's
  // safe area, so the unsafe top height must be added.
  CGFloat top = AlignValueToPixel(
      self.headerHeight + (progress - 1.0) * [self primaryToolbarHeightDelta]);
  CGFloat bottom =
      AlignValueToPixel([self secondaryToolbarHeightWithInset] +
                        (progress - 1.0) * [self secondaryToolbarHeightDelta]);

  [self updateContentPaddingForTopToolbarHeight:top bottomToolbarHeight:bottom];
}

// Updates the padding of the web view proxy. This either resets the frame of
// the WKWebView or the contentInsets of the WKWebView's UIScrollView, depending
// on the the proxy's `shouldUseViewContentInset` property.
- (void)updateContentPaddingForTopToolbarHeight:(CGFloat)topToolbarHeight
                            bottomToolbarHeight:(CGFloat)bottomToolbarHeight {
  if (!self.currentWebState)
    return;

  id<CRWWebViewProxy> webViewProxy = self.currentWebState->GetWebViewProxy();
  UIEdgeInsets contentPadding = webViewProxy.contentInset;
  contentPadding.top = topToolbarHeight;
  contentPadding.bottom = bottomToolbarHeight;
  webViewProxy.contentInset = contentPadding;
}

- (CGFloat)currentHeaderOffset {
  NSArray<HeaderDefinition*>* headers = [self headerViews];
  if (!headers.count)
    return 0.0;

  // Prerender tab does not have a toolbar, return `headerHeight` as promised by
  // API documentation.
  if ([self.toolbarCoordinator isLoadingPrerenderer]) {
    return self.headerHeight;
  }

  UIView* topHeader = headers[0].view;
  return -(topHeader.frame.origin.y - self.headerOffset);
}

#pragma mark - MainContentUI

- (MainContentUIState*)mainContentUIState {
  return _mainContentUIUpdater.state;
}

#pragma mark - OmniboxFocusDelegate (Public)

- (void)omniboxDidBecomeFirstResponder {
  if (self.ntpCoordinator.isNTPActiveForCurrentWebState) {
    [self.ntpCoordinator locationBarDidBecomeFirstResponder];
  }
  [_sideSwipeMediator setEnabled:NO];

  if (!IsVisibleURLNewTabPage(self.currentWebState) ||
      ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    // Tapping on web content area should dismiss the keyboard. Tapping on NTP
    // gesture should propagate to NTP view.
    [self.view insertSubview:self.typingShield aboveSubview:self.contentArea];
    [self.typingShield setAlpha:0.0];
    [self.typingShield setHidden:NO];
    [UIView animateWithDuration:0.3
                     animations:^{
                       [self.typingShield setAlpha:1.0];
                     }];
  }

  [self.toolbarCoordinator transitionToLocationBarFocusedState:YES
                                                    completion:nil];
}

- (void)omniboxDidResignFirstResponder {
  [_sideSwipeMediator setEnabled:YES];

  [self.ntpCoordinator locationBarWillResignFirstResponder];

  [UIView animateWithDuration:0.3
      animations:^{
        [self.typingShield setAlpha:0.0];
      }
      completion:^(BOOL finished) {
        // This can happen if one quickly resigns the omnibox and then taps
        // on the omnibox again during this animation. If the animation is
        // interrupted and the toolbar controller is first responder, it's safe
        // to assume `self.typingShield` shouldn't be hidden here.
        if (!finished && [self.toolbarCoordinator isOmniboxFirstResponder]) {
          return;
        }
        [self.typingShield setHidden:YES];
      }];

  ProceduralBlock completion = ^{
    // Show the NTP's fake toolbar after the defocus animation completes.
    [self.ntpCoordinator locationBarDidResignFirstResponder];
  };

  [self.toolbarCoordinator transitionToLocationBarFocusedState:NO
                                                    completion:completion];
}

#pragma mark - BrowserCommands

- (void)dismissSoftKeyboard {
  DCHECK(self.visible || self.dismissingModal);
  [self.viewForCurrentWebState endEditing:NO];
}

#pragma mark - TabConsumer (Public)

- (void)resetTab {
  self.browserContainerViewController.contentView = nil;
}

- (void)prepareForNewTabAnimation {
  [self dismissPopups];
}

- (void)webStateSelected {
  // Ignore changes while the tab stack view is visible (or while suspended).
  // The display will be refreshed when this view becomes active again.
  if (!self.visible || !self.webUsageEnabled) {
    return;
  }

  if (!self.viewForCurrentWebState) {
    return;
  }

  [self displayTabView];
  if (!self.inNewTabAnimation) {
    _pagePlaceholderBrowserAgent->CancelPagePlaceholder();
  }
}

- (void)initiateNewTabForegroundAnimationForWebState:(web::WebState*)webState {
  // Initiates the new tab foreground animation, which is phone-specific.
  if (IsRegularXRegularSizeClass(self)) {
    if (self.foregroundTabWasAddedCompletionBlock) {
      // This callback is called before webState is activated. Dispatch the
      // callback asynchronously to be sure the activation is complete.
      __weak BrowserViewController* weakSelf = self;
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(^{
            [weakSelf executeAndClearForegroundTabWasAddedCompletionBlock:YES];
          }));
    }
    return;
  }
  // Do nothing if browsing is currently suspended.  The BVC will set everything
  // up correctly when browsing resumes.
  if (!self.visible || !self.webUsageEnabled) {
    return;
  }

  self.inNewTabAnimation = YES;
  __weak __typeof(self) weakSelf = self;
  [self animateNewTabForWebState:webState
      inForegroundWithCompletion:^{
        [weakSelf startVoiceSearchIfNecessary];
      }];
}

- (void)initiateNewTabBackgroundAnimation {
  if (self.foregroundTabWasAddedCompletionBlock) {
    // This callback is called before webState is activated. Dispatch the
    // callback asynchronously to be sure the activation is complete.
    __weak BrowserViewController* weakSelf = self;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(^{
          [weakSelf executeAndClearForegroundTabWasAddedCompletionBlock:NO];
        }));
  }
  self.inNewTabAnimation = NO;
}

- (void)displayTabViewIfActive {
  if (self.active) {
    [self displayTabView];
  }
}

- (void)switchToTabAnimationPosition:(SwitchToTabAnimationPosition)position
                   snapshotTabHelper:(SnapshotTabHelper*)snapshotTabHelper
                  willAddPlaceholder:(BOOL)willAddPlaceholder
                 newTabPageTabHelper:(NewTabPageTabHelper*)NTPHelper
                     topToolbarImage:(UIImage*)topToolbarImage
                  bottomToolbarImage:(UIImage*)bottomToolbarImage {
  if (IsRegularXRegularSizeClass(self)) {
    return;
  }

  // Add animations only if the tab strip isn't shown.
  UIView* snapshotView = [self.view snapshotViewAfterScreenUpdates:NO];

  SwipeView* swipeView = [[SwipeView alloc]
      initWithFrame:self.contentArea.frame
          topMargin:[self snapshotEdgeInsetsForNTPHelper:NTPHelper].top];

  [swipeView setTopToolbarImage:topToolbarImage];
  [swipeView setBottomToolbarImage:bottomToolbarImage];

  snapshotTabHelper->RetrieveColorSnapshot(^(UIImage* image) {
    willAddPlaceholder ? [swipeView setImage:nil] : [swipeView setImage:image];
  });

  SwitchToTabAnimationView* animationView =
      [[SwitchToTabAnimationView alloc] initWithFrame:self.view.bounds];
  [self.view addSubview:animationView];

  [animationView animateFromCurrentView:snapshotView
                              toNewView:swipeView
                             inPosition:position];
}

- (void)dismissBookmarkModalController {
  [_bookmarksCoordinator dismissBookmarkModalControllerAnimated:YES];
}

#pragma mark - TabConsumer helpers

// Helper which execute and then clears `foregroundTabWasAddedCompletionBlock`
// if it is still set, or does nothing.
- (void)executeAndClearForegroundTabWasAddedCompletionBlock:(BOOL)animated {
  // Test existence again as the block may have been deleted.
  ProceduralBlock completion = self.foregroundTabWasAddedCompletionBlock;
  if (!completion)
    return;

  // Clear the property before executing the completion, in case the
  // completion calls appendTabAddedCompletion:tabAddedCompletion.
  // Clearing the property after running the completion would cause any
  // newly appended completion to be immediately cleared without ever
  // getting run. An example where this would happen is when opening
  // multiple tabs via the "Open URLs in Chrome" Siri Shortcut.
  self.foregroundTabWasAddedCompletionBlock = nil;
  if (animated) {
    completion();
  } else {
    [UIView performWithoutAnimation:^{
      completion();
    }];
  }
}

// Helper which starts voice search at the end of new Tab animation if
// necessary.
- (void)startVoiceSearchIfNecessary {
  if (_startVoiceSearchAfterNewTabAnimation) {
    _startVoiceSearchAfterNewTabAnimation = NO;
    [self startVoiceSearch];
    [IntentDonationHelper donateIntent:IntentType::kOpenVoiceSearch];
  }
}

- (void)animateNewTabForWebState:(web::WebState*)webState
      inForegroundWithCompletion:(ProceduralBlock)completion {
  // Create the new page image, and load with the new tab snapshot except if
  // it is the NTP.
  UIView* newPage = [self viewForWebState:webState];
  DCHECK(newPage);
  GURL tabURL = webState->GetVisibleURL();
  // Toolbar snapshot is only used for the UIRefresh animation.
  UIView* toolbarSnapshot;

  if (tabURL == kChromeUINewTabURL && !_isOffTheRecord &&
      !IsRegularXRegularSizeClass(self)) {
    // Add a snapshot of the primary toolbar to the background as the
    // animation runs.
    UIViewController* toolbarViewController =
        self.toolbarCoordinator.primaryToolbarViewController;
    toolbarSnapshot =
        [toolbarViewController.view snapshotViewAfterScreenUpdates:NO];
    toolbarSnapshot.frame = [self.contentArea convertRect:toolbarSnapshot.frame
                                                 fromView:self.view];
    [self.contentArea addSubview:toolbarSnapshot];
    newPage.frame = self.view.bounds;
  } else {
    if (self.ntpCoordinator.isNTPActiveForCurrentWebState &&
        self.webUsageEnabled) {
      newPage.frame = [self ntpFrameForCurrentWebState];
    } else {
      newPage.frame = self.contentArea.bounds;
    }
  }
  newPage.userInteractionEnabled = NO;
  NSInteger currentAnimationIdentifier = ++_NTPAnimationIdentifier;

  // Cleanup steps needed for both UI Refresh and stack-view style animations.
  UIView* webStateView = [self viewForWebState:webState];
  __weak __typeof(self) weakSelf = self;
  auto commonCompletion = ^{
    __strong __typeof(self) strongSelf = weakSelf;
    newPage.userInteractionEnabled = YES;

    // Check for nil because we need to access an ivar below.
    if (!strongSelf) {
      return;
    }

    // Do not resize the same view.
    if (webStateView != newPage)
      webStateView.frame = strongSelf.contentArea.bounds;

    if (currentAnimationIdentifier != strongSelf->_NTPAnimationIdentifier) {
      // Prevent the completion block from being executed if a new animation has
      // started in between. `self.foregroundTabWasAddedCompletionBlock` isn't
      // called because it is overridden when a new animation is started.
      // Calling it here would call the block from the lastest animation that
      // haved started.
      return;
    }

    strongSelf.inNewTabAnimation = NO;

    [strongSelf webStateSelected];
    if (completion)
      completion();

    [strongSelf executeAndClearForegroundTabWasAddedCompletionBlock:YES];
  };

  CGPoint origin = [self lastTapPoint];

  CGRect frame = [self.contentArea convertRect:self.view.bounds
                                      fromView:self.view];
  ForegroundTabAnimationView* animatedView =
      [[ForegroundTabAnimationView alloc] initWithFrame:frame];
  animatedView.contentView = newPage;
  __weak UIView* weakAnimatedView = animatedView;
  auto completionBlock = ^() {
    [weakAnimatedView removeFromSuperview];
    [toolbarSnapshot removeFromSuperview];
    commonCompletion();
  };
  [self.contentArea addSubview:animatedView];
  [animatedView animateFrom:origin withCompletion:completionBlock];
}

#pragma mark - IncognitoReauthConsumer

- (void)setItemsRequireAuthentication:(BOOL)require {
  _itemsRequireAuthentication = require;
  if (require) {
    if (!self.blockingView) {
      self.blockingView = [[IncognitoReauthView alloc] init];
      self.blockingView.translatesAutoresizingMaskIntoConstraints = NO;
      self.blockingView.layer.zPosition = FLT_MAX;

      DCHECK(self.reauthHandler);
      [self.blockingView.authenticateButton
                 addTarget:self.reauthHandler
                    action:@selector(authenticateIncognitoContent)
          forControlEvents:UIControlEventTouchUpInside];

      DCHECK(self.applicationCommandsHandler);
      __weak __typeof(self) weakSelf = self;
      [self.blockingView.tabSwitcherButton
                 addAction:[UIAction actionWithHandler:^(UIAction* action) {
                   [weakSelf.applicationCommandsHandler
                       displayTabGridInMode:TabGridOpeningMode::kRegular];
                 }]
          forControlEvents:UIControlEventTouchUpInside];
    }

    [self.view addSubview:self.blockingView];
    AddSameConstraints(self.view, self.blockingView);
    self.blockingView.alpha = 1;
    [self.omniboxCommandsHandler cancelOmniboxEdit];
    // Resign the first responder. This achieves multiple goals:
    // 1. The keyboard is dismissed.
    // 2. Hardware keyboard events (such as space to scroll) will be ignored.
    UIResponder* firstResponder = GetFirstResponder();
    [firstResponder resignFirstResponder];
    // Close presented view controllers, e.g. share sheets.
    if (self.presentedViewController) {
      [self.applicationCommandsHandler dismissModalDialogsWithCompletion:nil];
    }

  } else {
    [UIView animateWithDuration:0.2
        animations:^{
          self.blockingView.alpha = 0;
        }
        completion:^(BOOL finished) {
          // In an extreme case, this method can be called twice in quick
          // succession, before the animation completes. Check if the blocking
          // UI should be shown or the animation needs to be rolled back.
          if (self->_itemsRequireAuthentication) {
            self.blockingView.alpha = 1;
          } else {
            [self.blockingView removeFromSuperview];
          }
        }];
  }
}

#pragma mark - UIGestureRecognizerDelegate

// Always return yes, as this tap should work with various recognizers,
// including UITextTapRecognizer, UILongPressGestureRecognizer,
// UIScrollViewPanGestureRecognizer and others.
- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
    shouldRecognizeSimultaneouslyWithGestureRecognizer:
        (UIGestureRecognizer*)otherGestureRecognizer {
  return YES;
}

// Tap gestures should only be recognized within `contentArea`.
- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer*)gesture {
  CGPoint location = [gesture locationInView:self.view];

  // Only allow touches on descendant views of `contentArea`.
  UIView* hitView = [self.view hitTest:location withEvent:nil];
  return [hitView isDescendantOfView:self.contentArea];
}

// TODO(crbug.com/40842427): Factor this delegate into a mediator or other
// helper
#pragma mark - SideSwipeMediatorDelegate

- (void)sideSwipeViewDismissAnimationDidEnd:(UIView*)sideSwipeView {
  DCHECK(!IsRegularXRegularSizeClass(self));
  // TODO(crbug.com/40842406): Signal to the toolbar coordinator to perform this
  // update. Longer-term, make SideSwipeMediatorDelegate observable instead of
  // delegating.
  [self.toolbarCoordinator updateToolbar];

  // Reset horizontal stack view.
  [sideSwipeView removeFromSuperview];
  [_sideSwipeMediator setInSwipe:NO];
}

- (UIView*)sideSwipeContentView {
  return self.contentArea;
}

- (void)sideSwipeRedisplayTabView {
  [self displayTabView];
}

// TODO(crbug.com/40842427): Federate side swipe logic.
- (BOOL)preventSideSwipe {
  if ([self.popupMenuCoordinator isShowingPopupMenu])
    return YES;

  if (_voiceSearchController.visible)
    return YES;

  if (!self.active)
    return YES;

  BOOL isShowingIncognitoBlocker = (self.blockingView.superview != nil);
  if (isShowingIncognitoBlocker) {
    return YES;
  }

  return NO;
}

- (void)updateAccessoryViewsForSideSwipeWithVisibility:(BOOL)visible {
  if (visible) {
    // TODO(crbug.com/40842406): Signal to the toolbar coordinator to perform
    // this update. Longer-term, make SideSwipeMediatorDelegate observable
    // instead of delegating.
    [self.toolbarCoordinator updateToolbar];
  } else {
    // Hide UI accessories such as find bar and first visit overlays
    // for welcome page.
    [self.findInPageCommandsHandler hideFindUI];
    [self.textZoomHandler hideTextZoomUI];
  }
}

- (CGFloat)headerHeightForSideSwipe {
  // If the toolbar is hidden, only inset the side swipe navigation view by
  // `safeAreaInsets.top`.  Otherwise insetting by `self.headerHeight` would
  // show a grey strip where the toolbar would normally be.
  if (self.toolbarCoordinator.primaryToolbarViewController.view.hidden) {
    return self.rootSafeAreaInsets.top;
  }
  return self.headerHeight;
}

- (BOOL)canBeginToolbarSwipe {
  return ![self.toolbarCoordinator isOmniboxFirstResponder] &&
         ![self.toolbarCoordinator showingOmniboxPopup];
}

- (UIView*)topToolbarView {
  return self.toolbarCoordinator.primaryToolbarViewController.view;
}

#pragma mark - ToolbarHeightDelegate

- (void)toolbarsHeightChanged {
  // Toolbar state must be updated before `updateForFullscreenProgress` as the
  // later uses the insets from fullscreen model.
  [self updateToolbarState];

  self.primaryToolbarHeightConstraint.constant =
      [self primaryToolbarHeightWithInset];
  self.secondaryToolbarHeightConstraint.constant =
      [self secondaryToolbarHeightWithInset];
  [self updateForFullscreenProgress:self.footerFullscreenProgress];
}

- (void)secondaryToolbarMovedAboveKeyboard {
  // Lower the height constraint priority, allowing UIKeyboardLayoutGuide to
  // move the toolbar above the keyboard.
  self.secondaryToolbarHeightConstraint.priority = UILayoutPriorityDefaultHigh;
}

- (void)secondaryToolbarRemovedFromKeyboard {
  // Return to required priority, otherwise UIKeyboardLayoutGuide would set the
  // toolbar minimum height to the bottom safe area.
  self.secondaryToolbarHeightConstraint.priority = UILayoutPriorityRequired - 1;
}

#pragma mark - LogoAnimationControllerOwnerOwner (Public)

- (id<LogoAnimationControllerOwner>)logoAnimationControllerOwner {
  return nil;
}

#pragma mark - TabStripPresentation

- (BOOL)isTabStripFullyVisible {
  return ([self currentHeaderOffset] == 0.0f);
}

- (void)showTabStripView:(UIView*)tabStripView {
  DCHECK([self isViewLoaded]);
  DCHECK(tabStripView);
  self.tabStripView = tabStripView;
  CGRect tabStripFrame = [self.tabStripView frame];
  tabStripFrame.origin = CGPointZero;
  // TODO(crbug.com/41023322): Move the origin.y below to -setUpViewLayout.
  // because the CGPointZero above will break reset the offset, but it's not
  // clear what removing that will do.
  tabStripFrame.origin.y = self.headerOffset;
  tabStripFrame.size.width = CGRectGetWidth([self view].bounds);
  [self.tabStripView setFrame:tabStripFrame];

  if (IsModernTabStripOrRaccoonEnabled()) {
    UIView* primaryToolbar =
        self.toolbarCoordinator.primaryToolbarViewController.view;
    [self.view insertSubview:tabStripView belowSubview:primaryToolbar];
  } else {
    [self.view addSubview:tabStripView];
  }
}

#pragma mark - FindBarPresentationDelegate

- (void)setHeadersForFindBarCoordinator:
    (FindBarCoordinator*)findBarCoordinator {
  [self setFramesForHeaders:[self headerViews]
                   atOffset:[self currentHeaderOffset]];
}

- (void)findBarDidAppearForFindBarCoordinator:
    (FindBarCoordinator*)findBarCoordinator {
  // When the Find bar is presented, hide underlying elements from VoiceOver.
  self.contentArea.accessibilityElementsHidden = YES;
  self.toolbarCoordinator.primaryToolbarViewController.view
      .accessibilityElementsHidden = YES;
  self.toolbarCoordinator.secondaryToolbarViewController.view
      .accessibilityElementsHidden = YES;
}

- (void)findBarDidDisappearForFindBarCoordinator:
    (FindBarCoordinator*)findBarCoordinator {
  // When the Find bar is dismissed, show underlying elements to VoiceOver.
  self.contentArea.accessibilityElementsHidden = NO;
  self.toolbarCoordinator.primaryToolbarViewController.view
      .accessibilityElementsHidden = NO;
  self.toolbarCoordinator.secondaryToolbarViewController.view
      .accessibilityElementsHidden = NO;
}

#pragma mark - LensPresentationDelegate

- (CGRect)webContentAreaForLensCoordinator:(LensCoordinator*)lensCoordinator {
  DCHECK(lensCoordinator);

  // The LensCoordinator needs the content area of the webView with the
  // header and footer toolbars visible.
  UIEdgeInsets viewportInsets = self.rootSafeAreaInsets;
  if (!IsRegularXRegularSizeClass(self)) {
    viewportInsets.bottom = [self secondaryToolbarHeightWithInset];
  }

  viewportInsets.top = [self expandedTopToolbarHeight];
  return UIEdgeInsetsInsetRect(self.contentArea.bounds, viewportInsets);
}

#pragma mark - ContextualSheetPresenter

- (void)insertContextualSheet:(UIView*)contextualSheet {
  [self.view
      insertSubview:contextualSheet
       aboveSubview:self.toolbarCoordinator.primaryToolbarViewController.view];
}

@end
