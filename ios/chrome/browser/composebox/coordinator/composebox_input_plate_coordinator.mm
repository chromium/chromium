// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/coordinator/composebox_input_plate_coordinator.h"

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import <algorithm>
#import <iterator>
#import <set>

#import "base/memory/raw_ptr.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/contextual_search/contextual_search_service.h"
#import "components/contextual_search/contextual_search_session_handle.h"
#import "components/contextual_search/internal/ios/composebox_query_controller_ios.h"
#import "components/lens/lens_features.h"
#import "components/lens/lens_overlay_invocation_source.h"
#import "components/omnibox/browser/aim_eligibility_service.h"
#import "components/omnibox/browser/location_bar_model_impl.h"
#import "components/search_engines/template_url_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/aim/model/ios_chrome_aim_eligibility_service_factory.h"
#import "ios/chrome/browser/cobrowse/model/cobrowse_browser_agent.h"
#import "ios/chrome/browser/cobrowse/model/cobrowse_context.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_cobrowse_omnibox_client.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_input_plate_mediator.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_mode_holder.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_omnibox_client.h"
#import "ios/chrome/browser/composebox/debugger/composebox_debugger_logger.h"
#import "ios/chrome/browser/composebox/menu/coordinator/composebox_menu_coordinator.h"
#import "ios/chrome/browser/composebox/model/ios_contextual_search_service_factory.h"
#import "ios/chrome/browser/composebox/public/composebox_attachment_selection.h"
#import "ios/chrome/browser/composebox/public/composebox_entrypoint.h"
#import "ios/chrome/browser/composebox/public/composebox_focus_params.h"
#import "ios/chrome/browser/composebox/public/composebox_input_item_source.h"
#import "ios/chrome/browser/composebox/public/composebox_model_option.h"
#import "ios/chrome/browser/composebox/public/composebox_theme.h"
#import "ios/chrome/browser/composebox/public/features.h"
#import "ios/chrome/browser/composebox/shared/coordinator/composebox_attachment_diff.h"
#import "ios/chrome/browser/composebox/shared/coordinator/composebox_picker_presenter.h"
#import "ios/chrome/browser/composebox/shared/metrics/composebox_metrics_recorder.h"
#import "ios/chrome/browser/composebox/shared/ui/composebox_snackbar_presenter.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_plate_view_controller.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_plate_view_controller_delegate.h"
#import "ios/chrome/browser/drag_and_drop/model/drag_item_util.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/intelligence/persist_tab_context/model/persist_tab_context_browser_agent.h"
#import "ios/chrome/browser/lens/ui_bundled/lens_entrypoint.h"
#import "ios/chrome/browser/location_bar/model/web_location_bar_delegate.h"
#import "ios/chrome/browser/location_bar/model/web_location_bar_impl.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_model_delegate_ios.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_url_loader.h"
#import "ios/chrome/browser/omnibox/coordinator/omnibox_coordinator.h"
#import "ios/chrome/browser/omnibox/public/omnibox_presentation_context.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_focus_delegate.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_lens_input_selection_command.h"
#import "ios/chrome/browser/shared/public/commands/qr_scanner_commands.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_utils.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/url_loading/model/url_loading_util.h"
#import "ios/chrome/browser/variations/model/client/variations_client_service.h"
#import "ios/chrome/browser/variations/model/client/variations_client_service_factory.h"
#import "ios/chrome/browser/web/model/web_navigation_util.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/public/provider/chrome/browser/voice_search/voice_search_api.h"
#import "ios/web/public/web_state.h"
#import "net/base/apple/url_conversions.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "third_party/omnibox_proto/searchbox_config.pb.h"

namespace {
const size_t kMaxURLDisplayChars = 32 * 1024;
const CGFloat kSnackbarBottomMargin = 10;

// Converts ComposeboxEntrypoint to ContextualSearchSource.
contextual_search::ContextualSearchSource ContextualSearchSourceFromEntrypoint(
    ComposeboxEntrypoint entrypoint) {
  switch (entrypoint) {
    case ComposeboxEntrypoint::kNTPAIMButton:
    case ComposeboxEntrypoint::kNTPPlusButton:
      return contextual_search::ContextualSearchSource::kNewTabPage;
    case ComposeboxEntrypoint::kNTPFakebox:
    case ComposeboxEntrypoint::kOther:
      return contextual_search::ContextualSearchSource::kOmnibox;
    case ComposeboxEntrypoint::kCobrowse:
      return contextual_search::ContextualSearchSource::kContextualTasks;
  }
}

}  // namespace

