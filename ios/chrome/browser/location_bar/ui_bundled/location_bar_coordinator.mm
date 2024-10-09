// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_coordinator.h"

#import "base/memory/ptr_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/omnibox/browser/location_bar_model_impl.h"
#import "components/omnibox/browser/omnibox_edit_model.h"
#import "components/omnibox/browser/omnibox_view.h"
#import "components/open_from_clipboard/clipboard_recent_content.h"
#import "components/prefs/pref_service.h"
#import "components/profile_metrics/browser_profile_type.h"
#import "components/search_engines/util.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_scheme_classifier_impl.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_button_factory.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_delegate.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_mediator.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_view_controller.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_view_visibility_delegate.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/coordinator/contextual_panel_entrypoint_coordinator.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/coordinator/contextual_panel_entrypoint_coordinator_delegate.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/ui/contextual_panel_entrypoint_visibility_delegate.h"
#import "ios/chrome/browser/default_browser/model/default_browser_interest_signals.h"
#import "ios/chrome/browser/drag_and_drop/model/drag_item_util.h"
#import "ios/chrome/browser/drag_and_drop/model/url_drag_drop_handler.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/infobars/model/infobar_metrics_recorder.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_availability.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_constants.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_consumer.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_mediator.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_model_delegate_ios.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_steady_view_consumer.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_steady_view_mediator.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_url_loader.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_view_controller.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/public/commands/load_query_commands.h"
#import "ios/chrome/browser/shared/public/commands/search_image_with_lens_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/pasteboard_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_updater.h"
#import "ios/chrome/browser/ui/lens/lens_entrypoint.h"
#import "ios/chrome/browser/ui/omnibox/chrome_omnibox_client_ios.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_controller_delegate.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_coordinator.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_focus_delegate.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_coordinator.h"
#import "ios/chrome/browser/ui/omnibox/web_location_bar_impl.h"
#import "ios/chrome/browser/url_loading/model/image_search_param_generator.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/url_loading/model/url_loading_util.h"
#import "ios/chrome/browser/web/model/web_navigation_util.h"
#import "ios/public/provider/chrome/browser/lens/lens_api.h"
#import "ios/public/provider/chrome/browser/voice_search/voice_search_api.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/web_state.h"
#import "services/network/public/cpp/resource_request.h"
#import "ui/base/device_form_factor.h"
#import "url/gurl.h"

namespace {
const size_t kMaxURLDisplayChars = 32 * 1024;
}  // namespace

@interface LocationBarCoordinator () <
    ContextualPanelEntrypointCoordinatorDelegate,
    LoadQueryCommands,
    LocationBarViewControllerDelegate,
    LocationBarSteadyViewConsumer,
    OmniboxControllerDelegate,
    URLDragDataSource> {
  // API endpoint for omnibox.
  std::unique_ptr<WebLocationBarImpl> _locationBar;
  // Observer that updates `viewController` for fullscreen events.
  std::unique_ptr<FullscreenUIUpdater> _omniboxFullscreenUIUpdater;
  // Observer that updates BadgeViewController for fullscreen events.
  std::unique_ptr<FullscreenUIUpdater> _badgeFullscreenUIUpdater;

  // Facade objects used by `_toolbarCoordinator`.
  // Must outlive `_toolbarCoordinator`.
  std::unique_ptr<LocationBarModelDelegateIOS> _locationBarModelDelegate;
  std::unique_ptr<LocationBarModel> _locationBarModel;
}
// Whether the coordinator is started.
@property(nonatomic, assign, getter=isStarted) BOOL started;
// Mediator for the badges displayed in the LocationBar.
@property(nonatomic, strong) BadgeMediator* badgeMediator;
// ViewController for the badges displayed in the LocationBar.
@property(nonatomic, strong) BadgeViewController* badgeViewController;
// Coordinator for the contextual panel entrypoint.
@property(nonatomic, strong)
    ContextualPanelEntrypointCoordinator* contextualPanelEntrypointCoordinator;
