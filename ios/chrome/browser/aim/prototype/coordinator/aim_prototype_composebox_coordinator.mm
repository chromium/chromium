// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/coordinator/aim_prototype_composebox_coordinator.h"

#import "components/application_locale_storage/application_locale_storage.h"
#import "components/omnibox/browser/location_bar_model_impl.h"
#import "components/omnibox/composebox/ios/composebox_query_controller_ios.h"
#import "components/search_engines/template_url_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/aim/prototype/coordinator/aim_omnibox_client.h"
#import "ios/chrome/browser/aim/prototype/coordinator/aim_prototype_composebox_mediator.h"
#import "ios/chrome/browser/aim/prototype/coordinator/aim_prototype_entrypoint.h"
#import "ios/chrome/browser/aim/prototype/coordinator/aim_prototype_tab_picker_coordinator.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_composebox_view_controller.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
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
}

@interface AIMPrototypeComposeboxCoordinator () <
    AIMPrototypeComposeboxViewControllerDelegate,
    LocationBarModelDelegateWebStateProvider,
    LocationBarURLLoader,
    PHPickerViewControllerDelegate,
    UIDocumentPickerDelegate,
    UIImagePickerControllerDelegate,
    UINavigationControllerDelegate,
    UIViewControllerTransitioningDelegate,
    WebLocationBarDelegate>
@end

@implementation AIMPrototypeComposeboxCoordinator {
  AIMPrototypeComposeboxViewController* _viewController;
  AIMPrototypeComposeboxMediator* _mediator;
  id<VoiceSearchController> _voiceSearchController;
  /// The prewarmed picker as it takes time to appear.
  PHPickerViewController* _picker;
  /// The entrypoing from which the coordinator was invoked.
  AIMPrototypeEntrypoint _entrypoint;
  /// Optional query inserted into the omnibox at start.
  NSString* _query;
  /// The URLLoader to pass to the mediator.
  __weak id<AIMPrototypeURLLoader> _URLLoader;
  /// Coordinator of the omnibox.
  OmniboxCoordinator* _omniboxCoordinator;
  // API endpoint for omnibox.
  std::unique_ptr<WebLocationBarImpl> _locationBar;
  std::unique_ptr<LocationBarModelDelegateIOS> _locationBarModelDelegate;
  std::unique_ptr<LocationBarModel> _locationBarModel;
  AimPrototypeTabPickerCoordinator* _tabPickerCoordinator;
}

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                                entrypoint:(AIMPrototypeEntrypoint)entrypoint
                                     query:(NSString*)query
                                 URLLoader:
                                     (id<AIMPrototypeURLLoader>)URLLoader {
  self = [super initWithBaseViewController:baseViewController browser:browser];
  if (self) {
    _entrypoint = entrypoint;
    _query = query;
    _URLLoader = URLLoader;
  }
  return self;
}

- (void)start {
  _viewController = [[AIMPrototypeComposeboxViewController alloc] init];
  _viewController.delegate = self;

  _tabPickerCoordinator = [[AimPrototypeTabPickerCoordinator alloc]
      initWithBaseViewController:_viewController
                         browser:self.browser];

  _voiceSearchController =
      ios::provider::CreateVoiceSearchController(self.browser);

  TemplateURLService* templateURLService =
      ios::TemplateURLServiceFactory::GetForProfile(self.profile);
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(self.profile);
  auto query_contoller_config_params = std::make_unique<
      ComposeboxQueryController::QueryControllerConfigParams>();
  query_contoller_config_params->send_lns_surface = false;
  query_contoller_config_params->enable_multi_context_input_flow = false;
  query_contoller_config_params->enable_viewport_images = true;
  auto composeboxQueryController =
      std::make_unique<ComposeboxQueryControllerIOS>(
          identityManager, GetApplicationContext()->GetSharedURLLoaderFactory(),
          ::GetChannel(),
          GetApplicationContext()->GetApplicationLocaleStorage()->Get(),
          templateURLService,
          VariationsClientServiceFactory::GetForProfile(self.profile),
          std::move(query_contoller_config_params));

  FaviconLoader* faviconLoader =
      IOSChromeFaviconLoaderFactory::GetForProfile(self.profile);
  _mediator = [[AIMPrototypeComposeboxMediator alloc]
      initWithComposeboxQueryController:std::move(composeboxQueryController)
                           webStateList:self.browser->GetWebStateList()
                          faviconLoader:faviconLoader];
  _mediator.URLLoader = _URLLoader;
  _mediator.consumer = _viewController;
  _viewController.mutator = _mediator;
  _voiceSearchController.dispatcher = _mediator;

  _locationBar = std::make_unique<WebLocationBarImpl>(self);
  _locationBar->SetURLLoader(self);
  _locationBarModelDelegate.reset(
      new LocationBarModelDelegateIOS(self, self.profile));
  _locationBarModel = std::make_unique<LocationBarModelImpl>(
      _locationBarModelDelegate.get(), kMaxURLDisplayChars);

  auto omniboxClient = std::make_unique<AIMOmniboxClient>(
      _locationBar.get(), self.browser,
      feature_engagement::TrackerFactory::GetForProfile(self.profile),
      _mediator);

  _omniboxCoordinator = [[OmniboxCoordinator alloc]
      initWithBaseViewController:nil
                         browser:self.browser
                   omniboxClient:std::move(omniboxClient)
             presentationContext:OmniboxPresentationContext::kAIMPrototype];
  _omniboxCoordinator.presenterDelegate = self.omniboxPopupPresenterDelegate;
  [_omniboxCoordinator start];

  [_omniboxCoordinator.managedViewController
      willMoveToParentViewController:_viewController];
  [_viewController
      addChildViewController:_omniboxCoordinator.managedViewController];
  [_viewController setEditView:_omniboxCoordinator.editView];
  [_omniboxCoordinator.managedViewController
      didMoveToParentViewController:_viewController];

  [_omniboxCoordinator updateOmniboxState];
  [_omniboxCoordinator focusOmnibox];
}