@interface ComposeboxInputPlateCoordinator () <
    ComposeboxInputPlateMediatorDelegate,
    ComposeboxInputPlateViewControllerDelegate,
    ComposeboxPickerPresenterDelegate,
    ComposeboxPickerPresenterDataSource,
    ComposeboxMenuCoordinatorDelegate,
    ComposeboxMenuCoordinatorInputPlateDelegate,
    LocationBarModelDelegateWebStateProvider,
    LocationBarURLLoader,
    OmniboxFocusDelegate,
    UINavigationControllerDelegate,
    UIViewControllerTransitioningDelegate,
    WebLocationBarDelegate>
@end

@implementation ComposeboxInputPlateCoordinator {
  ComposeboxInputPlateViewController* _viewController;
  ComposeboxInputPlateMediator* _mediator;
  id<VoiceSearchController> _voiceSearchController;
  /// The entrypoing from which the coordinator was invoked.
  ComposeboxEntrypoint _entrypoint;
  /// Optional query inserted into the omnibox at start.
  NSString* _query;
  /// The URLLoader to pass to the mediator.
  __weak id<ComposeboxURLLoader> _URLLoader;
  /// Coordinator of the omnibox.
  OmniboxCoordinator* _omniboxCoordinator;
  // API endpoint for omnibox.
  std::unique_ptr<WebLocationBarImpl> _locationBar;
  std::unique_ptr<LocationBarModelDelegateIOS> _locationBarModelDelegate;
  std::unique_ptr<LocationBarModel> _locationBarModel;
  raw_ptr<contextual_search::ContextualSearchService> _contextualService;
  ComposeboxTheme* _theme;
  ComposeboxMetricsRecorder* _metricsRecorder;
  ComposeboxModeHolder* _modeHolder;
  ComposeboxSnackbarPresenter* _snackbarPresenter;

  // The coordinator for the bottom sheet menu.
  ComposeboxMenuCoordinator* _menuCoorinator;

  // Parameters used to focus and initialize the composebox.
  ComposeboxFocusParams* _focusParams;

  // The presenter for pickers.
  ComposeboxPickerPresenter* _pickerPresenter;
}

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                               focusParams:(ComposeboxFocusParams*)focusParams
                                 URLLoader:(id<ComposeboxURLLoader>)URLLoader
                                     theme:(ComposeboxTheme*)theme
                                modeHolder:(ComposeboxModeHolder*)modeHolder {
  self = [super initWithBaseViewController:baseViewController browser:browser];
  if (self) {
    _entrypoint = focusParams.entrypoint;
    _query = focusParams.query;
    _URLLoader = URLLoader;
    _theme = theme;
    // If there a shared metrics recorder, reuse it to maintain the same
    // recording session.
    _metricsRecorder = focusParams.metricsRecorder
                           ?: [[ComposeboxMetricsRecorder alloc]
                                  initWithEntrypoint:_entrypoint];
    _modeHolder = modeHolder;
    _focusParams = focusParams;
  }
  return self;
}

