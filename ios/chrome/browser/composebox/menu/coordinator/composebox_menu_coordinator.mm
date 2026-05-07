// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/menu/coordinator/composebox_menu_coordinator.h"

#import <memory>
#import <set>
#import <vector>

#import "components/contextual_search/contextual_search_service.h"
#import "components/contextual_search/contextual_search_session_handle.h"
#import "components/omnibox/browser/aim_eligibility_service.h"
#import "ios/chrome/browser/aim/model/ios_chrome_aim_eligibility_service_factory.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_input_state_manager.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_mode_holder.h"
#import "ios/chrome/browser/composebox/menu/coordinator/composebox_menu_mediator.h"
#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_view_controller.h"
#import "ios/chrome/browser/composebox/model/ios_contextual_search_service_factory.h"
#import "ios/chrome/browser/composebox/public/composebox_attachment_selection.h"
#import "ios/chrome/browser/composebox/public/composebox_focus_params.h"
#import "ios/chrome/browser/composebox/shared/coordinator/composebox_picker_presenter.h"
#import "ios/chrome/browser/composebox/shared/metrics/composebox_metrics_recorder.h"
#import "ios/chrome/browser/composebox/ui/composebox_ui_input_state.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_utils.h"
#import "ios/web/public/web_state_id.h"
#import "third_party/omnibox_proto/searchbox_config.pb.h"

namespace {

// Custom detent identifier to fit the collection view by
// the `preferredContentSize`.
NSString* const kCustomFittingDetentIdentifier = @"kFittingDetentIdentifier";

/// Top padding above the sheet. Allows the user to drag the sheet down without
/// conflicting with system behavior.
CGFloat const kSheetTopPadding = 40.0f;

}  // namespace

@interface ComposeboxMenuCoordinator () <ComposeboxMenuMediatorDelegate,
                                         ComposeboxPickerPresenterDelegate,
                                         ComposeboxPickerPresenterDataSource,
                                         UISheetPresentationControllerDelegate>
@end

@implementation ComposeboxMenuCoordinator {
  ComposeboxMenuViewController* _viewController;
  ComposeboxMenuMediator* _mediator;
  ComposeboxEntrypoint _entrypoint;
  ComposeboxPickerPresenter* _pickerPresenter;
  ComposeboxUIInputState* _inputState;
  ComposeboxAttachmentSelection* _preselection;
  // Whether the menu is invoked standalone and should manage its own state.
  BOOL _isStandaloneMenu;
  // Resources owned by the coordinator in standalone mode.
  ComposeboxInputStateManager* _stateManager;
  std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
      _sessionHandle;

  // Metrics recorder
  ComposeboxMetricsRecorder* _metricsRecorder;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                    preselectedAttachments:
                        (ComposeboxAttachmentSelection*)preselectedAttachments
                                inputState:(ComposeboxUIInputState*)inputState
                           metricsRecorder:
                               (ComposeboxMetricsRecorder*)metricsRecorder
                                entrypoint:(ComposeboxEntrypoint)entrypoint {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _entrypoint = entrypoint;
    _preselection = preselectedAttachments;
    _inputState = inputState;
    _isStandaloneMenu = (inputState == nil);
    _metricsRecorder = metricsRecorder;
  }
  return self;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                entrypoint:(ComposeboxEntrypoint)entrypoint {
  return
      [self initWithBaseViewController:viewController
                               browser:browser
                preselectedAttachments:nil
                            inputState:nil
                       metricsRecorder:[[ComposeboxMetricsRecorder alloc] init]
                            entrypoint:entrypoint];
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
    _inputState = [_stateManager computeUIInputStateWithFavicon:nil
                                            attachedWebStateIDs:emptySet];
  }

  CHECK(_inputState);
  _mediator = [[ComposeboxMenuMediator alloc]
          initWithEntrypoint:_entrypoint
                  inputState:_inputState
                webStateList:self.browser->GetWebStateList()
      preselectedAttachments:_preselection];
  _mediator.delegate = self;

  _viewController.sheetPresentationController.prefersGrabberVisible = YES;
  _viewController.sheetPresentationController.delegate = self;
  _viewController.sheetPresentationController
      .prefersEdgeAttachedInCompactHeight = YES;

  if ([UIDevice currentDevice].userInterfaceIdiom ==
      UIUserInterfaceIdiomPhone) {
    _viewController.sheetPresentationController
        .widthFollowsPreferredContentSizeWhenEdgeAttached = YES;
  }

  __weak UIViewController* weakVC = _viewController;
  auto detentResolver = ^CGFloat(
      id<UISheetPresentationControllerDetentResolutionContext> context) {
    CGFloat contentHeight = weakVC.preferredContentSize.height;
    CGFloat maxAllowedHeight = context.maximumDetentValue - kSheetTopPadding;
    return contentHeight < maxAllowedHeight ? contentHeight : maxAllowedHeight;
  };
  _viewController.sheetPresentationController.detents =
      @[ [UISheetPresentationControllerDetent
          customDetentWithIdentifier:kCustomFittingDetentIdentifier
                            resolver:detentResolver] ];

  _viewController.mutator = _mediator;
  _mediator.consumer = _viewController;

  [self recordAttachmentsMenuOpen];
  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];

  _pickerPresenter = [[ComposeboxPickerPresenter alloc]
      initWithBaseViewController:_viewController
                         browser:self.browser];
  _pickerPresenter.delegate = self;
  _pickerPresenter.dataSource = self;
}

