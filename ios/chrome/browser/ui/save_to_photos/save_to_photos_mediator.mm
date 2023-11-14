// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/save_to_photos/save_to_photos_mediator.h"

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/photos/photos_metrics.h"
#import "ios/chrome/browser/photos/photos_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/system_identity.h"
#import "ios/chrome/browser/ui/account_picker/account_picker_configuration.h"
#import "ios/chrome/browser/ui/save_to_photos/save_to_photos_mediator_delegate.h"
#import "ios/chrome/browser/web/image_fetch/image_fetch_tab_helper.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Maximum length of the suggested image name passed to the Photos service.
constexpr size_t kSuggestedImageNameMaxLength = 100;

NSURL* GetGooglePhotosAppURL() {
  NSURLComponents* photosAppURLComponents = [[NSURLComponents alloc] init];
  photosAppURLComponents.scheme = kGooglePhotosAppURLScheme;
  return photosAppURLComponents.URL;
}

// Returns formatted size string.
NSString* GetSizeString(NSUInteger sizeInBytes) {
  return [NSByteCountFormatter
      stringFromByteCount:sizeInBytes
               countStyle:NSByteCountFormatterCountStyleFile];
}

// Helper to call -[mediator startWithImageURL:referrer:webState:]
void StartMediatorHelper(__weak SaveToPhotosMediator* mediator,
                         const GURL& image_url,
                         const web::Referrer referrer,
                         base::WeakPtr<web::WebState> web_state) {
  [mediator startWithImageURL:image_url
                     referrer:referrer
                     webState:web_state.get()];
}

}  // namespace

NSString* const kGooglePhotosAppProductIdentifier = @"962194608";

NSString* const kGooglePhotosStoreKitCampaignToken = @"chrome-x-photos";

NSString* const kGooglePhotosRecentlyAddedURLString =
    @"https://photos.google.com/search/_tra_?obfsgid=";

NSString* const kGooglePhotosAppURLScheme = @"googlephotos";

@implementation SaveToPhotosMediator {
  PhotosService* _photosService;
  PrefService* _prefService;
  ChromeAccountManagerService* _accountManagerService;
  signin::IdentityManager* _identityManager;
  NSString* _imageName;
  NSData* _imageData;
  id<SystemIdentity> _identity;
  BOOL _userTappedSuccessSnackbarButton;
  base::TimeTicks _uploadStart;
  BOOL _successSnackbarAppeared;
  BOOL _successSnackbarDisappeared;
  BOOL _uploadCompletedSuccessfully;
}

#pragma mark - Initialization

- (instancetype)
    initWithPhotosService:(PhotosService*)photosService
              prefService:(PrefService*)prefService
    accountManagerService:(ChromeAccountManagerService*)accountManagerService
          identityManager:(signin::IdentityManager*)identityManager {
  self = [super init];
  if (self) {
    _photosService = photosService;
    _prefService = prefService;
    _accountManagerService = accountManagerService;
    _identityManager = identityManager;
    CHECK(_photosService);
    CHECK(_prefService);
    CHECK(_accountManagerService);
    CHECK(_identityManager);
  }
  return self;
}

#pragma mark - Public

- (void)startWithImageURL:(const GURL&)imageURL
                 referrer:(const web::Referrer&)referrer
                 webState:(web::WebState*)webState {
  // If the web state does not exist anymore (which can happen when the user
  // tries again), hide Save to Photos.
  if (!webState) {
    base::UmaHistogramEnumeration(
        kSaveToPhotosActionsHistogram,
        SaveToPhotosActions::kFailureWebStateDestroyed);
    [self.delegate hideSaveToPhotos];
    return;
  }

  // If the image cannot be fetched, the user can "Try Again" from here. It
  // makes sense to let the user try again if the image cannot be fetched
  // because of a connection issue.
  __weak __typeof(self) weakSelf = self;
  ProceduralBlock tryAgainBlock = base::CallbackToBlock(
      base::BindOnce(&StartMediatorHelper, weakSelf, imageURL, referrer,
                     webState->GetWeakPtr()));

  _imageName = base::SysUTF8ToNSString(imageURL.ExtractFileName());

  ImageFetchTabHelper* imageFetcher =
      ImageFetchTabHelper::FromWebState(webState);
  CHECK(imageFetcher);
  imageFetcher->GetImageData(imageURL, referrer, ^(NSData* imageData) {
    if (imageData) {
      [weakSelf continueSaveImageWithData:imageData];
    } else {
      [weakSelf showTryAgainOrCancelAlertWithTryAgainBlock:tryAgainBlock];
    }
  });
}