- (void)start {
  _viewController =
      [[ComposeboxInputPlateViewController alloc] initWithTheme:_theme];
  _viewController.delegate = self;
  _pickerPresenter = [[ComposeboxPickerPresenter alloc]
      initWithBaseViewController:_viewController
                         browser:self.browser];
  _pickerPresenter.delegate = self;
  _pickerPresenter.dataSource = self;

  if (_entrypoint == ComposeboxEntrypoint::kNTPAIMButton) {
    [_metricsRecorder
        recordAiModeActivationSource:AiModeActivationSource::kNTPButton];
  }

  _voiceSearchController =
      ios::provider::CreateVoiceSearchController(self.browser);

  std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
      contextualSearchSession = nullptr;
  if (!IsComposeboxAIMDisabled()) {
    auto query_controller_config_params = std::make_unique<
        contextual_search::ContextualSearchContextController::ConfigParams>();
    query_controller_config_params->send_lns_surface = false;
    query_controller_config_params->enable_viewport_images = true;
    query_controller_config_params
        ->prioritize_suggestions_for_the_first_attached_document = true;

    _contextualService =
        ContextualSearchServiceFactory::GetForProfile(self.profile);

    contextualSearchSession = _contextualService->CreateSession(
        std::move(query_controller_config_params),
        ContextualSearchSourceFromEntrypoint(_entrypoint),
        lens::LensOverlayInvocationSource::kOmniboxContextualQuery);
    _metricsRecorder.contextualSearchMetricsRecorder =
        contextualSearchSession->GetMetricsRecorder();
  }

  FaviconLoader* faviconLoader =
      IOSChromeFaviconLoaderFactory::GetForProfile(self.profile);
  TemplateURLService* templateURLService =
      ios::TemplateURLServiceFactory::GetForProfile(self.profile);
  AimEligibilityService* aimEligibilityService =
      IOSChromeAimEligibilityServiceFactory::GetForProfile(self.profile);

  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();

  _mediator = [[ComposeboxInputPlateMediator alloc]
      initWithContextualSearchSession:std::move(contextualSearchSession)
                         webStateList:self.browser->GetWebStateList()
                        faviconLoader:faviconLoader
               persistTabContextAgent:PersistTabContextBrowserAgent::
                                          FromBrowser(self.browser)
                          isIncognito:self.isOffTheRecord
                           modeHolder:_modeHolder
                   templateURLService:templateURLService
                aimEligibilityService:aimEligibilityService
                          prefService:self.profile->GetPrefs()
                              profile:self.profile
                 cobrowseBrowserAgent:CobrowseBrowserAgent::FromBrowser(
                                          self.browser)
            browserCoordinatorHandler:HandlerForProtocol(
                                          dispatcher,
                                          BrowserCoordinatorCommands)
                         sceneHandler:HandlerForProtocol(dispatcher,
                                                         SceneCommands)
                           entrypoint:_entrypoint];
  _mediator.debugLogger = self.debugLogger;
  _mediator.URLLoader = _URLLoader;
  _mediator.consumer = _viewController;
  _mediator.delegate = self;
  _mediator.metricsRecorder = _metricsRecorder;


  _viewController.mutator = _mediator;
  // Mediator is the voice search delegate to load queries in composebox.
  _voiceSearchController.delegate = _mediator;

  _locationBar = std::make_unique<WebLocationBarImpl>(self);
  _locationBar->SetURLLoader(self);
  _locationBarModelDelegate.reset(
      new LocationBarModelDelegateIOS(self, self.profile));
  _locationBarModel = std::make_unique<LocationBarModelImpl>(
      _locationBarModelDelegate.get(), kMaxURLDisplayChars);

  std::unique_ptr<OmniboxClientIOS> omniboxClient;
  if (_entrypoint == ComposeboxEntrypoint::kCobrowse) {
    omniboxClient = std::make_unique<ComposeboxCobrowseOmniboxClient>(
        self.browser,
        feature_engagement::TrackerFactory::GetForProfile(self.profile),
        _mediator);
  } else {
    omniboxClient = std::make_unique<ComposeboxOmniboxClient>(
        _locationBar.get(), self.browser,
        feature_engagement::TrackerFactory::GetForProfile(self.profile),
        _mediator);
  }

  _omniboxCoordinator = [[OmniboxCoordinator alloc]
      initWithBaseViewController:nil
                         browser:self.browser
                   omniboxClient:std::move(omniboxClient)
             presentationContext:_entrypoint == ComposeboxEntrypoint::kCobrowse
                                     ? OmniboxPresentationContext::kCobrowse
                                     : OmniboxPresentationContext::kComposebox];
  _omniboxCoordinator.presenterDelegate = self.omniboxPopupPresenterDelegate;
  _omniboxCoordinator.focusDelegate = self;
  [_omniboxCoordinator start];

  [_omniboxCoordinator.managedViewController
      willMoveToParentViewController:_viewController];
  [_viewController
      addChildViewController:_omniboxCoordinator.managedViewController];
  [_viewController setEditView:_omniboxCoordinator.editView];
  _omniboxCoordinator.editView.heightDelegate = _mediator;
  [_omniboxCoordinator.managedViewController
      didMoveToParentViewController:_viewController];

  if (_focusParams) {
    [_mediator applyFocusParams:_focusParams];
  }

  [_omniboxCoordinator updateOmniboxState];
  if (_entrypoint != ComposeboxEntrypoint::kCobrowse) {
    [_omniboxCoordinator focusOmnibox];
  }
}