// Coordinator for the omnibox.
@property(nonatomic, strong) OmniboxCoordinator* omniboxCoordinator;
@property(nonatomic, strong) LocationBarMediator* mediator;
@property(nonatomic, strong) LocationBarSteadyViewMediator* steadyViewMediator;
@property(nonatomic, strong) LocationBarViewController* viewController;
@property(nonatomic, readonly) ProfileIOS* profile;
@property(nonatomic, readonly) WebStateList* webStateList;

// Tracks calls in progress to -cancelOmniboxEdit to avoid calling it from
// itself when -resignFirstResponder causes -textFieldWillResignFirstResponder
// delegate call.
@property(nonatomic, assign) BOOL isCancellingOmniboxEdit;

// Handler for URL drag interactions.
@property(nonatomic, strong) URLDragDropHandler* dragDropHandler;
@end

@implementation LocationBarCoordinator

#pragma mark - Accessors

- (ProfileIOS*)profile {
  return self.browser ? self.browser->GetProfile() : nullptr;
}

- (WebStateList*)webStateList {
  return self.browser ? self.browser->GetWebStateList() : nullptr;
}

#pragma mark - Public

- (UIViewController*)locationBarViewController {
  return self.viewController;
}

- (instancetype)initWithBrowser:(Browser*)browser {
  CHECK(browser);
  return [super initWithBaseViewController:nil browser:browser];
}