- (void)accountPickerDidSelectIdentity:(id<SystemIdentity>)identity
                          askEveryTime:(BOOL)askEveryTime {
  CHECK(identity);
  base::UmaHistogramEnumeration(
      kSaveToPhotosAccountPickerActionsHistogram,
      SaveToPhotosAccountPickerActions::kSelectedIdentity);
  [self.delegate hideAccountPicker];

  // Memorize the account that was picked and whether to ask which account to
  // use every time.
  _prefService->SetString(prefs::kIosSaveToPhotosDefaultGaiaId,
                          base::SysNSStringToUTF8(identity.gaiaID));
  _prefService->SetBoolean(prefs::kIosSaveToPhotosSkipAccountPicker,
                           !askEveryTime);

  _identity = identity;
}

- (void)accountPickerDidCancel {
  base::UmaHistogramEnumeration(kSaveToPhotosAccountPickerActionsHistogram,
                                SaveToPhotosAccountPickerActions::kCancelled);
  [self.delegate hideAccountPicker];
}

- (void)accountPickerWasHidden {
  if (!_identity) {
    base::UmaHistogramEnumeration(
        kSaveToPhotosActionsHistogram,
        SaveToPhotosActions::kFailureUserCancelledWithAccountPicker);
    [self.delegate hideSaveToPhotos];
    return;
  }
  [self tryUploadImage];
}

- (void)storeKitWantsToHide {
  BOOL photosAppInstalled =
      [UIApplication.sharedApplication canOpenURL:GetGooglePhotosAppURL()];
  base::UmaHistogramEnumeration(
      kSaveToPhotosActionsHistogram,
      photosAppInstalled
          ? SaveToPhotosActions::kSuccessAndOpenStoreKitAndAppInstalled
          : SaveToPhotosActions::kSuccessAndOpenStoreKitAndAppNotInstalled);
  [self.delegate hideSaveToPhotos];
}

- (void)disconnect {
  self.delegate = nil;
  _photosService = nullptr;
  _prefService = nullptr;
  _accountManagerService = nullptr;
  _identityManager = nullptr;
  _imageName = nil;
  _imageData = nil;
  _identity = nil;
}

#pragma mark - Private

// Resume the process of saving the image once the data has been fetched.
- (void)continueSaveImageWithData:(NSData*)imageData {
  _imageData = imageData;

  // Although it is unlikely, the user could sign-out while the image data is
  // being fetched. Exit now if that happened.
  if (!_identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    base::UmaHistogramEnumeration(kSaveToPhotosActionsHistogram,
                                  SaveToPhotosActions::kFailureUserSignedOut);
    [self.delegate hideSaveToPhotos];
    return;
  }

  const std::string defaultGaiaId =
      _prefService->GetString(prefs::kIosSaveToPhotosDefaultGaiaId);
  id<SystemIdentity> defaultIdentity =
      _accountManagerService->GetIdentityWithGaiaID(defaultGaiaId);
  bool skipAccountPicker =
      _prefService->GetBoolean(prefs::kIosSaveToPhotosSkipAccountPicker);

  // If the user has already selected a default account to save images to
  // Photos and opted to skip the account picker, use that default.
  if (skipAccountPicker && defaultIdentity) {
    _identity = defaultIdentity;
    base::UmaHistogramEnumeration(kSaveToPhotosAccountPickerActionsHistogram,
                                  SaveToPhotosAccountPickerActions::kSkipped);
    [self tryUploadImage];
    return;
  }

  // If the memorized account is not found on the device, unmemorize it.
  if (skipAccountPicker) {
    _prefService->ClearPref(prefs::kIosSaveToPhotosDefaultGaiaId);
    _prefService->ClearPref(prefs::kIosSaveToPhotosSkipAccountPicker);
  }

  // If no default account can be used, present the account picker instead.
  AccountPickerConfiguration* configuration =
      [[AccountPickerConfiguration alloc] init];
  configuration.titleText =
      l10n_util::GetNSString(IDS_IOS_SAVE_TO_PHOTOS_ACCOUNT_PICKER_TITLE);
  NSString* imageSize = GetSizeString(_imageData.length);
  configuration.bodyText =
      l10n_util::GetNSStringF(IDS_IOS_SAVE_TO_PHOTOS_ACCOUNT_PICKER_BODY,
                              base::SysNSStringToUTF16(_imageName),
                              base::SysNSStringToUTF16(imageSize));
  configuration.submitButtonTitle =
      l10n_util::GetNSString(IDS_IOS_SAVE_TO_PHOTOS_ACCOUNT_PICKER_SUBMIT);
  configuration.askEveryTimeSwitchLabelText = l10n_util::GetNSString(
      IDS_IOS_SAVE_TO_PHOTOS_ACCOUNT_PICKER_ASK_EVERY_TIME);
  [self.delegate showAccountPickerWithConfiguration:configuration
                                   selectedIdentity:defaultIdentity];
}