- (void)stop {
  if (_menuCoorinator) {
    [_menuCoorinator stop];
    _menuCoorinator = nil;
  }
  [_snackbarPresenter dismissAllSnackbars];
  [_snackbarPresenter stop];
  _snackbarPresenter = nil;
  [_metricsRecorder recordAttachmentButtonsUsageInSession];

  _metricsRecorder.contextualSearchMetricsRecorder = nullptr;

  _viewController.mutator = nil;
  _viewController = nil;
  _pickerPresenter = nil;
  [_voiceSearchController dismissMicPermissionHelp];
  [_voiceSearchController disconnect];
  _voiceSearchController = nil;
  [_mediator disconnect];
  _mediator = nil;
  [_omniboxCoordinator endEditing];
  [_omniboxCoordinator stop];
  _omniboxCoordinator = nil;
  _metricsRecorder = nil;
  _theme = nil;
  _modeHolder = nil;
  _contextualService = nullptr;

  _locationBar = nullptr;
  _locationBarModel = nullptr;
  _locationBarModelDelegate = nullptr;
  _focusParams = nullptr;
}

- (UIViewController*)inputViewController {
  return _viewController;
}

/// Shows the debug UI.
- (void)showOmniboxDebugUI {
  [_omniboxCoordinator toggleOmniboxDebuggerView];
}

- (void)endEditing {
  [_omniboxCoordinator endEditing];
}

#pragma mark - ComposeboxInputPlateViewControllerDelegate

- (void)composeboxViewController:
            (ComposeboxInputPlateViewController*)composeboxViewController
                 didTapMicButton:(UIButton*)micButton {
  [_metricsRecorder recordVoiceSearchButtonUsed];

  WebStateList* webStateList = self.browser->GetWebStateList();
  if (!webStateList) {
    return;
  }
  web::WebState* webState = webStateList->GetActiveWebState();
  if (!webState) {
    return;
  }

  LayoutGuideCenter* layoutGuideCenter =
      LayoutGuideCenterForBrowser(self.browser);
  [layoutGuideCenter referenceView:micButton underName:kVoiceSearchButtonGuide];

  [_voiceSearchController startRecognitionOnViewController:_viewController
                                                  webState:webState];
}

- (void)composeboxViewController:
            (ComposeboxInputPlateViewController*)composeboxViewController
                didTapLensButton:(UIButton*)lensButton {
  [_metricsRecorder recordLensSearchButtonUsed];
  OpenLensInputSelectionCommand* command = [[OpenLensInputSelectionCommand
      alloc]
          initWithEntryPoint:LensEntrypoint::Composebox
           presentationStyle:LensInputSelectionPresentationStyle::SlideFromRight
      presentationCompletion:nil];
  __weak id<LensCommands> handler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), LensCommands);
  [self.baseViewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           [handler openLensInputSelection:command];
                         }];
}

- (void)composeboxViewController:
            (ComposeboxInputPlateViewController*)composeboxViewController
           didTapQRScannerButton:(UIButton*)button {
  [_metricsRecorder recordQRScannerButtonUsed];

  __weak id<QRScannerCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), QRScannerCommands);
  [self.baseViewController dismissViewControllerAnimated:YES
                                              completion:^{
                                                [handler showQRScanner];
                                              }];
}

- (void)composeboxViewControllerDidCompleteInitialPresentation:
    (ComposeboxInputPlateViewController*)composeboxViewController {
  if (_entrypoint == ComposeboxEntrypoint::kNTPPlusButton) {
    [composeboxViewController showMultimodalMenu];
  }
}

- (void)composeboxViewControllerDidTapGalleryButton:
    (ComposeboxInputPlateViewController*)composeboxViewController {
  [_metricsRecorder
      recordAttachmentButtonUsed:FuseboxAttachmentButtonType::kGallery];
  if (![_mediator canAddMoreAttachments]) {
    [self showMaxAttachmentSnackbarError];
    return;
  }

  [_pickerPresenter
      presentGalleryPickerWithLimit:[_mediator remainingNumberOfImagesAllowed]];
}