- (void)start {
  DCHECK(self.browser);

  if (self.started)
    return;

  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(OmniboxCommands)];
  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(LoadQueryCommands)];

  BOOL isIncognito = self.profile->IsOffTheRecord();

  self.viewController = [[LocationBarViewController alloc] init];
  self.viewController.incognito = isIncognito;
  self.viewController.delegate = self;
  // TODO(crbug.com/40670043): Use HandlerForProtocol after commands protocol
  // clean up.
  self.viewController.dispatcher =
      static_cast<id<ActivityServiceCommands, ApplicationCommands,
                     LoadQueryCommands, LensOverlayCommands, OmniboxCommands>>(
          self.browser->GetCommandDispatcher());
  self.viewController.tracker =
      feature_engagement::TrackerFactory::GetForProfile(
          self.browser->GetProfile());
  self.viewController.voiceSearchEnabled =
      ios::provider::IsVoiceSearchEnabled();
  self.viewController.layoutGuideCenter =
      LayoutGuideCenterForBrowser(self.browser);
  id<HelpCommands> helpHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), HelpCommands);
  [self.viewController setHelpCommandsHandler:helpHandler];

  _locationBar = std::make_unique<WebLocationBarImpl>(self);
  _locationBar->SetURLLoader(self);
  _locationBarModelDelegate.reset(new LocationBarModelDelegateIOS(
      self.browser->GetWebStateList(), self.profile));
  _locationBarModel = std::make_unique<LocationBarModelImpl>(
      _locationBarModelDelegate.get(), kMaxURLDisplayChars);

  self.omniboxCoordinator = [[OmniboxCoordinator alloc]
      initWithBaseViewController:nil
                         browser:self.browser
                   omniboxClient:std::make_unique<ChromeOmniboxClientIOS>(
                                     _locationBar.get(), self.profile,
                                     feature_engagement::TrackerFactory::
                                         GetForProfile(self.profile))
                   isLensOverlay:NO];
  self.omniboxCoordinator.focusDelegate = self.delegate;

  self.omniboxCoordinator.presenterDelegate = self.popupPresenterDelegate;
  [self.omniboxCoordinator start];

  [self.omniboxCoordinator.managedViewController
      willMoveToParentViewController:self.viewController];
  [self.viewController
      addChildViewController:self.omniboxCoordinator.managedViewController];
  [self.viewController setEditView:self.omniboxCoordinator.editView];
  [self.omniboxCoordinator.managedViewController
      didMoveToParentViewController:self.viewController];
  self.viewController.offsetProvider = [self.omniboxCoordinator offsetProvider];

  if (!isIncognito && IsContextualPanelEnabled()) {
    self.contextualPanelEntrypointCoordinator =
        [[ContextualPanelEntrypointCoordinator alloc]
            initWithBaseViewController:self.viewController
                               browser:self.browser];
    self.contextualPanelEntrypointCoordinator.delegate = self;
    self.contextualPanelEntrypointCoordinator.visibilityDelegate =
        self.viewController.contextualEntrypointVisibilityDelegate;
    [self.contextualPanelEntrypointCoordinator start];
    [self.viewController
        addChildViewController:self.contextualPanelEntrypointCoordinator
                                   .viewController];
    [self.viewController
        setContextualPanelEntrypointView:
            self.contextualPanelEntrypointCoordinator.viewController.view];
    [self.contextualPanelEntrypointCoordinator.viewController
        didMoveToParentViewController:self.viewController];
  }

  // Create button factory that wil be used by the ViewController to get
  // BadgeButtons for a BadgeType.
  BadgeButtonFactory* buttonFactory = [[BadgeButtonFactory alloc] init];
  self.badgeViewController =
      [[BadgeViewController alloc] initWithButtonFactory:buttonFactory];
  self.badgeViewController.layoutGuideCenter =
      LayoutGuideCenterForBrowser(self.browser);
  self.badgeViewController.visibilityDelegate =
      [self.viewController badgeViewVisibilityDelegate];
  [self.viewController addChildViewController:self.badgeViewController];
  [self.viewController setBadgeView:self.badgeViewController.view];
  [self.badgeViewController didMoveToParentViewController:self.viewController];
  // Create BadgeMediator and set the viewController as its consumer.
  OverlayPresenter* overlayPresenter = OverlayPresenter::FromBrowser(
      self.browser, OverlayModality::kInfobarBanner);
  self.badgeMediator =
      [[BadgeMediator alloc] initWithWebStateList:self.webStateList
                                 overlayPresenter:overlayPresenter
                                      isIncognito:isIncognito];
  self.badgeMediator.consumer = self.badgeViewController;
  // TODO(crbug.com/40670043): Use HandlerForProtocol after commands protocol
  // clean up.
  self.badgeMediator.dispatcher = static_cast<id<BrowserCoordinatorCommands>>(
      self.browser->GetCommandDispatcher());
  buttonFactory.delegate = self.badgeMediator;
  FullscreenController* fullscreenController =
      FullscreenController::FromBrowser(self.browser);
  _badgeFullscreenUIUpdater = std::make_unique<FullscreenUIUpdater>(
      fullscreenController, self.badgeViewController);

  self.mediator = [[LocationBarMediator alloc] initWithIsIncognito:isIncognito];
  self.mediator.templateURLService =
      ios::TemplateURLServiceFactory::GetForProfile(self.profile);
  self.mediator.consumer = self.viewController;
  self.mediator.webStateList = self.webStateList;

  self.steadyViewMediator = [[LocationBarSteadyViewMediator alloc]
      initWithLocationBarModel:[self locationBarModel]];
  self.steadyViewMediator.webStateList = self.browser->GetWebStateList();
  self.steadyViewMediator.webContentAreaOverlayPresenter =
      OverlayPresenter::FromBrowser(self.browser,
                                    OverlayModality::kWebContentArea);
  self.steadyViewMediator.consumer = self;
  self.steadyViewMediator.tracker =
      feature_engagement::TrackerFactory::GetForBrowserState(
          self.browser->GetProfile());

  _omniboxFullscreenUIUpdater = std::make_unique<FullscreenUIUpdater>(
      fullscreenController, self.viewController);

  self.started = YES;

  [self setUpDragAndDrop];
}

- (void)stop {
  if (!self.started)
    return;
  [self.browser->GetCommandDispatcher() stopDispatchingToTarget:self];

  [self.contextualPanelEntrypointCoordinator stop];
  self.contextualPanelEntrypointCoordinator.delegate = nil;
  self.contextualPanelEntrypointCoordinator = nil;

  // The popup has to be destroyed before the location bar.
  [self.omniboxCoordinator stop];
  [self.badgeMediator disconnect];
  self.badgeMediator = nil;
  _locationBar.reset();

  self.viewController = nil;
  [self.mediator disconnect];
  self.mediator.templateURLService = nil;
  self.mediator.consumer = nil;
  self.mediator = nil;
  [self.steadyViewMediator disconnect];
  self.steadyViewMediator.webStateList = nullptr;
  self.steadyViewMediator.webContentAreaOverlayPresenter = nil;
  self.steadyViewMediator.consumer = nil;
  self.steadyViewMediator = nil;

  _locationBarModel = nullptr;
  _locationBarModelDelegate = nullptr;

  _badgeFullscreenUIUpdater = nullptr;
  _omniboxFullscreenUIUpdater = nullptr;
  self.started = NO;
}

