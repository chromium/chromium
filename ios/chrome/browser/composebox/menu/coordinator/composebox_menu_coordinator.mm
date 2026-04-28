// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/menu/coordinator/composebox_menu_coordinator.h"

#import "ios/chrome/browser/composebox/menu/coordinator/composebox_menu_mediator.h"
#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_view_controller.h"
#import "ios/chrome/browser/composebox/shared/coordinator/composebox_picker_presenter.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"

namespace {

// Custom detent identifier to fit the collection view by
// the `preferredContentSize`.
NSString* const kCustomFittingDetentIdentifier = @"kFittingDetentIdentifier";

}  // namespace

@interface ComposeboxMenuCoordinator () <ComposeboxMenuMediatorDelegate,
                                         ComposeboxPickerPresenterDelegate,
                                         UISheetPresentationControllerDelegate>
@end

@implementation ComposeboxMenuCoordinator {
  ComposeboxMenuViewController* _viewController;
  ComposeboxMenuMediator* _mediator;
  ComposeboxEntrypoint _entrypoint;
  ComposeboxPickerPresenter* _pickerPresenter;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                entrypoint:(ComposeboxEntrypoint)entrypoint {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _entrypoint = entrypoint;
  }

  return self;
}

- (void)start {
  _viewController = [[ComposeboxMenuViewController alloc] init];
  _mediator = [[ComposeboxMenuMediator alloc] initWithEntrypoint:_entrypoint];
  _mediator.delegate = self;

  _viewController.sheetPresentationController.prefersGrabberVisible = YES;
  _viewController.sheetPresentationController.delegate = self;
  _viewController.sheetPresentationController
      .prefersEdgeAttachedInCompactHeight = YES;

  __weak UIViewController* weakVC = _viewController;
  auto detentResolver = ^CGFloat(
      id<UISheetPresentationControllerDetentResolutionContext> context) {
    return weakVC.preferredContentSize.height;
  };
  _viewController.sheetPresentationController.detents =
      @[ [UISheetPresentationControllerDetent
          customDetentWithIdentifier:kCustomFittingDetentIdentifier
                            resolver:detentResolver] ];

  _viewController.mutator = _mediator;

  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];

  _pickerPresenter = [[ComposeboxPickerPresenter alloc]
      initWithBaseViewController:_viewController];
  _pickerPresenter.delegate = self;
}

- (void)stop {
  [_viewController dismissViewControllerAnimated:YES completion:nil];
  _viewController = nil;
  _mediator = nil;
  _pickerPresenter = nil;
}

#pragma mark - UISheetPresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.delegate composeboxMenuCoordinatorDidDismissMenu:self];
}

#pragma mark - ComposeboxMenuMediatorDelegate

- (void)composeboxMenuMediatorDidProduceFocusParams:
    (ComposeboxFocusParams*)focusParams {
  __weak id<BrowserCoordinatorCommands> commands = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), BrowserCoordinatorCommands);

  [_viewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           [commands showComposeboxWithParams:focusParams];
                         }];
}

- (void)composeboxMenuMediatorDidRequestCameraSelection:
    (ComposeboxMenuMediator*)mediator {
  // TODO(crbug.com/506955766): Unify metrics recording and record this action.

  if (![_mediator canAddMoreAttachments]) {
    [self showMaxAttachmentSnackbarError];
    return;
  }

  [_pickerPresenter presentCameraPicker];
}

- (void)composeboxMenuMediatorDidRequestGallerySelection:
    (ComposeboxMenuMediator*)mediator {
  // TODO(crbug.com/506955766): Unify metrics recording and record this action.

  if (![_mediator canAddMoreAttachments]) {
    [self showMaxAttachmentSnackbarError];
    return;
  }

  [_pickerPresenter
      presentGalleryPickerWithLimit:[_mediator remainingNumberOfImagesAllowed]];
}

#pragma mark - ComposeboxPickerPresenterDelegate

- (void)composeboxPickerPresenter:(ComposeboxPickerPresenter*)presenter
                    didPickImages:
                        (NSArray<ComposeboxPickerImageResult*>*)results {
  [_mediator processImageItems:results];
}

#pragma mark - Private

/// Displays a snackbar error indicating the maximum number of attachments has
/// been reached.
- (void)showMaxAttachmentSnackbarError {
  // TODO(crbug.com/506956765): Implement.
}

@end
