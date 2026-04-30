// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/menu/coordinator/composebox_menu_coordinator.h"

#import <memory>
#import <set>

#import "components/contextual_search/contextual_search_service.h"
#import "components/contextual_search/contextual_search_session_handle.h"
#import "ios/chrome/browser/aim/model/ios_chrome_aim_eligibility_service_factory.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_input_state_manager.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_mode_holder.h"
#import "ios/chrome/browser/composebox/menu/coordinator/composebox_menu_mediator.h"
#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_view_controller.h"
#import "ios/chrome/browser/composebox/model/ios_contextual_search_service_factory.h"
#import "ios/chrome/browser/composebox/shared/coordinator/composebox_picker_presenter.h"
#import "ios/chrome/browser/composebox/ui/composebox_ui_input_state.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/web/public/web_state_id.h"

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
  ComposeboxUIInputState* _inputState;
  // Whether the menu is invoked standalone and should manage its own state.
  BOOL _isStandaloneMenu;
  // Resources owned by the coordinator in standalone mode.
  ComposeboxInputStateManager* _stateManager;
  std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
      _sessionHandle;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                entrypoint:(ComposeboxEntrypoint)entrypoint
                                inputState:(ComposeboxUIInputState*)inputState {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _entrypoint = entrypoint;
    _inputState = inputState;
    _isStandaloneMenu = (inputState == nil);
  }

  return self;
}

- (void)start {
  _viewController = [[ComposeboxMenuViewController alloc] init];

  if (_isStandaloneMenu) {
    ProfileIOS* profile = self.browser->GetProfile();

    contextual_search::ContextualSearchService* service =
        ContextualSearchServiceFactory::GetForProfile(profile);

    auto configParams = std::make_unique<
        contextual_search::ContextualSearchContextController::ConfigParams>();
    _sessionHandle = service->CreateSession(
        std::move(configParams),
        contextual_search::ContextualSearchSource::kNewTabPage,
        lens::LensOverlayInvocationSource::kNtpContextualQuery);

    ComposeboxModeHolder* modeHolder = [[ComposeboxModeHolder alloc] init];

    AimEligibilityService* aimEligibilityService =
        IOSChromeAimEligibilityServiceFactory::GetForProfile(profile);

    signin::IdentityManager* identityManager =
        IdentityManagerFactory::GetForProfile(profile);

    TemplateURLService* templateURLService =
        ::ios::TemplateURLServiceFactory::GetForProfile(profile);

    _stateManager = [[ComposeboxInputStateManager alloc]
         initWithWebStateList:self.browser->GetWebStateList()
                   modeHolder:modeHolder
                  prefService:profile->GetPrefs()
        aimEligibilityService:aimEligibilityService
              identityManager:identityManager
           templateURLService:templateURLService
                sessionHandle:_sessionHandle.get()
                   entrypoint:_entrypoint
                  isIncognito:profile->IsOffTheRecord()];

    std::set<web::WebStateID> emptySet;
    ComposeboxUIInputState* inputState =
        [_stateManager computeUIInputStateWithFavicon:nil
                                  attachedWebStateIDs:emptySet];

    _mediator = [[ComposeboxMenuMediator alloc] initWithEntrypoint:_entrypoint
                                                        inputState:inputState];
  } else {
    CHECK(_inputState);
    _mediator = [[ComposeboxMenuMediator alloc] initWithEntrypoint:_entrypoint
                                                        inputState:_inputState];
  }
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
  _mediator.consumer = _viewController;

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
  [_stateManager disconnect];
  _stateManager = nil;
  _sessionHandle.reset();
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

- (void)composeboxMenuMediatorDidRequestFileSelection:
    (ComposeboxMenuMediator*)mediator {
  // TODO(crbug.com/506955766): Unify metrics recording and record this action.

  if (![_mediator canAddMoreAttachments]) {
    [self showMaxAttachmentSnackbarError];
    return;
  }
  [_pickerPresenter presentFilePicker];
}

#pragma mark - ComposeboxPickerPresenterDelegate

- (void)composeboxPickerPresenter:(ComposeboxPickerPresenter*)presenter
                    didPickImages:
                        (NSArray<ComposeboxPickerImageResult*>*)results {
  [_mediator processImageItems:results];
}

- (void)composeboxPickerPresenterDidDissmissCamera:
    (ComposeboxPickerPresenter*)presenter {
  // NO-OP.
}

- (void)composeboxPickerPresenter:(ComposeboxPickerPresenter*)presenter
             didPickFilesWithURLs:(NSArray<NSURL*>*)urls {
  [_mediator processFileURLs:urls];
}

#pragma mark - Private

/// Displays a snackbar error indicating the maximum number of attachments has
/// been reached.
- (void)showMaxAttachmentSnackbarError {
  // TODO(crbug.com/506956765): Implement.
}

@end
