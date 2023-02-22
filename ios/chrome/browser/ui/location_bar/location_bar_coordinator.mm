// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/location_bar/location_bar_coordinator.h"

#import <CoreLocation/CoreLocation.h>

#import "base/memory/ptr_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/omnibox/browser/location_bar_model_impl.h"
#import "components/omnibox/browser/omnibox_edit_model.h"
#import "components/omnibox/browser/omnibox_view.h"
#import "components/open_from_clipboard/clipboard_recent_content.h"
#import "components/profile_metrics/browser_profile_type.h"
#import "components/search_engines/util.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autocomplete/autocomplete_scheme_classifier_impl.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/browser_state_metrics/browser_state_metrics.h"
#import "ios/chrome/browser/drag_and_drop/drag_item_util.h"
#import "ios/chrome/browser/drag_and_drop/url_drag_drop_handler.h"
#import "ios/chrome/browser/feature_engagement/tracker_factory.h"
#import "ios/chrome/browser/infobars/infobar_metrics_recorder.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/overlays/public/overlay_presenter.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/ui/badges/badge_button_factory.h"
#import "ios/chrome/browser/ui/badges/badge_delegate.h"
#import "ios/chrome/browser/ui/badges/badge_mediator.h"
#import "ios/chrome/browser/ui/badges/badge_view_controller.h"
#import "ios/chrome/browser/ui/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/lens_commands.h"
#import "ios/chrome/browser/ui/commands/load_query_commands.h"
#import "ios/chrome/browser/ui/commands/search_image_with_lens_command.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_promo_non_modal_scheduler.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_utils.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_updater.h"
#import "ios/chrome/browser/ui/lens/lens_entrypoint.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_constants.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_consumer.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_mediator.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_model_delegate_ios.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_steady_view_consumer.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_steady_view_mediator.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_url_loader.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_view_controller.h"
#import "ios/chrome/browser/ui/main/default_browser_scene_agent.h"
#import "ios/chrome/browser/ui/main/layout_guide_util.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_util.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_controller_delegate.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_coordinator.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_focus_delegate.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_coordinator.h"
#import "ios/chrome/browser/ui/omnibox/web_omnibox_edit_model_delegate_impl.h"
#import "ios/chrome/browser/ui/util/pasteboard_util.h"
#import "ios/chrome/browser/url_loading/image_search_param_generator.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/url_loading/url_loading_util.h"
#import "ios/chrome/browser/web/web_navigation_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/public/provider/chrome/browser/lens/lens_api.h"
#import "ios/public/provider/chrome/browser/voice_search/voice_search_api.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/web_state.h"
#import "services/network/public/cpp/resource_request.h"
#import "ui/base/device_form_factor.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const size_t kMaxURLDisplayChars = 32 * 1024;
}  // namespace

