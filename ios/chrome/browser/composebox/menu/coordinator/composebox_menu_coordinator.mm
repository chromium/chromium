// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/menu/coordinator/composebox_menu_coordinator.h"

#import <algorithm>
#import <iterator>
#import <memory>
#import <set>
#import <vector>

#import "base/unguessable_token.h"
#import "components/contextual_search/contextual_search_service.h"
#import "components/contextual_search/contextual_search_session_handle.h"
#import "components/contextual_tasks/public/features.h"
#import "components/omnibox/browser/aim_eligibility_service.h"
#import "ios/chrome/browser/aim/model/ios_chrome_aim_eligibility_service_factory.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_input_state_manager.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_mode_holder.h"
#import "ios/chrome/browser/composebox/menu/coordinator/composebox_menu_mediator.h"
#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_shared_tabs_view_controller.h"
#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_view_controller.h"
#import "ios/chrome/browser/composebox/model/ios_contextual_search_service_factory.h"
#import "ios/chrome/browser/composebox/public/composebox_attachment_selection.h"
#import "ios/chrome/browser/composebox/public/composebox_focus_params.h"
#import "ios/chrome/browser/composebox/shared/coordinator/composebox_attachment_diff.h"
#import "ios/chrome/browser/composebox/shared/coordinator/composebox_picker_drive_result.h"
#import "ios/chrome/browser/composebox/shared/coordinator/composebox_picker_presenter.h"
#import "ios/chrome/browser/composebox/shared/metrics/composebox_metrics_recorder.h"
#import "ios/chrome/browser/composebox/ui/composebox_ui_input_state.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_utils.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state_id.h"
#import "third_party/omnibox_proto/searchbox_config.pb.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Custom detent identifier to fit the collection view by
// the `preferredContentSize`.
NSString* const kCustomFittingDetentIdentifier = @"kFittingDetentIdentifier";

/// Top padding above the sheet. Allows the user to drag the sheet down without
/// conflicting with system behavior.
CGFloat const kSheetTopPadding = 40.0f;

}  // namespace

@interface ComposeboxMenuCoordinator () <
    ComposeboxInputStateManagerDelegate,
    ComposeboxMenuMediatorDelegate,
    ComposeboxMenuSharedTabsViewControllerDelegate,
    ComposeboxMenuViewControllerDelegate,
    ComposeboxPickerPresenterDataSource,
    ComposeboxPickerPresenterDelegate,
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
  // Tracks if the user performed a successful action in the menu.
  BOOL _successfulActionPerformed;
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
  return [self initWithBaseViewController:viewController
                                  browser:browser
                   preselectedAttachments:nil
                               inputState:nil
                          metricsRecorder:nil
                               entrypoint:entrypoint];
}

- (void)start {
  _viewController = [[ComposeboxMenuViewController alloc] init];
  _viewController.delegate = self;

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

    _metricsRecorder =
        [[ComposeboxMetricsRecorder alloc] initWithEntrypoint:_entrypoint];
    if (_sessionHandle) {
      _metricsRecorder.contextualSearchMetricsRecorder =
          _sessionHandle->GetMetricsRecorder();
    }

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
                  isIncognito:profile->IsOffTheRecord()
             urlLoaderFactory:profile->GetSharedURLLoaderFactory()];
    _stateManager.delegate = self;
    _stateManager.metricsRecorder = _metricsRecorder;

    std::set<web::WebStateID> emptySet;
    _inputState = [_stateManager computeUIInputStateWithFavicon:nil
                                            attachedWebStateIDs:emptySet];
  }

  CHECK(_inputState);
  _mediator = [[ComposeboxMenuMediator alloc]
          initWithEntrypoint:_entrypoint
                  inputState:_inputState
                webStateList:self.browser->GetWebStateList()
      preselectedAttachments:_preselection
             metricsRecorder:_metricsRecorder];
  _mediator.delegate = self;

  _viewController.sheetPresentationController.prefersGrabberVisible = YES;
  _viewController.sheetPresentationController.delegate = self;
  _viewController.sheetPresentationController
      .prefersEdgeAttachedInCompactHeight = YES;

  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE) {
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
  if (!_successfulActionPerformed) {
    [_metricsRecorder recordAttachmentsMenuShown:NO];
  }
  if (_isStandaloneMenu) {
    // Disconnect the metrics recorder when its constructed by the menu.
    _metricsRecorder.contextualSearchMetricsRecorder = nullptr;
  }
  _metricsRecorder = nil;
  if (!_viewController.isBeingDismissed) {
    [_viewController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:nil];
  }
  _viewController = nil;
  [_mediator disconnect];
  _mediator = nil;
  _pickerPresenter = nil;
  [_stateManager disconnect];
  _stateManager = nil;
  _sessionHandle.reset();
}