- (void)composeboxViewControllerDidTapCameraButton:
    (ComposeboxInputPlateViewController*)composeboxViewController {
  [_metricsRecorder
      recordAttachmentButtonUsed:FuseboxAttachmentButtonType::kCamera];
  if (![_mediator canAddMoreAttachments]) {
    [self showMaxAttachmentSnackbarError];
    return;
  }

  [_pickerPresenter presentCameraPicker];
}

- (void)composeboxViewController:
            (ComposeboxInputPlateViewController*)composeboxViewController
    didOpenPlusMenuWithVisibleInternalButtons:
        (const std::vector<FuseboxAttachmentButtonType>&)visibleInternalButtons
                                 uiInputState:(ComposeboxUIInputState*)state {
  [_mediator
      recordPlusMenuOpenedWithVisibleInternalButtons:visibleInternalButtons
                                        uiInputState:state];
}

- (void)composeboxViewControllerDidTapPlusButton:
            (ComposeboxInputPlateViewController*)composeboxViewController
                                withUIInputState:
                                    (ComposeboxUIInputState*)state {
  if (IsComposeboxPlusButtonBottomSheet()) {
    _menuCoorinator = [[ComposeboxMenuCoordinator alloc]
        initWithBaseViewController:_viewController
                           browser:self.browser
            preselectedAttachments:_mediator.currentAttachmentSelection
                        inputState:state
                   metricsRecorder:_metricsRecorder
                        entrypoint:_entrypoint];
    _menuCoorinator.inputPlateDelegate = self;
    _menuCoorinator.delegate = self;
    [_menuCoorinator start];
  }
}

- (void)composeboxViewControllerDidTapFileButton:
    (ComposeboxInputPlateViewController*)composeboxViewController {
  [_metricsRecorder
      recordAttachmentButtonUsed:FuseboxAttachmentButtonType::kFiles];
  if (![_mediator canAddMoreAttachments]) {
    [self showMaxAttachmentSnackbarError];
    return;
  }

  [_pickerPresenter presentFilePicker];
}

- (void)composeboxViewControllerDidTapAttachTabsButton:
    (ComposeboxInputPlateViewController*)viewController {
  if (![_mediator canAddMoreAttachments]) {
    [self showMaxAttachmentSnackbarError];
    return;
  }

  [_metricsRecorder
      recordAttachmentButtonUsed:FuseboxAttachmentButtonType::kTabPicker];
  [_pickerPresenter presentTabPicker];
}

- (void)composeboxViewController:
            (ComposeboxInputPlateViewController*)viewController
       didAttemptDragAndDropType:(ComposeboxDragAndDropType)type {
  [_metricsRecorder recordDragAndDropAttempt:type];
}

- (void)composeboxViewController:
            (ComposeboxInputPlateViewController*)composeboxViewController
                      didTapTool:(ComposeboxMode)toolMode
                activationSource:(AiModeActivationSource)activationSource {
  [self selectTool:toolMode activationSource:activationSource];
}

- (void)composeboxViewController:
            (ComposeboxInputPlateViewController*)composeboxViewController
                  didSelectModel:(ComposeboxModelOption)modelOption {
  [_mediator setModelOption:modelOption explicitUserAction:YES];
  [_metricsRecorder recordModelSelected:modelOption];
}

- (void)composeboxViewController:
            (ComposeboxInputPlateViewController*)composeboxViewController
                didTapSendButton:(UIButton*)button {
  [_omniboxCoordinator acceptInput];
}

- (void)didFailToAttachDueToIneligibleAttachments:
    (ComposeboxInputPlateViewController*)composeboxViewController {
  CHECK_EQ(_viewController, composeboxViewController);
  switch (_modeHolder.mode) {
    case ComposeboxMode::kRegularSearch:
    case ComposeboxMode::kAIM:
    case ComposeboxMode::kCanvas:
    case ComposeboxMode::kDeepSearch:
      [self showMaxAttachmentSnackbarError];
      return;
    case ComposeboxMode::kImageGeneration:
      [self showMaxAttachmentForImageGenerationSnackbarError];
      return;
  }
  NOTREACHED();
}