- (void)stop {
  [_viewController.presentingViewController dismissViewControllerAnimated:YES
                                                               completion:nil];
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

- (void)composeboxMenuMediator:(ComposeboxMenuMediator*)mediator
                    didTapTool:(ComposeboxMode)toolMode {
  if (_isStandaloneMenu) {
    ComposeboxFocusParams* focusParams = [[ComposeboxFocusParams alloc]
        initWithEntrypoint:_entrypoint
                     query:nil
                  toolMode:toolMode
                 modelMode:ComposeboxModelOption::kNone
            attachmentList:nil];
    __weak __typeof(self) weakSelf = self;
    [_viewController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:^{
                             [weakSelf showComposeboxWithParams:focusParams];
                           }];
  } else {
    [self.inputPlateDelegate composeboxMenuCoordinator:self
                                            didTapTool:toolMode];
    [_viewController dismissViewControllerAnimated:YES completion:nil];
  }
}

- (void)composeboxMenuMediator:(ComposeboxMenuMediator*)mediator
                   didTapModel:(ComposeboxModelOption)modelMode {
  if (_isStandaloneMenu) {
    ComposeboxFocusParams* focusParams = [[ComposeboxFocusParams alloc]
        initWithEntrypoint:_entrypoint
                     query:nil
                  toolMode:ComposeboxMode::kRegularSearch
                 modelMode:modelMode
            attachmentList:nil];
    __weak __typeof(self) weakSelf = self;
    [_viewController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:^{
                             [weakSelf showComposeboxWithParams:focusParams];
                           }];
  } else {
    [self.inputPlateDelegate composeboxMenuCoordinator:self
                                           didTapModel:modelMode];
    [_viewController dismissViewControllerAnimated:YES completion:nil];
  }
}

- (void)composeboxMenuMediator:(ComposeboxMenuMediator*)mediator
          didUpdateAttachments:(ComposeboxAttachmentSelection*)attachments {
  if (_isStandaloneMenu) {
    ComposeboxFocusParams* focusParams = [[ComposeboxFocusParams alloc]
        initWithEntrypoint:_entrypoint
                     query:nil
                  toolMode:ComposeboxMode::kRegularSearch
                 modelMode:ComposeboxModelOption::kNone
            attachmentList:attachments];
    __weak __typeof(self) weakSelf = self;
    [_viewController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:^{
                             [weakSelf showComposeboxWithParams:focusParams];
                           }];
  } else {
    [self.inputPlateDelegate composeboxMenuCoordinator:self
                                  didUpdateAttachments:attachments];
    [_viewController dismissViewControllerAnimated:YES completion:nil];
  }
}

- (void)composeboxMenuMediatorDidRequestCameraSelection:
    (ComposeboxMenuMediator*)mediator {
  [_metricsRecorder
      recordAttachmentButtonUsed:FuseboxAttachmentButtonType::kCamera];

  if (![_mediator canAddMoreAttachments]) {
    [self showMaxAttachmentSnackbarError];
    return;
  }

  [_pickerPresenter presentCameraPicker];
}

- (void)composeboxMenuMediatorDidRequestGallerySelection:
    (ComposeboxMenuMediator*)mediator {
  [_metricsRecorder
      recordAttachmentButtonUsed:FuseboxAttachmentButtonType::kGallery];

  if (![_mediator canAddMoreAttachments]) {
    [self showMaxAttachmentSnackbarError];
    return;
  }

  [_pickerPresenter
      presentGalleryPickerWithLimit:[_mediator remainingNumberOfImagesAllowed]];
}

- (void)composeboxMenuMediatorDidRequestFileSelection:
    (ComposeboxMenuMediator*)mediator {
  [_metricsRecorder
      recordAttachmentButtonUsed:FuseboxAttachmentButtonType::kFiles];

  if (![_mediator canAddMoreAttachments]) {
    [self showMaxAttachmentSnackbarError];
    return;
  }
  [_pickerPresenter presentFilePicker];
}

- (void)composeboxMenuMediatorDidRequestTabSelection:
    (ComposeboxMenuMediator*)mediator {
  [_metricsRecorder
      recordAttachmentButtonUsed:FuseboxAttachmentButtonType::kTabPicker];

  if (![_mediator canAddMoreAttachments]) {
    [self showMaxAttachmentSnackbarError];
    return;
  }
  [_pickerPresenter presentTabPicker];
}

#pragma mark - ComposeboxPickerPresenterDelegate

- (void)composeboxPickerPresenter:(ComposeboxPickerPresenter*)presenter
                    didPickImages:
                        (NSArray<ComposeboxPickerImageResult*>*)results {
  [_metricsRecorder recordImagesAttached:results.count];

  [_mediator processImageItems:results];
}

- (void)composeboxPickerPresenterDidDissmissCamera:
    (ComposeboxPickerPresenter*)presenter {
  // NO-OP.
}

- (void)composeboxPickerPresenter:(ComposeboxPickerPresenter*)presenter
             didPickFilesWithURLs:(NSArray<NSURL*>*)urls {
  [_metricsRecorder recordFilesAttached:urls.count];

  [_mediator processFileURLs:urls];
}

- (void)composeboxPickerPresenter:(ComposeboxPickerPresenter*)presenter
    handleSelectedTabsWithWebStateIDs:
        (std::set<web::WebStateID>)selectedWebStateIDs
                    cachedWebStateIDs:
                        (std::set<web::WebStateID>)cachedWebStateIDs {
  [_mediator processWebStateIDs:selectedWebStateIDs
              cachedWebStateIDs:cachedWebStateIDs];
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
  CHECK(_inputState);
  return _inputState.maxTabAttachmentCount;
}

#pragma mark - Private

/// Displays a snackbar error indicating the maximum number of attachments has
/// been reached.
- (void)showMaxAttachmentSnackbarError {
  // TODO(crbug.com/506956765): Implement.
}

// Records the menu open with visible buttons.
- (void)recordAttachmentsMenuOpen {
  using enum ComposeboxAttachmentOption;

  std::vector<FuseboxAttachmentButtonType> visibleButtons;
  if (![_inputState isAttachmentHidden:kCurrentTab]) {
    visibleButtons.push_back(FuseboxAttachmentButtonType::kCurrentTab);
  }
  if (![_inputState isAttachmentHidden:kTab]) {
    visibleButtons.push_back(FuseboxAttachmentButtonType::kTabPicker);
  }
  if (![_inputState isAttachmentHidden:kCamera]) {
    visibleButtons.push_back(FuseboxAttachmentButtonType::kCamera);
  }
  if (![_inputState isAttachmentHidden:kGallery]) {
    visibleButtons.push_back(FuseboxAttachmentButtonType::kGallery);
  }
  if (![_inputState isAttachmentHidden:kFile]) {
    visibleButtons.push_back(FuseboxAttachmentButtonType::kFiles);
  }

  [_metricsRecorder
      recordAttachmentsMenuOpenedWithVisibleButtons:visibleButtons];
}

- (void)showComposeboxWithParams:(ComposeboxFocusParams*)params {
  id<BrowserCoordinatorCommands> commands = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), BrowserCoordinatorCommands);
  params.metricsRecorder = _metricsRecorder;
  [commands showComposeboxWithParams:params];
}

@end