- (void)stop {
  if (_tabPickerCoordinator.started) {
    [_tabPickerCoordinator stop];
  }
  _viewController.mutator = nil;
  _viewController = nil;
  _picker = nil;
  [_voiceSearchController dismissMicPermissionHelp];
  [_voiceSearchController disconnect];
  _voiceSearchController.dispatcher = nil;
  _voiceSearchController = nil;
  [_mediator disconnect];
  _mediator.URLLoader = nil;
  _mediator.consumer = nil;
  _mediator = nil;
  [_omniboxCoordinator endEditing];
  [_omniboxCoordinator stop];
  _omniboxCoordinator = nil;
}

- (UIViewController*)inputViewController {
  return _viewController;
}

- (id<AIMPrototypeAnimationContextProvider>)contextProvider {
  return _viewController;
}

#pragma mark - AIMPrototypeComposeboxViewControllerDelegate

- (void)aimPrototypeViewController:
            (AIMPrototypeComposeboxViewController*)composeboxViewController
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

- (void)aimPrototypeViewController:
            (AIMPrototypeComposeboxViewController*)composeboxViewController
                  didTapLensButton:(UIButton*)lensButton {
  OpenLensInputSelectionCommand* command = [[OpenLensInputSelectionCommand
      alloc]
          // TODO(crbug.com/452307696) : Add and update the entrypoint to
          // reflect on the aim composebox.
          initWithEntryPoint:LensEntrypoint::Keyboard
           presentationStyle:LensInputSelectionPresentationStyle::SlideFromRight
      presentationCompletion:nil];
  id<LensCommands> handler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), LensCommands);
  [handler openLensInputSelection:command];
}

- (void)aimPrototypeViewControllerDidTapGalleryButton:
    (AIMPrototypeComposeboxViewController*)composeboxViewController {
  if (!_picker) {
    [self aimPrototypeViewControllerMayShowGalleryPicker:
              composeboxViewController];
  }
  [_viewController presentViewController:_picker animated:YES completion:nil];
  _picker = nil;
}

- (void)aimPrototypeViewControllerDidTapCameraButton:
    (AIMPrototypeComposeboxViewController*)composeboxViewController {
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

- (void)aimPrototypeViewControllerMayShowGalleryPicker:
    (AIMPrototypeComposeboxViewController*)composeboxViewController {
  if (_picker) {
    return;
  }
  PHPickerConfiguration* config = [[PHPickerConfiguration alloc]
      initWithPhotoLibrary:PHPhotoLibrary.sharedPhotoLibrary];
  config.selectionLimit = 1;
  config.filter = [PHPickerFilter imagesFilter];
  _picker = [[PHPickerViewController alloc] initWithConfiguration:config];
  _picker.delegate = self;
}

- (void)aimPrototypeViewControllerDidTapFileButton:
    (AIMPrototypeComposeboxViewController*)composeboxViewController {
  UIDocumentPickerViewController* picker =
      [[UIDocumentPickerViewController alloc]
          initForOpeningContentTypes:@[ UTTypePDF ]];
  picker.allowsMultipleSelection = NO;
  picker.delegate = self;
  [_viewController presentViewController:picker animated:YES completion:nil];
}

- (void)aimPrototypeViewControllerDidTapAttachTabsButton:
    (AIMPrototypeComposeboxViewController*)viewController {
  [_tabPickerCoordinator start];
}

#pragma mark - PHPickerViewControllerDelegate

- (void)picker:(PHPickerViewController*)picker
    didFinishPicking:(NSArray<PHPickerResult*>*)results {
  [picker dismissViewControllerAnimated:YES completion:nil];

  if (results.count == 0) {
    return;
  }

  NSItemProvider* provider = results.firstObject.itemProvider;
  [_mediator processImageItemProvider:provider];
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
  [_mediator processImageItemProvider:provider];
}

- (void)imagePickerControllerDidCancel:(UIImagePickerController*)picker {
  [picker dismissViewControllerAnimated:YES completion:nil];
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

  [self dismissAIMPrototype];
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

#pragma mark - Private

- (void)dismissAIMPrototype {
  id<BrowserCoordinatorCommands> commands = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), BrowserCoordinatorCommands);
  [commands hideAIMPrototype];
}

@end