- (BOOL)tabExistsOnCurrentProfile:(TabInfo*)tabInfo {
  if (self.profile != tabInfo.profile) {
    return NO;
  }

  BrowserList* browserList = BrowserListFactory::GetForProfile(tabInfo.profile);
  WebStateSearchCriteria tabSearchCriteria = WebStateSearchCriteria{
      .identifier = tabInfo.tabID,
      .pinned_state = WebStateSearchCriteria::PinnedState::kAny,
  };

  return GetBrowserForTabWithCriteria(browserList, tabSearchCriteria,
                                      /*is_otr_tab*/ _theme.incognito);
}

- (web::WebState*)webStateForTabOnCurrentProfile:(TabInfo*)tabInfo {
  if (self.profile != tabInfo.profile) {
    return nullptr;
  }

  BrowserList* browserList = BrowserListFactory::GetForProfile(tabInfo.profile);
  WebStateSearchCriteria tabSearchCriteria = WebStateSearchCriteria{
      .identifier = tabInfo.tabID,
      .pinned_state = WebStateSearchCriteria::PinnedState::kAny,
  };

  return GetWebStateForTabWithCriteria(browserList, tabSearchCriteria,
                                       _theme.incognito);
}

#pragma mark - ComposeboxInputPlateMediatorDelegate

- (void)reloadAutocompleteSuggestionsRestarting:(BOOL)restart {
  [_omniboxCoordinator clearSuggestionsWithRestartAutocomplete:restart];
}

- (void)refineWithText:(NSString*)text {
  [_omniboxCoordinator refineWithText:text];
}

- (void)showAttachmentLimitError {
  [self showMaxAttachmentSnackbarError];
}

- (void)showSnackbarForItemUploadDidFail {
  [self showUnableToAddAttachmentSnackbarError];
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
    // Not managed in the proto.
    return;
  } else {
    web::NavigationManager::WebLoadParams web_params =
        web_navigation_util::CreateWebLoadParams(url, transition, postContent);
    if (destination_url_entered_without_scheme) {
      web_params.https_upgrade_type = web::HttpsUpgradeType::kOmnibox;
    }
    bool isIncognito = self.profile->IsOffTheRecord();

    NSMutableDictionary<NSString*, NSString*>* combinedExtraHeaders =
        [web_navigation_util::VariationHeadersForURL(url, isIncognito)
            mutableCopy];
    [combinedExtraHeaders addEntriesFromDictionary:web_params.extra_headers];
    web_params.extra_headers = [combinedExtraHeaders copy];
    UrlLoadParams params = UrlLoadParams::InNewTab(web_params);
    params.disposition = disposition;
    params.in_incognito = isIncognito;
    if (params.disposition == WindowOpenDisposition::CURRENT_TAB) {
      params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    }
    UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);
  }

  [self dismissComposebox];
}

#pragma mark - LocationBarModelDelegateWebStateProvider

- (web::WebState*)webStateForLocationBarModelDelegate:
    (const LocationBarModelDelegateIOS*)locationBarModelDelegate {
  return [self webState];
}

#pragma mark - WebLocationBarDelegate

- (web::WebState*)webState {
  return self.browser->GetWebStateList()->GetActiveWebState();
}

- (LocationBarModel*)locationBarModel {
  return _locationBarModel.get();
}

#pragma mark - OmniboxFocusDelegate

- (void)omniboxDidBecomeFirstResponder {
  // When the omnibox is focused the first time, set the initial `_query` if
  // there is one. This can be used by features like QR code scanner to write
  // URLs in the omnibox.
  if (_query) {
    [_omniboxCoordinator insertTextToOmnibox:_query];
    _query = nil;
  }
  [_mediator setOmniboxFocused:YES];
}

- (void)omniboxDidResignFirstResponder {
  [_mediator setOmniboxFocused:NO];
}

- (void)omniboxDidEndEditing {
}

#pragma mark - Private helpers