- (BOOL)omniboxPopupHasAutocompleteResults {
  return self.omniboxCoordinator.popupCoordinator.hasResults;
}

- (BOOL)showingOmniboxPopup {
  return self.omniboxCoordinator.popupCoordinator.isOpen;
}

- (BOOL)isOmniboxFirstResponder {
  return [self.omniboxCoordinator isOmniboxFirstResponder];
}

- (id<LocationBarAnimatee>)locationBarAnimatee {
  return self.viewController;
}

- (id<EditViewAnimatee>)editViewAnimatee {
  return self.omniboxCoordinator.animatee;
}

- (UIResponder<UITextInput>*)omniboxScribbleForwardingTarget {
  return self.omniboxCoordinator.scribbleInput;
}

// Returns the toolbar omnibox consumer.
- (id<ToolbarOmniboxConsumer>)toolbarOmniboxConsumer {
  return self.omniboxCoordinator.toolbarOmniboxConsumer;
}

#pragma mark - LoadQueryCommands

- (void)loadQuery:(NSString*)query immediately:(BOOL)immediately {
  DCHECK(query);
  // Since the query is not user typed, sanitize it to make sure it's safe.
  std::u16string sanitizedQuery =
      OmniboxView::SanitizeTextForPaste(base::SysNSStringToUTF16(query));
  if (immediately) {
    [self loadURLForQuery:sanitizedQuery];
  } else {
    [self focusOmnibox];
    [self.omniboxCoordinator
        insertTextToOmnibox:base::SysUTF16ToNSString(sanitizedQuery)];
  }
}

#pragma mark - LocationBarURLLoader

- (void)loadGURLFromLocationBar:(const GURL&)url
                               postContent:
                                   (TemplateURLRef::PostContent*)postContent
                                transition:(ui::PageTransition)transition
                               disposition:(WindowOpenDisposition)disposition
    destination_url_entered_without_scheme:
        (bool)destination_url_entered_without_scheme {
  if (url.SchemeIs(url::kJavaScriptScheme)) {
    LoadJavaScriptURL(url, self.profile,
                      self.webStateList->GetActiveWebState());
  } else {
    // TODO(crbug.com/40550038): Is it ok to call `cancelOmniboxEdit` after
    // `loadURL|?  It doesn't seem to be causing major problems.  If we call
    // cancel before load, then any prerendered pages get destroyed before the
    // call to load.
    web::NavigationManager::WebLoadParams web_params =
        web_navigation_util::CreateWebLoadParams(url, transition, postContent);
    if (destination_url_entered_without_scheme) {
      web_params.https_upgrade_type = web::HttpsUpgradeType::kOmnibox;
    }
    NSMutableDictionary<NSString*, NSString*>* combinedExtraHeaders =
        [web_navigation_util::VariationHeadersForURL(
            url, self.profile->IsOffTheRecord()) mutableCopy];
    [combinedExtraHeaders addEntriesFromDictionary:web_params.extra_headers];
    web_params.extra_headers = [combinedExtraHeaders copy];
    UrlLoadParams params = UrlLoadParams::InCurrentTab(web_params);
    params.disposition = disposition;
    UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);
  }
  [self cancelOmniboxEdit];
}

#pragma mark - OmniboxCommands

- (void)focusOmniboxFromFakebox {
  [self.omniboxCoordinator focusOmnibox];
}