// Once the destination account is known, tries to upload the image using the
// Photos service.
- (void)tryUploadImage {
  __weak __typeof(self) weakSelf = self;

  // Reset part of the state in case this is not the first attempt.
  _userTappedSuccessSnackbarButton = NO;
  _successSnackbarAppeared = NO;
  _successSnackbarDisappeared = NO;
  _uploadCompletedSuccessfully = NO;

  // If the Photos service is unavailable (maybe busy in a separate window),
  // present an alert.
  if (!_photosService->IsAvailable()) {
    [self showTryAgainOrCancelAlertWithTryAgainBlock:^{
      [weakSelf tryUploadImage];
    }];
    return;
  }

  // Else start uploading the image and let the user know with a snackbar.
  auto uploadCompletionCallback =
      base::BindOnce(^(PhotosService::UploadResult result) {
        [weakSelf photosServiceFinishedUploadWithResult:result];
      });
  _uploadStart = base::TimeTicks::Now();
  auto uploadProgressCallback =
      base::BindRepeating(^(const PhotosService::UploadProgress& progress) {
        [weakSelf photosServiceReportedUploadProgress:progress];
      });
  NSString* suggestedImageName =
      _imageName.length > kSuggestedImageNameMaxLength ? nil : _imageName;
  _photosService->UploadImage(suggestedImageName, _imageData, _identity,
                              std::move(uploadProgressCallback),
                              std::move(uploadCompletionCallback));
}

// Called when the Photos service reports upload completion.
- (void)photosServiceFinishedUploadWithResult:
    (const PhotosService::UploadResult&)result {
  if (!result.successful) {
    base::UmaHistogramTimes("IOS.SaveToPhotos.UploadFailureLatency",
                            base::TimeTicks::Now() - _uploadStart);
    __weak __typeof(self) weakSelf = self;
    [self showTryAgainOrCancelAlertWithTryAgainBlock:^{
      [weakSelf tryUploadImage];
    }];
    return;
  }

  base::UmaHistogramTimes("IOS.SaveToPhotos.UploadSuccessLatency",
                          base::TimeTicks::Now() - _uploadStart);
  _uploadCompletedSuccessfully = YES;

  if (!_successSnackbarAppeared) {
    // If the success snackbar did not appear for some reason (no progress has
    // been reported), show it now.
    [self showSnackbarWithSuccessMessageAndOpenButton];
    return;
  }

  if (!_successSnackbarDisappeared) {
    // If the success snackbar has not disappeared, wait until it does to
    // maybe open the photo and then finish.
    return;
  }

  if (_userTappedSuccessSnackbarButton) {
    [self openPhotosAppOrShowInStoreKit];
    return;
  }

  base::UmaHistogramEnumeration(kSaveToPhotosActionsHistogram,
                                SaveToPhotosActions::kSuccess);
  [self.delegate hideSaveToPhotos];
}

- (void)photosServiceReportedUploadProgress:
    (const PhotosService::UploadProgress&)progress {
  if (progress.total_bytes_sent == progress.total_bytes_expected_to_send) {
    [self showSnackbarWithSuccessMessageAndOpenButton];
  }
}