#pragma mark - UISheetPresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self requestMenuDismissal];
}

#pragma mark - ComposeboxMenuMediatorDelegate

- (void)composeboxMenuMediator:(ComposeboxMenuMediator*)mediator
                    didTapTool:(ComposeboxMode)toolMode {
  _successfulActionPerformed = YES;

  if (_isStandaloneMenu) {
    [_metricsRecorder recordToolSelected:toolMode];
    if (toolMode == ComposeboxMode::kAIM) {
      [_metricsRecorder
          recordAiModeActivationSource:AiModeActivationSource::kToolMenu];
    }

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
  _successfulActionPerformed = YES;

  if (_isStandaloneMenu) {
    [_metricsRecorder recordModelSelected:modelMode];
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
  _successfulActionPerformed = YES;
  __weak __typeof(self) weakSelf = self;
  if (_isStandaloneMenu) {
    ComposeboxFocusParams* focusParams = [[ComposeboxFocusParams alloc]
        initWithEntrypoint:_entrypoint
                     query:nil
                  toolMode:ComposeboxMode::kRegularSearch
                 modelMode:ComposeboxModelOption::kNone
            attachmentList:attachments];
    [_viewController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:^{
                             [weakSelf showComposeboxWithParams:focusParams];
                           }];
  } else {
    [self.inputPlateDelegate composeboxMenuCoordinator:self
                                  didUpdateAttachments:attachments];
    [_viewController dismissViewControllerAnimated:YES
                                        completion:^{
                                          [weakSelf requestMenuDismissal];
                                        }];
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

- (void)composeboxMenuMediatorDidRequestDriveFileSelection:
    (ComposeboxMenuMediator*)mediator {
  [_pickerPresenter presentDriveFilePicker];
}

- (void)composeboxMenuMediatorDidRequestSharedTabs:
    (ComposeboxMenuMediator*)mediator {
  ComposeboxMenuSharedTabsViewController* viewController =
      [[ComposeboxMenuSharedTabsViewController alloc]
          initWithSharedTabs:_inputState.sharedTabs];
  viewController.delegate = self;

  viewController.sheetPresentationController
      .prefersEdgeAttachedInCompactHeight = YES;
  viewController.sheetPresentationController.detents =
      @[ [UISheetPresentationControllerDetent largeDetent] ];

  [_viewController presentViewController:viewController
                                animated:YES
                              completion:nil];
}

#pragma mark - ComposeboxMenuSharedTabsViewControllerDelegate

- (void)composeboxMenuSharedTabsViewController:
            (ComposeboxMenuSharedTabsViewController*)viewController
                                     didTapURL:(const GURL&)url {
  UrlLoadParams params = UrlLoadParams::InNewTab(url);
  UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);

  [_viewController.presentingViewController dismissViewControllerAnimated:YES
                                                               completion:nil];
}

- (void)composeboxMenuSharedTabsViewController:
            (ComposeboxMenuSharedTabsViewController*)viewController
                   didRemoveTabWithServerToken:
                       (const base::UnguessableToken&)serverToken {
  [self.inputPlateDelegate composeboxMenuCoordinator:self
                         didRemoveTabWithServerToken:serverToken];

  ComposeboxUIInputState* newState =
      [self.inputPlateDelegate currentUIInputStateForMenuCoordinator:self];
  if (newState) {
    _inputState = newState;
    [_mediator updateUIInputState:_inputState];
  }

  id<SnackbarCommands> snackbarHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SnackbarCommands);
  NSString* title =
      l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_MENU_TAB_NO_LONGER_SHARED);
  SnackbarMessage* message = [[SnackbarMessage alloc] initWithTitle:title];
  [snackbarHandler showSnackbarMessage:message bottomOffset:0];
}

#pragma mark - ComposeboxPickerPresenterDelegate

- (void)composeboxPickerPresenter:(ComposeboxPickerPresenter*)presenter
                    didPickImages:
                        (NSArray<ComposeboxPickerImageResult*>*)results {
  if (results.count > 0) {
    [_metricsRecorder recordImagesAttached:results.count];
  }

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
  std::set<web::WebStateID> alreadyProcessedIDs =
      [_mediator allAttachedWebStateIDs];
  composebox::TabDiff diff =
      composebox::ComputeTabDiff(alreadyProcessedIDs, selectedWebStateIDs);

  if (diff.added.size() > 0) {
    [_metricsRecorder recordTabPickerTabsAttached:diff.added.size()];
  }

  [_mediator processWebStateIDs:selectedWebStateIDs
              cachedWebStateIDs:cachedWebStateIDs];
}

- (void)composeboxPickerPresenter:(ComposeboxPickerPresenter*)presenter
                didPickDriveItems:
                    (NSArray<ComposeboxPickerDriveResult*>*)results {
  if (results.count == 0) {
    return;
  }
  [_metricsRecorder recordDriveFilesAttached:results.count];
  [_mediator processDriveItems:results];
}

#pragma mark - ComposeboxPickerPresenterDataSource

- (std::set<web::WebStateID>)attachedWebStateIDsInCurrentContextForPresenter:
    (ComposeboxPickerPresenter*)presenter {
  return [_mediator attachedWebStateIDsInCurrentContext];
}

- (NSUInteger)maxTabAttachmentCountForPresenter:
    (ComposeboxPickerPresenter*)presenter {
  CHECK(_inputState);
  return _inputState.maxTabAttachmentCount;
}

- (NSArray<NSString*>*)attachedImageAssetIDsForPresenter:
    (ComposeboxPickerPresenter*)presenter {
  return [_mediator attachedImageAssetIDs];
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

  for (const auto& tool : _inputState.allowedTools) {
    [_metricsRecorder recordToolModeShown:tool];
  }

  for (const auto& model : _inputState.allowedModels) {
    [_metricsRecorder recordModelModeShown:model];
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

// Requests the dismissal of the menu UI.
- (void)requestMenuDismissal {
  if (_isStandaloneMenu) {
    id<BrowserCoordinatorCommands> commands = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), BrowserCoordinatorCommands);
    [commands dismissMultimodalActionsMenu];
  } else {
    [self.delegate composeboxMenuCoordinatorDidDismissMenu:self];
  }
}

#pragma mark - ComposeboxMenuViewControllerDelegate

- (void)composeboxMenuViewControllerDidRequestClose:
    (ComposeboxMenuViewController*)composeboxMenuViewController {
  [self requestMenuDismissal];
}

#pragma mark - ComposeboxInputStateManagerDelegate

- (void)inputStateManager:(ComposeboxInputStateManager*)manager
             didChangeMode:(ComposeboxMode)mode
    invalidatedAttachments:(NSArray<ComposeboxInputItem*>*)invalidatedItems {
}

- (void)inputStateManagerDidUpdateUIState:
    (ComposeboxInputStateManager*)manager {
  _inputState = [_stateManager computeUIInputStateWithFavicon:nil
                                          attachedWebStateIDs:{}];
  [_mediator updateUIInputState:_inputState];
}

@end
