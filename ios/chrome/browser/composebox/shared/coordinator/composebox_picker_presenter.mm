// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/shared/coordinator/composebox_picker_presenter.h"

#import <AVFoundation/AVFoundation.h>
#import <PhotosUI/PhotosUI.h>

#import "base/check.h"
#import "base/check_op.h"
#import "base/feature_list.h"
#import "base/memory/weak_ptr.h"
#import "components/contextual_search/input_state_model.h"
#import "components/contextual_search/pref_names.h"
#import "components/lens/lens_features.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/composebox/public/composebox_input_item_source.h"
#import "ios/chrome/browser/composebox/shared/coordinator/composebox_picker_image_result.h"
#import "ios/chrome/browser/composebox/shared/metrics/composebox_metrics_recorder.h"
#import "ios/chrome/browser/composebox/shared/ui/composebox_snackbar_presenter.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/drive_file_picker_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_picker_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/public/provider/chrome/browser/privacy_primitive/privacy_primitive_api.h"
#import "ios/public/provider/chrome/browser/privacy_primitive/privacy_primitive_configuration.h"

namespace {
// The ConsentKit product ID for Chrome on iOS.
constexpr int kChromeIOSProductId = 71720513;
}  // namespace

@interface ComposeboxPickerPresenter () <PHPickerViewControllerDelegate,
                                         UIDocumentPickerDelegate,
                                         UIImagePickerControllerDelegate,
                                         UINavigationControllerDelegate>

// The service managing the active privacy primitive (ConsentKit) flow.
@property(nonatomic, strong) id<PrivacyPrimitiveService>
    privacyPrimitiveService;

@end

@implementation ComposeboxPickerPresenter {
  // The VC used as a base for presentations.
  __weak UIViewController* _baseViewController;
  base::WeakPtr<Browser> _browser;

  // Presents snackbars.
  ComposeboxSnackbarPresenter* _snackbarPresenter;
}

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser {
  self = [super init];
  if (self) {
    _baseViewController = baseViewController;
    _browser = browser->AsWeakPtr();
  }

  return self;
}

- (void)presentCameraPicker {
  if (![UIImagePickerController
          isSourceTypeAvailable:UIImagePickerControllerSourceTypeCamera]) {
    [self.metricsRecorder
        recordPickerOutcome:MobileFuseboxPickerOutcome::kLocalError
          forAttachmentType:MobileFuseboxPickerAttachmentType::kCamera];
    // TODO(crbug.com/40280872): Show an error to the user.
    return;
  }

  AVAuthorizationStatus status =
      [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];
  if (status == AVAuthorizationStatusDenied ||
      status == AVAuthorizationStatusRestricted) {
    [self.metricsRecorder
        recordPickerOutcome:MobileFuseboxPickerOutcome::kPermissionDenied
          forAttachmentType:MobileFuseboxPickerAttachmentType::kCamera];
    return;
  }

  UIImagePickerController* picker = [[UIImagePickerController alloc] init];
  picker.delegate = self;
  picker.sourceType = UIImagePickerControllerSourceTypeCamera;
  [_baseViewController presentViewController:picker
                                    animated:YES
                                  completion:nil];
}

- (void)presentGalleryPickerWithLimit:(NSUInteger)limit {
  PHPickerConfiguration* config = [[PHPickerConfiguration alloc]
      initWithPhotoLibrary:PHPhotoLibrary.sharedPhotoLibrary];

  NSArray<NSString*>* preselectedAssetIDs =
      [self.dataSource attachedImageAssetIDsForPresenter:self];

  if (preselectedAssetIDs.count > 0) {
    config.preselectedAssetIdentifiers = preselectedAssetIDs;
    config.selectionLimit = limit + preselectedAssetIDs.count;
  } else {
    config.selectionLimit = limit;
  }

  config.filter = [PHPickerFilter imagesFilter];
  PHPickerViewController* picker =
      [[PHPickerViewController alloc] initWithConfiguration:config];
  picker.delegate = self;

  [_baseViewController presentViewController:picker
                                    animated:YES
                                  completion:nil];
}

- (void)presentFilePicker {
  UIDocumentPickerViewController* picker;
  if (lens::features::IsLensSendRawFileMediaTypesEnabled()) {
    picker = [[UIDocumentPickerViewController alloc]
        initForOpeningContentTypes:@[ UTTypeData ]];
  } else {
    picker = [[UIDocumentPickerViewController alloc]
        initForOpeningContentTypes:@[ UTTypePDF ]];
  }

  picker.allowsMultipleSelection = NO;
  picker.delegate = self;

  [_baseViewController presentViewController:picker
                                    animated:YES
                                  completion:nil];
}

