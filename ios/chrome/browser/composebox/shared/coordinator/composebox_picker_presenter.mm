// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/shared/coordinator/composebox_picker_presenter.h"

#import <PhotosUI/PhotosUI.h>

#import "base/feature_list.h"
#import "base/memory/weak_ptr.h"
#import "components/contextual_search/input_state_model.h"
#import "components/contextual_search/pref_names.h"
#import "components/lens/lens_features.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/composebox/public/composebox_input_item_source.h"
#import "ios/chrome/browser/composebox/shared/coordinator/composebox_picker_image_result.h"
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
    // TODO(crbug.com/40280872): Show an error to the user.
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
    return;
  }

  ProfileIOS* profile = _browser->GetProfile();
  PrefService* prefService = profile->GetPrefs();
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(profile);
  id<SystemIdentity> identity = authService->GetPrimaryIdentity();

  auto consentState = static_cast<contextual_search::DriveConsentState>(
      prefService->GetInteger(contextual_search::kDriveConsentState));

  // TODO(crbug.com/551907302): Scope Drive consent state per GAIA ID or clear
  // it on account switch so consent is not shared across accounts in the same
  // Profile.
  if (base::FeatureList::IsEnabled(
          omnibox::kComposeboxDriveContextMenuOptionDisclaimer) &&
      !base::FeatureList::IsEnabled(omnibox::kForceDriveDisclaimerAccepted) &&
      consentState != contextual_search::DriveConsentState::kConsent &&
      identity) {
    PrivacyPrimitiveConfiguration* config =
        [[PrivacyPrimitiveConfiguration alloc] init];
    config.identity = identity;
    config.flowID = omnibox::kComposeboxDriveConsentFlowId.Get();
    config.productID = omnibox::kComposeboxDriveConsentProductId.Get();

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
  if (!success || !_browser) {
    return;
  }
  PrefService* prefs = _browser->GetProfile()->GetPrefs();
  prefs->SetInteger(
      contextual_search::kDriveConsentState,
      static_cast<int>(contextual_search::DriveConsentState::kConsent));
  [self showDriveFilePickerInternal];
}

- (void)showDriveFilePickerInternal {
  if (!_browser) {
    return;
  }
  id<DriveFilePickerCommands> driveFilePickerCommands = HandlerForProtocol(
      _browser->GetCommandDispatcher(), DriveFilePickerCommands);
  [driveFilePickerCommands
      showDriveFilePickerWithComposeboxDelegate:self.delegate
                             baseViewController:_baseViewController];
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
    return;
  }

  [picker dismissViewControllerAnimated:YES
                             completion:^{
                               [weakSelf.delegate
                                   composeboxPickerPresenterDidDissmissCamera:
                                       weakSelf];
                             }];

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

  // TODO(crbug.com/506955766): Unify metrics recording and record this action.

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
  [self.delegate composeboxPickerPresenter:self didPickFilesWithURLs:urls];
}

#pragma mark - Private

- (void)createSnackbarPresenterIfNeeded {
  if (_snackbarPresenter || !_browser) {
    return;
  }
  _snackbarPresenter =
      [[ComposeboxSnackbarPresenter alloc] initWithBrowser:_browser.get()];
}

@end