// Shows an alert with a "Try Again" button (calls `tryAgain`) and a "Cancel"
// button.
- (void)showTryAgainOrCancelAlertWithTryAgainBlock:(ProceduralBlock)tryAgain {
  NSString* title = l10n_util::GetNSString(
      IDS_IOS_SAVE_TO_PHOTOS_THIS_FILE_COULD_NOT_BE_UPLOADED_TITLE);
  NSString* imageSize = GetSizeString(_imageData.length);
  NSString* message = l10n_util::GetNSStringF(
      IDS_IOS_SAVE_TO_PHOTOS_THIS_FILE_COULD_NOT_BE_UPLOADED_MESSAGE,
      base::SysNSStringToUTF16(_imageName),
      base::SysNSStringToUTF16(imageSize));
  NSString* cancelTitle = l10n_util::GetNSString(IDS_CANCEL);
  NSString* tryAgainTitle = l10n_util::GetNSString(
      IDS_IOS_SAVE_TO_PHOTOS_THIS_FILE_COULD_NOT_BE_UPLOADED_TRY_AGAIN);
  __weak __typeof(self.delegate) weakDelegate = self.delegate;
  [self.delegate
      showTryAgainOrCancelAlertWithTitle:title
                                 message:message
                           tryAgainTitle:tryAgainTitle
                          tryAgainAction:tryAgain
                             cancelTitle:cancelTitle
                            cancelAction:^{
                              base::UmaHistogramEnumeration(
                                  kSaveToPhotosActionsHistogram,
                                  SaveToPhotosActions::
                                      kFailureUserCancelledWithAlert);
                              [weakDelegate hideSaveToPhotos];
                            }];
}

// Shows a snackbar to let the user know the Photos service is done uploading
// the image with a button to either open the Photos app if it is installed or
// show the Photos app in the AppStore otherwise.
- (void)showSnackbarWithSuccessMessageAndOpenButton {
  _successSnackbarAppeared = YES;
  NSString* message = l10n_util::GetNSStringF(
      IDS_IOS_SAVE_TO_PHOTOS_SNACKBAR_IMAGE_SAVED_MESSAGE,
      base::SysNSStringToUTF16(_identity.userEmail));
  NSString* buttonText = l10n_util::GetNSString(
      IDS_IOS_SAVE_TO_PHOTOS_SNACKBAR_IMAGE_SAVED_OPEN_BUTTON);
  __weak __typeof(self) weakSelf = self;
  [self.delegate showSnackbarWithMessage:message
      buttonText:buttonText
      messageAction:^{
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf) {
          return;
        }
        strongSelf->_userTappedSuccessSnackbarButton = YES;
      }
      completionAction:^(BOOL userTriggered) {
        [weakSelf successSnackbarDisappearedUserTriggered:userTriggered];
      }];
}

// Called when the snackbar shown upon completion disappears. `userTriggered` is
// YES if the snackbar has been dismissed by the user.
- (void)successSnackbarDisappearedUserTriggered:(BOOL)userTriggered {
  _successSnackbarDisappeared = YES;
  if (!_uploadCompletedSuccessfully) {
    // If the upload has not completed, wait until it does to finish.
    return;
  }

  // If the upload completed, maybe open the photo and finish.
  if (_userTappedSuccessSnackbarButton) {
    [self openPhotosAppOrShowInStoreKit];
    return;
  }
  base::UmaHistogramEnumeration(kSaveToPhotosActionsHistogram,
                                SaveToPhotosActions::kSuccess);
  [self.delegate hideSaveToPhotos];
}

// Opens the Photos app if it is installed or show the Photos app in the
// AppStore otherwise.
- (void)openPhotosAppOrShowInStoreKit {
  // If the Photos app is not installed, show StoreKit.
  if (![UIApplication.sharedApplication canOpenURL:GetGooglePhotosAppURL()]) {
    [self.delegate
        showStoreKitWithProductIdentifier:kGooglePhotosAppProductIdentifier
                            campaignToken:kGooglePhotosStoreKitCampaignToken];
    return;
  }

  // Otherwise, open the Photos app and hide Save to Photos.
  NSString* recentlyAddedURLString = [kGooglePhotosRecentlyAddedURLString
      stringByAppendingString:_identity.gaiaID];
  NSURL* photosURL = [NSURL URLWithString:recentlyAddedURLString];
  [UIApplication.sharedApplication
                openURL:photosURL
                options:@{UIApplicationOpenURLOptionUniversalLinksOnly : @YES}
      completionHandler:nil];
  base::UmaHistogramEnumeration(kSaveToPhotosActionsHistogram,
                                SaveToPhotosActions::kSuccessAndOpenPhotosApp);
  [self.delegate hideSaveToPhotos];
}

@end