- (void)focusOmnibox {

  // When the NTP and fakebox are visible, make the fakebox animates into place
  // before focusing the omnibox.
  if (IsVisibleURLNewTabPage([self webState]) &&
      !self.profile->IsOffTheRecord()) {
    id<BrowserCoordinatorCommands> browserCoordinatorCommandsHandler =
        HandlerForProtocol(self.browser->GetCommandDispatcher(),
                           BrowserCoordinatorCommands);
    [browserCoordinatorCommandsHandler focusFakebox];
  } else {
    [self.omniboxCoordinator focusOmnibox];
  }
}

- (void)cancelOmniboxEdit {
  if (self.isCancellingOmniboxEdit) {
    return;
  }
  self.isCancellingOmniboxEdit = YES;
  [self.omniboxCoordinator endEditing];
  self.isCancellingOmniboxEdit = NO;
}

#pragma mark - OmniboxControllerDelegate

- (web::WebState*)webState {
  return self.webStateList->GetActiveWebState();
}

- (LocationBarModel*)locationBarModel {
  return _locationBarModel.get();
}

#pragma mark - LocationBarViewControllerDelegate

- (void)locationBarSteadyViewTapped {
  [self focusOmnibox];
}

- (void)locationBarCopyTapped {
  StoreURLInPasteboard(self.webState->GetVisibleURL());
}

- (void)locationBarRequestScribbleTargetFocus {
  [self.omniboxCoordinator focusOmniboxForScribble];
}

- (void)recordShareButtonPressed {
  if (!self.profile) {
    return;
  }

  base::UmaHistogramEnumeration(
      "iOS.LocationBar.ShareButton.PerProfileType",
      profile_metrics::GetBrowserProfileType(self.profile));
}

- (void)locationBarVisitCopyLinkTapped {
  default_browser::NotifyOmniboxURLCopyPasteAndNavigate(
      self.profile->IsOffTheRecord(),
      feature_engagement::TrackerFactory::GetForProfile(self.profile),
      self.browser->GetSceneState());
}

- (void)locationBarSearchCopiedTextTapped {
  default_browser::NotifyOmniboxTextCopyPasteAndNavigate(
      feature_engagement::TrackerFactory::GetForProfile(self.profile));
}

- (void)searchCopiedImage {
  __weak LocationBarCoordinator* weakSelf = self;
  ClipboardRecentContent::GetInstance()->GetRecentImageFromClipboard(
      base::BindOnce(^(std::optional<gfx::Image> image) {
        [weakSelf searchImage:std::move(image) usingLens:NO];
      }));
}

- (void)lensCopiedImage {
  __weak LocationBarCoordinator* weakSelf = self;
  ClipboardRecentContent::GetInstance()->GetRecentImageFromClipboard(
      base::BindOnce(^(std::optional<gfx::Image> image) {
        [weakSelf searchImage:std::move(image) usingLens:YES];
      }));
}

- (void)displayContextualPanelEntrypointView:(BOOL)display {
  [self.contextualPanelEntrypointCoordinator.viewController
      displayEntrypointView:display];
}

#pragma mark - ContextualPanelEntrypointCoordinatorDelegate

- (BOOL)canShowLargeContextualPanelEntrypoint:
    (ContextualPanelEntrypointCoordinator*)coordinator {
  return [self.viewController canShowLargeContextualPanelEntrypoint];
}

- (void)setLocationBarLabelCenteredBetweenContent:
            (ContextualPanelEntrypointCoordinator*)coordinator
                                         centered:(BOOL)centered {
  [self.viewController setLocationBarLabelCenteredBetweenContent:centered];
}

#pragma mark - LocationBarSteadyViewConsumer

- (void)updateLocationText:(NSString*)text clipTail:(BOOL)clipTail {
  [self.omniboxCoordinator updateOmniboxState];
  [self.viewController updateLocationText:text clipTail:clipTail];
  [self.viewController updateForNTP:NO];
  [self.mediator locationUpdated];
}

- (void)updateLocationIcon:(UIImage*)icon
        securityStatusText:(NSString*)statusText {
  [self.viewController updateLocationIcon:icon securityStatusText:statusText];
}

- (void)updateAfterNavigatingToNTP {
  [self.viewController updateForNTP:YES];
}