/// Updates the active tool mode in the composebox, toggling it off if it's
/// already active, and records associated metrics.
- (void)selectTool:(ComposeboxMode)toolMode
    activationSource:(AiModeActivationSource)activationSource {
  if (_modeHolder.mode == toolMode) {
    ComposeboxMode defaultMode = _entrypoint == ComposeboxEntrypoint::kCobrowse
                                     ? ComposeboxMode::kAIM
                                     : ComposeboxMode::kRegularSearch;
    _modeHolder.mode = defaultMode;
  } else {
    _modeHolder.mode = toolMode;
    if (toolMode == ComposeboxMode::kAIM) {
      [_metricsRecorder recordAiModeActivationSource:activationSource];
    }
    [_metricsRecorder recordToolSelected:toolMode];
  }
}

- (void)focusComposebox {
  [_omniboxCoordinator focusOmnibox];
}

/// Dismisses the composebox via a command to the browser coordinator.
- (void)dismissComposebox {
  id<BrowserCoordinatorCommands> browserCoordinatorHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), BrowserCoordinatorCommands);
  [browserCoordinatorHandler hideComposebox];
}

/// Displays a snackbar error indicating the maximum number of attachments has
/// been reached.
- (void)showMaxAttachmentSnackbarError {
  [self createSnackbarPresenterIfNeeded];
  CGFloat offset = _viewController.keyboardHeight;
  if (!_theme.isTopInputPlate) {
    offset += _viewController.inputHeight + kSnackbarBottomMargin;
  }
  [_snackbarPresenter
      showSnackbarForAttachmentLimit:[_mediator remainingAttachmentCapacity]
                        bottomOffset:offset];
}

/// Displays a snackbar error indicating that attachment failed to be added.
- (void)showUnableToAddAttachmentSnackbarError {
  [self createSnackbarPresenterIfNeeded];
  CGFloat offset = _viewController.keyboardHeight;
  if (!_theme.isTopInputPlate) {
    offset += _viewController.inputHeight + kSnackbarBottomMargin;
  }
  [_snackbarPresenter showUnableToAddAttachmentSnackbarWithBottomOffset:offset];
}

/// Displays a snackbar error indicating the maximum number of attachments has
/// been reached.
- (void)showMaxAttachmentForImageGenerationSnackbarError {
  [self createSnackbarPresenterIfNeeded];
  CGFloat offset = _viewController.keyboardHeight;
  if (!_theme.isTopInputPlate) {
    offset += _viewController.inputHeight + kSnackbarBottomMargin;
  }
  [_snackbarPresenter showAttachmentLimitForImageGenerationSnackbar:
                          kAttachmentLimitForImageGeneration
                                                   withBottomOffset:offset];
}

- (void)createSnackbarPresenterIfNeeded {
  if (_snackbarPresenter) {
    return;
  }
  _snackbarPresenter =
      [[ComposeboxSnackbarPresenter alloc] initWithBrowser:self.browser];
}

#pragma mark - ComposeboxPickerPresenterDelegate

- (void)composeboxPickerPresenter:(ComposeboxPickerPresenter*)presenter
                    didPickImages:
                        (NSArray<ComposeboxPickerImageResult*>*)results {
  if (results.count == 0) {
    return;
  }

  [_metricsRecorder recordImagesAttached:results.count];

  for (ComposeboxPickerImageResult* result in results) {
    [_mediator processImageItemProvider:result.imageProvider
                                assetID:result.assetID
                                 source:result.source];
  }
}