- (void)presentTabPicker {
  if (!_browser) {
    return;
  }

  [self createSnackbarPresenterIfNeeded];

  TabPickerParams* params =
      [[TabPickerParams alloc] initWithSnackbarPresenter:_snackbarPresenter];
  params.maxTabAttachmentCount =
      [self.dataSource maxTabAttachmentCountForPresenter:self];
  params.preselectedWebStateIDs =
      [self.dataSource attachedWebStateIDsInCurrentContextForPresenter:self];
  params.baseViewController = _baseViewController;

  __weak __typeof(self) weakSelf = self;
  TabPickerCompletionBlock completionBlock =
      ^(std::set<web::WebStateID> selectedIDs,
        std::set<web::WebStateID> cachedIDs) {
        [weakSelf.delegate composeboxPickerPresenter:weakSelf
                   handleSelectedTabsWithWebStateIDs:selectedIDs
                                   cachedWebStateIDs:cachedIDs];
      };

  id<TabPickerCommands> tabPickerHandler =
      HandlerForProtocol(_browser->GetCommandDispatcher(), TabPickerCommands);
  [tabPickerHandler showTabPickerWithParams:params completion:completionBlock];
}

- (void)presentDriveFilePicker {
  if (!_browser) {
    [self.metricsRecorder
        recordPickerOutcome:MobileFuseboxPickerOutcome::kLocalError
          forAttachmentType:MobileFuseboxPickerAttachmentType::kDrive];
    return;
  }

  CHECK_EQ(_browser->type(), Browser::Type::kRegular);

  id<SystemIdentity> identity = [self driveFilePickerIdentity];
  CHECK(identity);

  PrefService* prefService = _browser->GetProfile()->GetPrefs();
  auto consentState = static_cast<contextual_search::DriveConsentState>(
      prefService->GetInteger(contextual_search::kDriveConsentState));

  // TODO(crbug.com/551907302): Scope Drive consent state per GAIA ID or clear
  // it on account switch so consent is not shared across accounts in the same
  // Profile.
  if (base::FeatureList::IsEnabled(
          omnibox::kComposeboxDriveContextMenuOptionDisclaimer) &&
      !base::FeatureList::IsEnabled(omnibox::kForceDriveDisclaimerAccepted) &&
      consentState != contextual_search::DriveConsentState::kConsent) {
    PrivacyPrimitiveConfiguration* config =
        [[PrivacyPrimitiveConfiguration alloc] init];
    config.identity = identity;
    config.flowID = omnibox::kComposeboxDriveConsentFlowId.Get();
    config.productID = kChromeIOSProductId;
    config.productSurface =
        omnibox::kComposeboxDriveConsentProductSurface.Get();

    self.privacyPrimitiveService =
        ios::provider::CreatePrivacyPrimitiveService(config);
    if (!self.privacyPrimitiveService) {
      [self showDriveFilePickerInternal];
      return;
    }

    __weak __typeof(self) weakSelf = self;
    [self.privacyPrimitiveService
        showFlowWithPresentingViewController:_baseViewController
                           completionHandler:^(BOOL success) {
                             [weakSelf privacyPrimitiveFlowCompletedWithSuccess:
                                           success];
                           }];
    return;
  }

  [self showDriveFilePickerInternal];
}

- (void)privacyPrimitiveFlowCompletedWithSuccess:(BOOL)success {
  self.privacyPrimitiveService = nil;
  if (!success || ![self canShowDriveFilePicker]) {
    [self.metricsRecorder
        recordPickerOutcome:MobileFuseboxPickerOutcome::kPermissionDenied
          forAttachmentType:MobileFuseboxPickerAttachmentType::kDrive];
    return;
  }
  PrefService* prefs = _browser->GetProfile()->GetPrefs();
  prefs->SetInteger(
      contextual_search::kDriveConsentState,
      static_cast<int>(contextual_search::DriveConsentState::kConsent));
  [self showDriveFilePickerInternal];
}

- (void)showDriveFilePickerInternal {
  if (!_browser || ![self canShowDriveFilePicker]) {
    return;
  }
  [self createSnackbarPresenterIfNeeded];
  NSUInteger maxDriveAttachmentCount =
      [self.dataSource maxDriveAttachmentCountForPresenter:self];
  id<DriveFilePickerCommands> driveFilePickerCommands = HandlerForProtocol(
      _browser->GetCommandDispatcher(), DriveFilePickerCommands);
  [driveFilePickerCommands
      showDriveFilePickerWithComposeboxDelegate:self.delegate
                             baseViewController:_baseViewController
                             maxAttachmentCount:maxDriveAttachmentCount
                              snackbarPresenter:_snackbarPresenter];
}

#pragma mark - UIImagePickerControllerDelegate

