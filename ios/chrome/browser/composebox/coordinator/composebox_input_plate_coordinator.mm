// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/coordinator/composebox_input_plate_coordinator.h"

#import <PhotosUI/PhotosUI.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "base/memory/raw_ptr.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/contextual_search/contextual_search_service.h"
#import "components/contextual_search/contextual_search_session_handle.h"
#import "components/omnibox/browser/location_bar_model_impl.h"
#import "components/omnibox/composebox/ios/composebox_query_controller_ios.h"
#import "components/search_engines/template_url_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/aim/model/ios_chrome_aim_eligibility_service_factory.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_entrypoint.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_input_plate_mediator.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_mode_holder.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_omnibox_client.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_tab_picker_coordinator.h"
#import "ios/chrome/browser/composebox/model/ios_contextual_search_service_factory.h"
#import "ios/chrome/browser/composebox/public/composebox_theme.h"
#import "ios/chrome/browser/composebox/public/features.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_plate_view_controller.h"
#import "ios/chrome/browser/composebox/ui/composebox_metrics_recorder.h"
#import "ios/chrome/browser/composebox/ui/composebox_snackbar_presenter.h"
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
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_lens_input_selection_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
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

namespace {
const size_t kMaxURLDisplayChars = 32 * 1024;
const CGFloat kSnackbarBottomMargin = 10;
}

@interface ComposeboxInputPlateCoordinator () <
    ComposeboxInputPlateMediatorDelegate,
    ComposeboxInputPlateViewControllerDelegate,
    LocationBarModelDelegateWebStateProvider,
    LocationBarURLLoader,
    PHPickerViewControllerDelegate,
    UIDocumentPickerDelegate,
    UIImagePickerControllerDelegate,
    UINavigationControllerDelegate,
    UIViewControllerTransitioningDelegate,
    WebLocationBarDelegate>
@end

@implementation ComposeboxInputPlateCoordinator {
  ComposeboxInputPlateViewController* _viewController;
  ComposeboxInputPlateMediator* _mediator;
  id<VoiceSearchController> _voiceSearchController;
  /// The prewarmed picker as it takes time to appear.
  PHPickerViewController* _picker;
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
  ComposeboxTabPickerCoordinator* _tabPickerCoordinator;
  ComposeboxTheme* _theme;
  ComposeboxMetricsRecorder* _metricsRecorder;
  ComposeboxModeHolder* _modeHolder;
}

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                                entrypoint:(ComposeboxEntrypoint)entrypoint
                                     query:(NSString*)query
                                 URLLoader:(id<ComposeboxURLLoader>)URLLoader
                                     theme:(ComposeboxTheme*)theme
                                modeHolder:(ComposeboxModeHolder*)modeHolder {
  self = [super initWithBaseViewController:baseViewController browser:browser];
  if (self) {
    _entrypoint = entrypoint;
    _query = query;
    _URLLoader = URLLoader;
    _theme = theme;
    _metricsRecorder = [[ComposeboxMetricsRecorder alloc] init];
    _modeHolder = modeHolder;
  }
  return self;
}