- (void)updateLocationShareable:(BOOL)shareable {
  [self.viewController setShareButtonEnabled:shareable];
}

- (void)attemptShowingLensOverlayIPH {
  [self.viewController attemptShowingLensOverlayIPH];
}

- (void)recordLensOverlayAvailability {
  [self.viewController recordLensOverlayAvailability];
}

#pragma mark - URLDragDataSource

- (URLInfo*)URLInfoForView:(UIView*)view {
  // Disable drag and drop when the omnibox is focused, as it interferes with
  // text interactions like moving the cursor (crbug.com/1502538).
  if ([self isOmniboxFirstResponder]) {
    return nil;
  }
  if (!self.webState->GetVisibleURL().is_valid()) {
    return nil;
  }
  return [[URLInfo alloc]
      initWithURL:self.webState->GetVisibleURL()
            title:base::SysUTF16ToNSString(self.webState->GetTitle())];
}

- (UIBezierPath*)visiblePathForView:(UIView*)view {
  return [UIBezierPath bezierPathWithRoundedRect:view.bounds
                                    cornerRadius:view.bounds.size.height / 2];
}

#pragma mark - Private

// Navigate to `query` from omnibox.
- (void)loadURLForQuery:(const std::u16string&)query {
  GURL searchURL;
  metrics::OmniboxInputType type = AutocompleteInput::Parse(
      query, std::string(), AutocompleteSchemeClassifierImpl(), nullptr,
      nullptr, &searchURL);
  if (type != metrics::OmniboxInputType::URL || !searchURL.is_valid()) {
    searchURL = GetDefaultSearchURLForSearchTerms(
        ios::TemplateURLServiceFactory::GetForProfile(self.profile), query);
  }
  if (searchURL.is_valid()) {
    // It is necessary to include PAGE_TRANSITION_FROM_ADDRESS_BAR in the
    // transition type is so that query-in-the-omnibox is triggered for the
    // URL.
    UrlLoadParams params = UrlLoadParams::InCurrentTab(searchURL);
    params.web_params.transition_type = ui::PageTransitionFromInt(
        ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
    UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);
  }
}

- (void)setUpDragAndDrop {
  // iOS 15 adds Drag and Drop support to iPhones. This causes the long-press
  // recognizer for showing the copy/paste menu to not appear until the user
  // lifts their finger. The long-term solution is to move to the new
  // UIContextMenu API, but for now, disable Drag from the omnibox on iOS 15
  // iPhones.
  // TODO (crbug.com/1247668): Reenable this after moving to new API and move
  // this code back to -start.
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE) {
    return;
  }
  self.dragDropHandler = [[URLDragDropHandler alloc] init];
  self.dragDropHandler.origin = WindowActivityLocationBarSteadyViewOrigin;
  self.dragDropHandler.dragDataSource = self;
  [self.viewController.view
      addInteraction:[[UIDragInteraction alloc]
                         initWithDelegate:self.dragDropHandler]];
}

- (void)searchImage:(std::optional<gfx::Image>)optionalImage
          usingLens:(BOOL)usingLens {
  if (!optionalImage)
    return;

  // If the Browser has been destroyed, then the UI should
  // no longer be active. Return early to avoid crashing.
  Browser* browser = self.browser;
  if (!browser)
    return;

  UIImage* image = optionalImage->ToUIImage();
  if (usingLens) {
    id<LensCommands> handler =
        HandlerForProtocol(browser->GetCommandDispatcher(), LensCommands);
    SearchImageWithLensCommand* command = [[SearchImageWithLensCommand alloc]
        initWithImage:image
           entryPoint:LensEntrypoint::OmniboxPostCapture];
    [handler searchImageWithLens:command];
  } else {
    web::NavigationManager::WebLoadParams webParams =
        ImageSearchParamGenerator::LoadParamsForImage(
            image, ios::TemplateURLServiceFactory::GetForProfile(
                       browser->GetProfile()));
    UrlLoadParams params = UrlLoadParams::InCurrentTab(webParams);
    UrlLoadingBrowserAgent::FromBrowser(browser)->Load(params);
  }

  [self cancelOmniboxEdit];
}

@end
