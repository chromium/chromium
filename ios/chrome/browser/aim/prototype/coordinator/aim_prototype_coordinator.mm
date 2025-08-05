// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/coordinator/aim_prototype_coordinator.h"

#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/aim/prototype/coordinator/aim_prototype_mediator.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_view_controller.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"

@interface AIMPrototypeCoordinator () <AIMPrototypeMediatorDelegate>
@end

@implementation AIMPrototypeCoordinator {
  AIMPrototypeViewController* _viewController;
  AIMPrototypeMediator* _mediator;
}

- (void)start {
  _viewController = [[AIMPrototypeViewController alloc] init];
  _viewController.delegate = self;
  _viewController.modalPresentationStyle = UIModalPresentationFullScreen;

  UrlLoadingBrowserAgent* urlLoadingBrowserAgent =
      UrlLoadingBrowserAgent::FromBrowser(self.browser);
  TemplateURLService* templateURLService =
      ios::TemplateURLServiceFactory::GetForProfile(self.profile);
  _mediator = [[AIMPrototypeMediator alloc]
      initWithUrlLoadingBrowserAgent:urlLoadingBrowserAgent
                  templateURLService:templateURLService];
  _mediator.consumer = _viewController;
  _mediator.delegate = self;
  _viewController.mutator = _mediator;

  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_viewController.presentingViewController dismissViewControllerAnimated:YES
                                                               completion:nil];
  _viewController = nil;
  [_mediator disconnect];
  _mediator = nil;
}

#pragma mark - AIMPrototypeViewControllerDelegate

- (void)aimPrototypeViewControllerDidTapCloseButton:
    (AIMPrototypeViewController*)viewController {
  [self.delegate aimPrototypeCoordinatorDidFinish:self];
}

- (void)aimPrototypeViewControllerDidTapGalleryButton:
    (AIMPrototypeViewController*)viewController {
  PHPickerConfiguration* config = [[PHPickerConfiguration alloc]
      initWithPhotoLibrary:PHPhotoLibrary.sharedPhotoLibrary];
  config.selectionLimit = 1;
  config.filter = [PHPickerFilter imagesFilter];
  PHPickerViewController* picker =
      [[PHPickerViewController alloc] initWithConfiguration:config];
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
  if ([provider canLoadObjectOfClass:[UIImage class]]) {
    __weak AIMPrototypeMediator* weakMediator = _mediator;
    [provider loadObjectOfClass:[UIImage class]
              completionHandler:^(__kindof id<NSItemProviderReading> object,
                                  NSError* error) {
                dispatch_async(dispatch_get_main_queue(), ^{
                  [weakMediator processImage:(UIImage*)object];
                });
              }];
  }
}

#pragma mark - AIMPrototypeMediatorDelegate

- (void)dismissAimPrototype {
  [self.delegate aimPrototypeCoordinatorDidFinish:self];
}

@end