- (void)start {
  _viewController =
      [[ComposeboxInputPlateViewController alloc] initWithTheme:_theme];
  _viewController.delegate = self;

  if (_entrypoint == ComposeboxEntrypoint::kNTPAIMButton) {
    [_metricsRecorder
        recordAiModeActivationSource:AiModeActivationSource::kNTPButton];
  }

  _voiceSearchController =
      ios::provider::CreateVoiceSearchController(self.browser);

  auto query_controller_config_params = std::make_unique<
      contextual_search::ContextualSearchContextController::ConfigParams>();
  query_controller_config_params->send_lns_surface = false;
  query_controller_config_params->enable_multi_context_input_flow = true;
  query_controller_config_params->enable_viewport_images = true;
  query_controller_config_params
      ->prioritize_suggestions_for_the_first_attached_document = true;

  _contextualService =
      ContextualSearchServiceFactory::GetForProfile(self.profile);

  std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
      contextualSearchSession = _contextualService->CreateSession(
          std::move(query_controller_config_params),
          contextual_search::ContextualSearchSource::kOmnibox);

  FaviconLoader* faviconLoader =
      IOSChromeFaviconLoaderFactory::GetForProfile(self.profile);
  TemplateURLService* templateURLService =
      ios::TemplateURLServiceFactory::GetForProfile(self.profile);
  _mediator = [[ComposeboxInputPlateMediator alloc]
      initWithContextualSearchSession:std::move(contextualSearchSession)
                         webStateList:self.browser->GetWebStateList()
                        faviconLoader:faviconLoader
               persistTabContextAgent:PersistTabContextBrowserAgent::
                                          FromBrowser(self.browser)
                          isIncognito:self.isOffTheRecord
                           modeHolder:_modeHolder
                   templateURLService:templateURLService
                aimEligibilityService:IOSChromeAimEligibilityServiceFactory::
                                          GetForProfile(self.profile)];
  _mediator.URLLoader = _URLLoader;
  _mediator.consumer = _viewController;
  _mediator.delegate = self;
  _mediator.metricsRecorder = _metricsRecorder;

  _viewController.mutator = _mediator;
  _voiceSearchController.dispatcher = _mediator;

  _locationBar = std::make_unique<WebLocationBarImpl>(self);
  _locationBar->SetURLLoader(self);
  _locationBarModelDelegate.reset(
      new LocationBarModelDelegateIOS(self, self.profile));
  _locationBarModel = std::make_unique<LocationBarModelImpl>(
      _locationBarModelDelegate.get(), kMaxURLDisplayChars);

  auto omniboxClient = std::make_unique<ComposeboxOmniboxClient>(
      _locationBar.get(), self.browser,
      feature_engagement::TrackerFactory::GetForProfile(self.profile),
      _mediator);

  _omniboxCoordinator = [[OmniboxCoordinator alloc]
      initWithBaseViewController:nil
                         browser:self.browser
                   omniboxClient:std::move(omniboxClient)
             presentationContext:OmniboxPresentationContext::kComposebox];
  _omniboxCoordinator.presenterDelegate = self.omniboxPopupPresenterDelegate;
  [_omniboxCoordinator start];

  [_omniboxCoordinator.managedViewController
      willMoveToParentViewController:_viewController];
  [_viewController
      addChildViewController:_omniboxCoordinator.managedViewController];
  [_viewController setEditView:_omniboxCoordinator.editView];
  _omniboxCoordinator.editView.heightDelegate = _mediator;
  [_omniboxCoordinator.managedViewController
      didMoveToParentViewController:_viewController];

  [_omniboxCoordinator updateOmniboxState];
  [_omniboxCoordinator focusOmnibox];
}

- (void)stop {
  if (_tabPickerCoordinator.started) {
    [_tabPickerCoordinator stop];
  }
  [_metricsRecorder recordAttachmentButtonsUsageInSession];

  _viewController.mutator = nil;
  _viewController = nil;
  _picker = nil;
  [_voiceSearchController dismissMicPermissionHelp];
  [_voiceSearchController disconnect];
  _voiceSearchController.dispatcher = nil;
  _voiceSearchController = nil;
  [_mediator disconnect];
  _mediator = nil;
  [_omniboxCoordinator endEditing];
  [_omniboxCoordinator stop];
  _omniboxCoordinator = nil;
  _metricsRecorder = nil;
}

- (UIViewController*)inputViewController {
  return _viewController;
}

#pragma mark - ComposeboxInputPlateViewControllerDelegate