@interface LocationBarCoordinator () <LoadQueryCommands,
                                      LocationBarViewControllerDelegate,
                                      LocationBarConsumer,
                                      LocationBarSteadyViewConsumer,
                                      OmniboxControllerDelegate,
                                      URLDragDataSource> {
  // API endpoint for omnibox.
  std::unique_ptr<WebOmniboxEditModelDelegateImpl> _editModelDelegate;
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
// Coordinator for the omnibox.
@property(nonatomic, strong) OmniboxCoordinator* omniboxCoordinator;
@property(nonatomic, strong) LocationBarMediator* mediator;
@property(nonatomic, strong) LocationBarSteadyViewMediator* steadyViewMediator;
@property(nonatomic, strong) LocationBarViewController* viewController;
@property(nonatomic, readonly) ChromeBrowserState* browserState;
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

- (ChromeBrowserState*)browserState {
  return self.browser ? self.browser->GetBrowserState() : nullptr;
}

- (WebStateList*)webStateList {
  return self.browser ? self.browser->GetWebStateList() : nullptr;
}

#pragma mark - Public

- (UIViewController*)locationBarViewController {
  return self.viewController;
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

  BOOL isIncognito = self.browserState->IsOffTheRecord();

  self.viewController = [[LocationBarViewController alloc] init];
  self.viewController.incognito = isIncognito;
  self.viewController.delegate = self;
  // TODO(crbug.com/1045047): Use HandlerForProtocol after commands protocol
  // clean up.
  self.viewController.dispatcher =
      static_cast<id<ActivityServiceCommands, ApplicationCommands,
                     LoadQueryCommands, OmniboxCommands>>(
          self.browser->GetCommandDispatcher());
  self.viewController.voiceSearchEnabled =
      ios::provider::IsVoiceSearchEnabled();
  self.viewController.layoutGuideCenter =
      LayoutGuideCenterForBrowser(self.browser);

  _editModelDelegate =
      std::make_unique<WebOmniboxEditModelDelegateImpl>(self, self.delegate);
  _editModelDelegate->SetURLLoader(self);
  _locationBarModelDelegate.reset(new LocationBarModelDelegateIOS(
      self.browser->GetWebStateList(), self.browserState));
  _locationBarModel = std::make_unique<LocationBarModelImpl>(
      _locationBarModelDelegate.get(), kMaxURLDisplayChars);

  self.omniboxCoordinator =
      [[OmniboxCoordinator alloc] initWithBaseViewController:nil
                                                     browser:self.browser];
  self.omniboxCoordinator.editModelDelegate = _editModelDelegate.get();
  self.omniboxCoordinator.presenterDelegate = self.popupPresenterDelegate;
  [self.omniboxCoordinator start];

  [self.omniboxCoordinator.managedViewController
      willMoveToParentViewController:self.viewController];
  [self.viewController
      addChildViewController:self.omniboxCoordinator.managedViewController];
  [self.viewController
      setEditView:self.omniboxCoordinator.managedViewController.view];
  [self.omniboxCoordinator.managedViewController
      didMoveToParentViewController:self.viewController];
  self.viewController.offsetProvider = [self.omniboxCoordinator offsetProvider];

  // Create button factory that wil be used by the ViewController to get
  // BadgeButtons for a BadgeType.
  BadgeButtonFactory* buttonFactory = [[BadgeButtonFactory alloc] init];
  self.badgeViewController =
      [[BadgeViewController alloc] initWithButtonFactory:buttonFactory];
  self.badgeViewController.layoutGuideCenter =
      LayoutGuideCenterForBrowser(self.browser);
  [self.viewController addChildViewController:self.badgeViewController];
  [self.viewController setBadgeView:self.badgeViewController.view];
  [self.badgeViewController didMoveToParentViewController:self.viewController];
  // Create BadgeMediator and set the viewController as its consumer.
  self.badgeMediator = [[BadgeMediator alloc] initWithBrowser:self.browser];
  self.badgeMediator.consumer = self.badgeViewController;
  // TODO(crbug.com/1045047): Use HandlerForProtocol after commands protocol
  // clean up.
  self.badgeMediator.dispatcher = static_cast<id<BrowserCoordinatorCommands>>(
      self.browser->GetCommandDispatcher());
  buttonFactory.delegate = self.badgeMediator;
  FullscreenController* fullscreenController =
      FullscreenController::FromBrowser(self.browser);
  _badgeFullscreenUIUpdater = std::make_unique<FullscreenUIUpdater>(
      fullscreenController, self.badgeViewController);

  self.mediator = [[LocationBarMediator alloc] init];
  self.mediator.templateURLService =
      ios::TemplateURLServiceFactory::GetForBrowserState(self.browserState);
  self.mediator.consumer = self;
  self.mediator.webStateList = self.webStateList;

  self.steadyViewMediator = [[LocationBarSteadyViewMediator alloc]
      initWithLocationBarModel:[self locationBarModel]];
  self.steadyViewMediator.webStateList = self.browser->GetWebStateList();
  self.steadyViewMediator.webContentAreaOverlayPresenter =
      OverlayPresenter::FromBrowser(self.browser,
                                    OverlayModality::kWebContentArea);
  self.steadyViewMediator.consumer = self;

  _omniboxFullscreenUIUpdater = std::make_unique<FullscreenUIUpdater>(
      fullscreenController, self.viewController);

  self.started = YES;

  [self setUpDragAndDrop];
}

- (void)stop {
  if (!self.started)
    return;
  [self.browser->GetCommandDispatcher() stopDispatchingToTarget:self];
  // The popup has to be destroyed before the location bar.
  [self.omniboxCoordinator stop];
  [self.badgeMediator disconnect];
  self.badgeMediator = nil;
  _editModelDelegate.reset();

  self.viewController = nil;
  [self.mediator disconnect];
  self.mediator = nil;
  [self.steadyViewMediator disconnect];
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

#pragma mark - LoadQueryCommands

- (void)loadQuery:(NSString*)query immediately:(BOOL)immediately {
  DCHECK(query);
  // Since the query is not user typed, sanitize it to make sure it's safe.
  std::u16string sanitizedQuery =
      OmniboxView::SanitizeTextForPaste(base::SysNSStringToUTF16(query));
  if (immediately) {
    [self loadURLForQuery:sanitizedQuery];
  } else {
    [self.omniboxCoordinator focusOmnibox];
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
    LoadJavaScriptURL(url, self.browserState,
                      self.webStateList->GetActiveWebState());
  } else {
    // TODO(crbug.com/785244): Is it ok to call `cancelOmniboxEdit` after
    // `loadURL|?  It doesn't seem to be causing major problems.  If we call
    // cancel before load, then any prerendered pages get destroyed before the
    // call to load.
    web::NavigationManager::WebLoadParams web_params =
        web_navigation_util::CreateWebLoadParams(url, transition, postContent);
    if (destination_url_entered_without_scheme) {
      web_params.https_upgrade_type = web::HttpsUpgradeType::kOmnibox;
    }
    NSMutableDictionary* combinedExtraHeaders =
        [web_navigation_util::VariationHeadersForURL(
            url, self.browserState->IsOffTheRecord()) mutableCopy];
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
  // Dismiss the edit menu.
  [[UIMenuController sharedMenuController] hideMenu];

  // When the NTP and fakebox are visible, make the fakebox animates into place
  // before focusing the omnibox.
  if (IsVisibleURLNewTabPage([self webState]) &&
      !self.browserState->IsOffTheRecord()) {
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
  if (!self.browserState) {
    return;
  }

  base::UmaHistogramEnumeration(
      "iOS.LocationBar.ShareButton.PerProfileType",
      profile_metrics::GetBrowserProfileType(self.browserState));
}

- (void)locationBarVisitCopyLinkTapped {
  // Don't log pastes in incognito.
  if (self.browserState->IsOffTheRecord()) {
    return;
  }

  SceneState* sceneState =
      SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();
  DefaultBrowserSceneAgent* agent =
      [DefaultBrowserSceneAgent agentFromScene:sceneState];
  [agent.nonModalScheduler logUserPastedInOmnibox];
  LogToFETUserPastedURLIntoOmnibox(
      feature_engagement::TrackerFactory::GetForBrowserState(
          self.browser->GetBrowserState()));
}

- (void)searchCopiedImage {
  __weak LocationBarCoordinator* weakSelf = self;
  ClipboardRecentContent::GetInstance()->GetRecentImageFromClipboard(
      base::BindOnce(^(absl::optional<gfx::Image> image) {
        [weakSelf searchImage:std::move(image) usingLens:NO];
      }));
}

- (void)lensCopiedImage {
  __weak LocationBarCoordinator* weakSelf = self;
  ClipboardRecentContent::GetInstance()->GetRecentImageFromClipboard(
      base::BindOnce(^(absl::optional<gfx::Image> image) {
        [weakSelf searchImage:std::move(image) usingLens:YES];
      }));
}

#pragma mark - LocationBarConsumer

- (void)defocusOmnibox {
  [self cancelOmniboxEdit];
}

- (void)updateSearchByImageSupported:(BOOL)searchByImageSupported {
  self.viewController.searchByImageEnabled = searchByImageSupported;
}

- (void)updateLensImageSupported:(BOOL)lensImageSupported {
  self.viewController.lensImageEnabled = lensImageSupported;
}

#pragma mark - LocationBarSteadyViewConsumer

- (void)updateLocationText:(NSString*)text clipTail:(BOOL)clipTail {
  [self.omniboxCoordinator updateOmniboxState];
  [self.viewController updateLocationText:text clipTail:clipTail];
  [self.viewController updateForNTP:NO];
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

#pragma mark - URLDragDataSource

- (URLInfo*)URLInfoForView:(UIView*)view {
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
        ios::TemplateURLServiceFactory::GetForBrowserState(self.browserState),
        query);
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
  if (@available(iOS 15, *)) {
    if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE) {
      return;
    }
  }
  self.dragDropHandler = [[URLDragDropHandler alloc] init];
  self.dragDropHandler.origin = WindowActivityLocationBarSteadyViewOrigin;
  self.dragDropHandler.dragDataSource = self;
  [self.viewController.view
      addInteraction:[[UIDragInteraction alloc]
                         initWithDelegate:self.dragDropHandler]];
}

- (void)searchImage:(absl::optional<gfx::Image>)optionalImage
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
            image, ios::TemplateURLServiceFactory::GetForBrowserState(
                       browser->GetBrowserState()));
    UrlLoadParams params = UrlLoadParams::InCurrentTab(webParams);
    UrlLoadingBrowserAgent::FromBrowser(browser)->Load(params);
  }

  [self cancelOmniboxEdit];
}

@end