- (void)imagePickerController:(UIImagePickerController*)picker
    didFinishPickingMediaWithInfo:(NSDictionary<NSString*, id>*)info {
  __weak __typeof(self) weakSelf = self;
  [picker dismissViewControllerAnimated:YES
                             completion:^{
                               [weakSelf.delegate
                                   composeboxPickerPresenterDidDissmissCamera:
                                       weakSelf];
                             }];

  UIImage* image = info[UIImagePickerControllerOriginalImage];
  if (!image) {
    [self.metricsRecorder
        recordPickerOutcome:MobileFuseboxPickerOutcome::kLocalError
          forAttachmentType:MobileFuseboxPickerAttachmentType::kCamera];
    return;
  }

  [self.metricsRecorder
      recordPickerOutcome:MobileFuseboxPickerOutcome::kAttachmentAdded
        forAttachmentType:MobileFuseboxPickerAttachmentType::kCamera];

  NSItemProvider* provider = [[NSItemProvider alloc] initWithObject:image];
  [self.delegate
      composeboxPickerPresenter:self
                  didPickImages:@[
                    [[ComposeboxPickerImageResult alloc]
                        initWithImageProvider:provider
                                      assetID:nil
                                       source:ComposeboxInputItemSource::
                                                  kCameraPicker]
                  ]];
}

- (void)imagePickerControllerDidCancel:(UIImagePickerController*)picker {
  [self.metricsRecorder
      recordPickerOutcome:MobileFuseboxPickerOutcome::kManualUserExit
        forAttachmentType:MobileFuseboxPickerAttachmentType::kCamera];

  __weak __typeof(self) weakSelf = self;
  [picker dismissViewControllerAnimated:YES
                             completion:^{
                               [weakSelf.delegate
                                   composeboxPickerPresenterDidDissmissCamera:
                                       weakSelf];
                             }];
}

#pragma mark - PHPickerViewControllerDelegate

- (void)picker:(PHPickerViewController*)picker
    didFinishPicking:(NSArray<PHPickerResult*>*)results {
  [picker dismissViewControllerAnimated:YES completion:nil];

  if (results.count == 0) {
    [self.metricsRecorder
        recordPickerOutcome:MobileFuseboxPickerOutcome::kManualUserExit
          forAttachmentType:MobileFuseboxPickerAttachmentType::kGallery];
    return;
  }

  [self.metricsRecorder
      recordPickerOutcome:MobileFuseboxPickerOutcome::kAttachmentAdded
        forAttachmentType:MobileFuseboxPickerAttachmentType::kGallery];

  NSMutableArray<ComposeboxPickerImageResult*>* imageItems =
      [[NSMutableArray alloc] initWithCapacity:results.count];
  for (PHPickerResult* result in results) {
    [imageItems addObject:[[ComposeboxPickerImageResult alloc]
                              initWithImageProvider:result.itemProvider
                                            assetID:result.assetIdentifier
                                             source:ComposeboxInputItemSource::
                                                        kGalleryPicker]];
  }

  [self.delegate composeboxPickerPresenter:self didPickImages:imageItems];
}

#pragma mark - UIDocumentPickerDelegate

- (void)documentPicker:(UIDocumentPickerViewController*)controller
    didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls {
  if (urls.count == 0) {
    [self.metricsRecorder
        recordPickerOutcome:MobileFuseboxPickerOutcome::kManualUserExit
          forAttachmentType:MobileFuseboxPickerAttachmentType::kFile];
    return;
  }

  [self.metricsRecorder
      recordPickerOutcome:MobileFuseboxPickerOutcome::kAttachmentAdded
        forAttachmentType:MobileFuseboxPickerAttachmentType::kFile];

  [self.delegate composeboxPickerPresenter:self didPickFilesWithURLs:urls];
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController*)controller {
  [self.metricsRecorder
      recordPickerOutcome:MobileFuseboxPickerOutcome::kManualUserExit
        forAttachmentType:MobileFuseboxPickerAttachmentType::kFile];
}

#pragma mark - Private

/// Returns the primary identity if the browser is regular and the user is
/// signed in; otherwise returns nil.
- (id<SystemIdentity>)driveFilePickerIdentity {
  if (!_browser || _browser->type() != Browser::Type::kRegular) {
    [self.metricsRecorder
        recordPickerOutcome:MobileFuseboxPickerOutcome::kLocalError
          forAttachmentType:MobileFuseboxPickerAttachmentType::kDrive];
    return nil;
  }
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(_browser->GetProfile());
  if (!authService || !authService->HasPrimaryIdentity()) {
    [self.metricsRecorder
        recordPickerOutcome:MobileFuseboxPickerOutcome::kLocalError
          forAttachmentType:MobileFuseboxPickerAttachmentType::kDrive];
    return nil;
  }

  id<SystemIdentity> identity = authService->GetPrimaryIdentity();
  if (identity == nil) {
    [self.metricsRecorder
        recordPickerOutcome:MobileFuseboxPickerOutcome::kLocalError
          forAttachmentType:MobileFuseboxPickerAttachmentType::kDrive];
  }

  return identity;
}

/// Returns whether the Drive file picker can be presented.
- (BOOL)canShowDriveFilePicker {
  return [self driveFilePickerIdentity] != nil;
}

- (void)createSnackbarPresenterIfNeeded {
  if (_snackbarPresenter || !_browser) {
    return;
  }
  _snackbarPresenter =
      [[ComposeboxSnackbarPresenter alloc] initWithBrowser:_browser.get()];
}

@end