- (void)composeboxViewController:
            (ComposeboxInputPlateViewController*)composeboxViewController
                 didTapMicButton:(UIButton*)micButton {
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

- (void)composeboxViewControllerDidTapGalleryButton:
    (ComposeboxInputPlateViewController*)composeboxViewController {
  [_metricsRecorder
      recordAttachmentButtonUsed:FuseboxAttachmentButtonType::kGallery];
  if (![_mediator canAddMoreAttachments]) {
    [self showMaxAttachmentSnackbarError];
    return;
  }
    [self
        composeboxViewControllerMayShowGalleryPicker:composeboxViewController];
  [_viewController presentViewController:_picker animated:YES completion:nil];
}

- (void)composeboxViewControllerDidTapCameraButton:
    (ComposeboxInputPlateViewController*)composeboxViewController {
  [_metricsRecorder
      recordAttachmentButtonUsed:FuseboxAttachmentButtonType::kCamera];
  if (![_mediator canAddMoreAttachments]) {
    [self showMaxAttachmentSnackbarError];
    return;
  }
  if (![UIImagePickerController
          isSourceTypeAvailable:UIImagePickerControllerSourceTypeCamera]) {
    // TODO(crbug.com/40280872): Show an error to the user.
    return;
  }

  UIImagePickerController* picker = [[UIImagePickerController alloc] init];
  picker.delegate = self;
  picker.sourceType = UIImagePickerControllerSourceTypeCamera;
  [_viewController presentViewController:picker animated:YES completion:nil];
}

- (void)composeboxViewControllerMayShowGalleryPicker:
    (ComposeboxInputPlateViewController*)composeboxViewController {
  PHPickerConfiguration* config = [[PHPickerConfiguration alloc]
      initWithPhotoLibrary:PHPhotoLibrary.sharedPhotoLibrary];
  config.selectionLimit = [_mediator maxNumberOfGalleryItemsAllowed];
  config.filter = [PHPickerFilter imagesFilter];
  _picker = [[PHPickerViewController alloc] initWithConfiguration:config];
  _picker.delegate = self;
}

- (void)composeboxViewControllerDidTapFileButton:
    (ComposeboxInputPlateViewController*)composeboxViewController {
  [_metricsRecorder
      recordAttachmentButtonUsed:FuseboxAttachmentButtonType::kFiles];
  if (![_mediator canAddMoreAttachments]) {
    [self showMaxAttachmentSnackbarError];
    return;
  }
  UIDocumentPickerViewController* picker =
      [[UIDocumentPickerViewController alloc]
          initForOpeningContentTypes:@[ UTTypePDF ]];
  picker.allowsMultipleSelection = NO;
  picker.delegate = self;
  [_viewController presentViewController:picker animated:YES completion:nil];
}

- (void)composeboxViewControllerDidTapAttachTabsButton:
    (ComposeboxInputPlateViewController*)viewController {
  if (![_mediator canAddMoreAttachments]) {
    [self showMaxAttachmentSnackbarError];
    return;
  }
  [self showComposeboxTabPicker];
}

- (void)composeboxViewControllerDidTapAIMButton:
            (ComposeboxInputPlateViewController*)viewController
                               activationSource:
                                   (AiModeActivationSource)activationSource {
  if (_modeHolder.mode == ComposeboxMode::kAIM) {
    _modeHolder.mode = ComposeboxMode::kRegularSearch;
  } else {
    _modeHolder.mode = ComposeboxMode::kAIM;
    [_metricsRecorder recordAiModeActivationSource:activationSource];
  }
}

- (void)composeboxViewControllerDidTapImageGenerationButton:
    (ComposeboxInputPlateViewController*)composeboxViewController {
  if (_modeHolder.mode == ComposeboxMode::kImageGeneration) {
    _modeHolder.mode = ComposeboxMode::kRegularSearch;
  } else {
    _modeHolder.mode = ComposeboxMode::kImageGeneration;
  }
}

- (void)composeboxViewController:
            (ComposeboxInputPlateViewController*)composeboxViewController
                didTapSendButton:(UIButton*)button {
  [_omniboxCoordinator acceptInput];
}

#pragma mark - PHPickerViewControllerDelegate

- (void)picker:(PHPickerViewController*)picker
    didFinishPicking:(NSArray<PHPickerResult*>*)results {
  [picker dismissViewControllerAnimated:YES completion:nil];
  _picker = nil;
  if (results.count == 0) {
    return;
  }

  for (PHPickerResult* result in results) {
    [_mediator processImageItemProvider:result.itemProvider
                                assetID:result.assetIdentifier];
  }
}

#pragma mark - UIDocumentPickerDelegate

- (void)documentPicker:(UIDocumentPickerViewController*)controller
    didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls {
  if (urls.count == 0) {
    return;
  }
  [_mediator processPDFFileURL:net::GURLWithNSURL(urls.firstObject)];
}

#pragma mark - UIImagePickerControllerDelegate

- (void)imagePickerController:(UIImagePickerController*)picker
    didFinishPickingMediaWithInfo:(NSDictionary<NSString*, id>*)info {
  [picker dismissViewControllerAnimated:YES completion:nil];
  UIImage* image = info[UIImagePickerControllerOriginalImage];
  if (!image) {
    return;
  }
  NSItemProvider* provider = [[NSItemProvider alloc] initWithObject:image];
  [_mediator processImageItemProvider:provider assetID:nil];
}

- (void)imagePickerControllerDidCancel:(UIImagePickerController*)picker {
  [picker dismissViewControllerAnimated:YES completion:nil];
}

#pragma mark - ComposeboxInputPlateMediatorDelegate

- (void)reloadAutocompleteSuggestionsRestarting:(BOOL)restart {
  [_omniboxCoordinator clearSuggestionsWithRestartAutocomplete:restart];
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

#pragma mark - ComposeboxTabPickerCommands

- (void)showComposeboxTabPicker {
  [_metricsRecorder
      recordAttachmentButtonUsed:FuseboxAttachmentButtonType::kTabPicker];

  _tabPickerCoordinator = [[ComposeboxTabPickerCoordinator alloc]
      initWithBaseViewController:_viewController
                         browser:self.browser];
  _tabPickerCoordinator.delegate = _mediator;
  _tabPickerCoordinator.composeboxTabPickerHandler = self;
  [_tabPickerCoordinator start];
}

- (void)hideComposeboxTabPicker {
  [_tabPickerCoordinator stop];
  _tabPickerCoordinator = nil;
}

#pragma mark - Private helpers

/// Dismisses the composebox via a command to the browser coordinator.
- (void)dismissComposebox {
  id<BrowserCoordinatorCommands> commands = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), BrowserCoordinatorCommands);
  [commands hideComposeboxImmediately:NO];
}

/// Displays a snackbar error indicating the maximum number of attachments has
/// been reached.
- (void)showMaxAttachmentSnackbarError {
  ComposeboxSnackbarPresenter* snackbar =
      [[ComposeboxSnackbarPresenter alloc] initWithBrowser:self.browser];
  CGFloat offset = _viewController.keyboardHeight;
  if (!_theme.isTopInputPlate) {
    offset += _viewController.inputHeight + kSnackbarBottomMargin;
  }
  [snackbar showAttachmentLimitSnackbarWithBottomOffset:offset];
}

/// Displays a snackbar error indicating that attachment failed to be added.
- (void)showUnableToAddAttachmentSnackbarError {
  ComposeboxSnackbarPresenter* snackbar =
      [[ComposeboxSnackbarPresenter alloc] initWithBrowser:self.browser];
  CGFloat offset = _viewController.keyboardHeight;
  if (!_theme.isTopInputPlate) {
    offset += _viewController.inputHeight + kSnackbarBottomMargin;
  }
  [snackbar showUnableToAddAttachmentSnackbarWithBottomOffset:offset];
}

@end
