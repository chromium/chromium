// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/coordinator/aim_prototype_coordinator.h"

#import "components/application_locale_storage/application_locale_storage.h"
#import "components/omnibox/composebox/ios/composebox_query_controller_ios.h"
#import "components/search_engines/template_url_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/aim/prototype/coordinator/aim_prototype_mediator.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_dismiss_animator.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_present_animator.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_view_controller+private.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_view_controller.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/variations/model/client/variations_client_service.h"
#import "ios/chrome/browser/variations/model/client/variations_client_service_factory.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/public/provider/chrome/browser/voice_search/voice_search_api.h"
#import "ios/web/public/web_state.h"
#import "net/base/apple/url_conversions.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

@interface AIMPrototypeCoordinator () <AIMPrototypeMediatorDelegate,
                                       AIMPrototypeViewControllerDelegate,
                                       PHPickerViewControllerDelegate,
                                       UIDocumentPickerDelegate,
                                       UIImagePickerControllerDelegate,
                                       UINavigationControllerDelegate,
                                       UIViewControllerTransitioningDelegate>
@end

@implementation AIMPrototypeCoordinator {
  AIMPrototypeViewController* _viewController;
  AIMPrototypeMediator* _mediator;
  id<VoiceSearchController> _voiceSearchController;
  /// The prewarmed picker as it takes time to appear.
  PHPickerViewController* _picker;
}

- (void)start {
  _viewController = [[AIMPrototypeViewController alloc] init];
  _viewController.delegate = self;
  _viewController.modalPresentationStyle = UIModalPresentationCustom;
  _viewController.transitioningDelegate = self;

  _voiceSearchController =
      ios::provider::CreateVoiceSearchController(self.browser);

  UrlLoadingBrowserAgent* urlLoadingBrowserAgent =
      UrlLoadingBrowserAgent::FromBrowser(self.browser);
  TemplateURLService* templateURLService =
      ios::TemplateURLServiceFactory::GetForProfile(self.profile);
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(self.profile);
  auto composeboxQueryController =
      std::make_unique<ComposeboxQueryControllerIOS>(
          identityManager, GetApplicationContext()->GetSharedURLLoaderFactory(),
          ::GetChannel(),
          GetApplicationContext()->GetApplicationLocaleStorage()->Get(),
          templateURLService,
          VariationsClientServiceFactory::GetForProfile(self.profile),
          /*send_lns_surface=*/false);

  _mediator = [[AIMPrototypeMediator alloc]
      initWithUrlLoadingBrowserAgent:urlLoadingBrowserAgent
           composeboxQueryController:std::move(composeboxQueryController)
                        webStateList:self.browser->GetWebStateList()];
  _mediator.consumer = _viewController;
  _mediator.delegate = self;
  _viewController.mutator = _mediator;
  _voiceSearchController.dispatcher = _mediator;

  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_viewController.presentingViewController dismissViewControllerAnimated:YES
                                                               completion:nil];
  _viewController = nil;
  _picker = nil;
  [_voiceSearchController dismissMicPermissionHelp];
  [_voiceSearchController disconnect];
  _voiceSearchController.dispatcher = nil;
  _voiceSearchController = nil;
  [_mediator disconnect];
  _mediator = nil;
}

#pragma mark - UIViewControllerTransitioningDelegate

- (id<UIViewControllerAnimatedTransitioning>)
    animationControllerForPresentedController:(UIViewController*)presented
                         presentingController:(UIViewController*)presenting
                             sourceController:(UIViewController*)source {
  return [[AIMPrototypePresentAnimator alloc]
      initWithContextProvider:_viewController];
}

- (id<UIViewControllerAnimatedTransitioning>)
    animationControllerForDismissedController:(UIViewController*)dismissed {
  return [[AIMPrototypeDismissAnimator alloc]
      initWithContextProvider:_viewController];
}

#pragma mark - AIMPrototypeViewControllerDelegate

- (void)aimPrototypeViewControllerDidTapCloseButton:
    (AIMPrototypeViewController*)viewController {
  [self.delegate aimPrototypeCoordinatorDidFinish:self];
}

- (void)aimPrototypeViewControllerDidTapMicButton:
    (AIMPrototypeViewController*)viewController {
  WebStateList* webStateList = self.browser->GetWebStateList();
  if (!webStateList) {
    return;
  }
  web::WebState* webState = webStateList->GetActiveWebState();
  if (!webState) {
    return;
  }
  [_voiceSearchController startRecognitionOnViewController:_viewController
                                                  webState:webState];
}

- (void)aimPrototypeViewControllerDidTapGalleryButton:
    (AIMPrototypeViewController*)viewController {
  if (!_picker) {
    [self aimPrototypeViewControllerMayShowGalleryPicker:viewController];
  }
  [_viewController presentViewController:_picker animated:YES completion:nil];
  _picker = nil;
}

- (void)aimPrototypeViewControllerDidTapCameraButton:
    (AIMPrototypeViewController*)viewController {
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
    (AIMPrototypeViewController*)viewController {
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
    (AIMPrototypeViewController*)viewController {
  UIDocumentPickerViewController* picker =
      [[UIDocumentPickerViewController alloc]
          initForOpeningContentTypes:@[ UTTypePDF ]];
  picker.allowsMultipleSelection = NO;
  picker.delegate = self;
  [_viewController presentViewController:picker animated:YES completion:nil];
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

#pragma mark - AIMPrototypeMediatorDelegate

- (void)dismissAimPrototype {
  [self.delegate aimPrototypeCoordinatorDidFinish:self];
}

@end