- (void)composeboxPickerPresenter:(ComposeboxPickerPresenter*)presenter
             didPickFilesWithURLs:(NSArray<NSURL*>*)urls {
  if (urls.count == 0) {
    return;
  }

  [_metricsRecorder recordFilesAttached:urls.count];

  NSURL* selectedURL = urls.firstObject;
  if (!lens::features::IsLensSendRawFileMediaTypesEnabled()) {
    [_mediator processFileURL:net::GURLWithNSURL(selectedURL) isPDF:YES];
    return;
  }

  NSError* error = nil;
  UTType* contentType = nil;

  // Accessing the resource should be requested before using and relinquished
  // when no longer needed.
  // Revoking the access should be done once the resource is no longer needed,
  // as requesting access again on a relinquished resource might fail.
  BOOL accessing = [selectedURL startAccessingSecurityScopedResource];
  [selectedURL getResourceValue:&contentType
                         forKey:NSURLContentTypeKey
                          error:&error];

  auto stopAccessScopedResourcesIfNeeded = ^{
    if (accessing) {
      [selectedURL stopAccessingSecurityScopedResource];
    }
  };

  if (contentType && !error) {
    if ([contentType conformsToType:UTTypeImage]) {
      NSItemProvider* provider =
          [[NSItemProvider alloc] initWithContentsOfURL:selectedURL];
      [_mediator processImageItemProvider:provider
                                  assetID:selectedURL.absoluteString
                                   source:ComposeboxInputItemSource::kFilePicker
                               completion:stopAccessScopedResourcesIfNeeded];
      return;
    } else if ([contentType conformsToType:UTTypePDF]) {
      [_mediator processFileURL:net::GURLWithNSURL(selectedURL)
                          isPDF:YES
                     completion:stopAccessScopedResourcesIfNeeded];
      return;
    }
  }

  [_mediator processFileURL:net::GURLWithNSURL(selectedURL)
                      isPDF:NO
                 completion:stopAccessScopedResourcesIfNeeded];
}

- (void)composeboxPickerPresenterDidDissmissCamera:
    (ComposeboxPickerPresenter*)presenter {
  [self focusComposebox];
}

- (void)composeboxPickerPresenter:(ComposeboxPickerPresenter*)presenter
    handleSelectedTabsWithWebStateIDs:
        (std::set<web::WebStateID>)selectedWebStateIDs
                    cachedWebStateIDs:
                        (std::set<web::WebStateID>)cachedWebStateIDs {
  std::set<web::WebStateID> alreadyProcessedIDs =
      [_mediator allAttachedWebStateIDs];
  composebox::TabDiff diff =
      composebox::ComputeTabDiff(alreadyProcessedIDs, selectedWebStateIDs);

  if (diff.added.size() > 0) {
    [_metricsRecorder recordTabPickerTabsAttached:diff.added.size()];
  }

  [_mediator attachSelectedTabsWithWebStateIDs:selectedWebStateIDs
                             cachedWebStateIDs:cachedWebStateIDs];
}

- (void)composeboxPickerPresenter:(ComposeboxPickerPresenter*)presenter
                didPickDriveItems:
                    (NSArray<ComposeboxPickerDriveResult*>*)results {
  if (results.count == 0) {
    return;
  }
  // TODO(crbug.com/515377633): Loop through `results` and process each Drive
  // file.
}

#pragma mark - ComposeboxPickerPresenterDataSource

- (std::set<web::WebStateID>)allAttachedWebStateIDsForPresenter:
    (ComposeboxPickerPresenter*)presenter {
  return [_mediator allAttachedWebStateIDs];
}

- (std::set<web::WebStateID>)attachedWebStateIDsInCurrentContextForPresenter:
    (ComposeboxPickerPresenter*)presenter {
  return [_mediator attachedWebStateIDsInCurrentContext];
}

- (NSUInteger)maxTabAttachmentCountForPresenter:
    (ComposeboxPickerPresenter*)presenter {
  return [_mediator maxTabAttachmentCount];
}

#pragma mark - ComposeboxMenuCoordinatorInputPlateDelegate

- (void)composeboxMenuCoordinator:(ComposeboxMenuCoordinator*)coordinator
                       didTapTool:(ComposeboxMode)toolMode {
  [self selectTool:toolMode activationSource:AiModeActivationSource::kToolMenu];
}

- (void)composeboxMenuCoordinator:(ComposeboxMenuCoordinator*)coordinator
                      didTapModel:(ComposeboxModelOption)modelMode {
  [_mediator setModelOption:modelMode explicitUserAction:YES];
  [_metricsRecorder recordModelSelected:modelMode];
}

- (void)composeboxMenuCoordinator:(ComposeboxMenuCoordinator*)coordinator
             didUpdateAttachments:(ComposeboxAttachmentSelection*)attachments {
  if (attachments) {
    [_mediator updateAttachments:attachments];
  }
}

#pragma mark - ComposeboxMenuCoordinatorDelegate

- (void)composeboxMenuCoordinatorDidDismissMenu:
    (ComposeboxMenuCoordinator*)composeboxMenuCoordinator {
  [_menuCoorinator stop];
  _menuCoorinator = nil;
}

@end
