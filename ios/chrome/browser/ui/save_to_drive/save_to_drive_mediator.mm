// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/save_to_drive/save_to_drive_mediator.h"

#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_coordinator.h"
#import "ios/chrome/browser/download/model/download_manager_tab_helper.h"
#import "ios/chrome/browser/download/model/download_mimetype_util.h"
#import "ios/chrome/browser/drive/model/drive_file_uploader.h"
#import "ios/chrome/browser/drive/model/drive_metrics.h"
#import "ios/chrome/browser/drive/model/drive_service.h"
#import "ios/chrome/browser/drive/model/drive_tab_helper.h"
#import "ios/chrome/browser/drive/model/manage_storage_url_util.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/commands/account_picker_commands.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/manage_storage_alert_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/save_to_drive_commands.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/ui/save_to_drive/file_destination.h"
#import "ios/chrome/browser/ui/save_to_drive/file_destination_picker_consumer.h"
#import "ios/web/public/download/download_task.h"
#import "ios/web/public/download/download_task_observer_bridge.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "net/base/url_util.h"
// TODO(crbug.com/40286505): Depend on account_picker_consumer.h directly.

@interface SaveToDriveMediator () <CRWWebStateObserver, CRWDownloadTaskObserver>

// Called when the storage quota has been fetched, with or without any error.
- (void)didReceiveStorageQuotaResult:(const DriveStorageQuotaResult&)result;

@end

namespace {

constexpr int64_t kBytesPerMegabyte = 1024 * 1024;

void StorageQuotaCompletionHelper(__weak SaveToDriveMediator* mediator,
                                  const DriveStorageQuotaResult& result) {
  [mediator didReceiveStorageQuotaResult:result];
}

}  // namespace

@implementation SaveToDriveMediator {
  raw_ptr<web::DownloadTask> _downloadTask;
  std::unique_ptr<web::DownloadTaskObserverBridge> _downloadTaskObserverBridge;
  raw_ptr<web::WebState> _webState;
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;
  id<SaveToDriveCommands> _saveToDriveHandler;
  id<ManageStorageAlertCommands> _manageStorageAlertHandler;
  id<ApplicationCommands> _applicationHandler;
  id<AccountPickerCommands> _accountPickerHandler;
  raw_ptr<drive::DriveService> _driveService;
  raw_ptr<PrefService> _prefService;
  raw_ptr<ChromeAccountManagerService> _accountManagerService;
  FileDestination _fileDestination;
  // The file uploader is used to fetch the storage quota for a given identity.
  std::unique_ptr<DriveFileUploader> _fileUploader;
  BOOL _prefsLoaded;
  int _numberOfAttempts;
}

- (instancetype)initWithDownloadTask:(web::DownloadTask*)downloadTask
                  saveToDriveHandler:(id<SaveToDriveCommands>)saveToDriveHandler
           manageStorageAlertHandler:
               (id<ManageStorageAlertCommands>)manageStorageAlertHandler
                  applicationHandler:(id<ApplicationCommands>)applicationHandler
                accountPickerHandler:
                    (id<AccountPickerCommands>)accountPickerHandler
                         prefService:(PrefService*)prefService
               accountManagerService:
                   (ChromeAccountManagerService*)accountManagerService
                        driveService:(drive::DriveService*)driveService {
  self = [super init];
  if (self) {
    _downloadTask = downloadTask;
    _downloadTaskObserverBridge =
        std::make_unique<web::DownloadTaskObserverBridge>(self);
    _downloadTask->AddObserver(_downloadTaskObserverBridge.get());
    _webState = downloadTask->GetWebState();
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _webState->AddObserver(_webStateObserverBridge.get());
    _saveToDriveHandler = saveToDriveHandler;
    _manageStorageAlertHandler = manageStorageAlertHandler;
    _applicationHandler = applicationHandler;
    _accountPickerHandler = accountPickerHandler;
    _prefService = prefService;
    _driveService = driveService;
    _accountManagerService = accountManagerService;
    _fileDestination = FileDestination::kFiles;
  }
  return self;
}

#pragma mark - Public

- (void)disconnect {
  if (_downloadTask) {
    _downloadTask->RemoveObserver(_downloadTaskObserverBridge.get());
  }
  _downloadTask = nullptr;
  _downloadTaskObserverBridge = nullptr;
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
  }
  _webState = nullptr;
  _webStateObserverBridge = nullptr;
  _prefService = nullptr;
  _accountManagerService = nullptr;
  _driveService = nullptr;
  _saveToDriveHandler = nil;
}

- (void)saveWithSelectedIdentity:(id<SystemIdentity>)identity {
  _numberOfAttempts++;
  if (!_downloadTask || !_webState) {
    base::UmaHistogramEnumeration(
        kSaveToDriveUIOutcome,
        !_downloadTask ? SaveToDriveOutcome::kFailureDownloadDestroyed
                       : SaveToDriveOutcome::kFailureWebStateDestroyed);
    [_saveToDriveHandler hideSaveToDrive];
    return;
  }
  switch (_fileDestination) {
    case FileDestination::kFiles: {
      // Clear the account pref.
      _prefService->ClearPref(prefs::kIosSaveToDriveDefaultGaiaId);
      // If the selected file destination is Files, start the download
      // immediately and hide the account picker.
      DownloadManagerTabHelper* downloadManagerTabHelper =
          DownloadManagerTabHelper::FromWebState(_webState);
      downloadManagerTabHelper->StartDownload(_downloadTask);
      base::UmaHistogramEnumeration(kSaveToDriveUIOutcome,
                                    SaveToDriveOutcome::kSuccessSelectedFiles);
      [self recordCommonHistogramsWithSuffix:".SuccessSelectedFiles"];
      [_accountPickerHandler hideAccountPickerAnimated:YES];
      break;
    }
    case FileDestination::kDrive: {
      // Memorize the account that was picked.
      _prefService->SetString(prefs::kIosSaveToDriveDefaultGaiaId,
                              base::SysNSStringToUTF8(identity.gaiaID));
      // Otherwise if the selected destination is Drive, check for sufficient
      // storage space before any further steps.
      [_accountPickerConsumer startValidationSpinner];
      _fileUploader = _driveService->CreateFileUploader(identity);
      __weak __typeof(self) weakSelf = self;
      _fileUploader->FetchStorageQuota(
          base::BindOnce(StorageQuotaCompletionHelper, weakSelf));
      break;
    }
  }
}

- (void)showManageStorageForIdentity:(id<SystemIdentity>)identity {
  // The uploading identity's user email is used to switch to the uploading
  // account before loading the "Manage Storage" web page.
  GURL manageStorageURL = GenerateManageDriveStorageUrl(
      base::SysNSStringToUTF8(identity.userEmail));
  OpenNewTabCommand* newTabCommand =
      [OpenNewTabCommand commandWithURLFromChrome:manageStorageURL];
  base::UmaHistogramEnumeration(
      kSaveToDriveUIOutcome,
      SaveToDriveOutcome::kSuccessSelectedDriveManageStorage);
  [self recordCommonHistogramsWithSuffix:".SuccessSelectedDriveManageStorage"];
  [_applicationHandler openURLInNewTab:newTabCommand];
}

- (void)cancelSaveToDrive {
  const bool selectedFiles = _fileDestination == FileDestination::kFiles;
  const auto outcome = selectedFiles
                           ? SaveToDriveOutcome::kFailureCanceledFiles
                           : SaveToDriveOutcome::kFailureCanceledDrive;
  base::UmaHistogramEnumeration(kSaveToDriveUIOutcome, outcome);
  [self recordCommonHistogramsWithSuffix:selectedFiles
                                             ? ".FailureCanceledFiles"
                                             : ".FailureCanceledDrive"];
  [_accountPickerHandler hideAccountPickerAnimated:YES];
}

#pragma mark - Properties getters/setters

- (void)setAccountPickerConsumer:(id<AccountPickerConsumer>)consumer {
  _accountPickerConsumer = consumer;
  [self updateConsumersAnimated:NO];
}

- (void)setDestinationPickerConsumer:
    (id<FileDestinationPickerConsumer>)consumer {
  _destinationPickerConsumer = consumer;
  [self updateConsumersAnimated:NO];
}

#pragma mark - CRWDownloadTaskObserver

- (void)downloadDestroyed:(web::DownloadTask*)task {
  CHECK_EQ(task, _downloadTask);
  task->RemoveObserver(_downloadTaskObserverBridge.get());
  _downloadTaskObserverBridge = nullptr;
  _downloadTask = nullptr;
  base::UmaHistogramEnumeration(kSaveToDriveUIOutcome,
                                SaveToDriveOutcome::kFailureDownloadDestroyed);
  [_saveToDriveHandler hideSaveToDrive];
}

#pragma mark - CRWWebStateObserver

- (void)webStateWasHidden:(web::WebState*)webState {
  base::UmaHistogramEnumeration(kSaveToDriveUIOutcome,
                                SaveToDriveOutcome::kFailureWebStateHidden);
  [_saveToDriveHandler hideSaveToDrive];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  CHECK_EQ(webState, _webState);
  webState->RemoveObserver(_webStateObserverBridge.get());
  _webStateObserverBridge = nullptr;
  _webState = nullptr;
  base::UmaHistogramEnumeration(kSaveToDriveUIOutcome,
                                SaveToDriveOutcome::kFailureWebStateDestroyed);
  [_saveToDriveHandler hideSaveToDrive];
}

#pragma mark - FileDestinationPickerActionDelegate

- (void)fileDestinationPicker:(UIViewController*)picker
         didSelectDestination:(FileDestination)destination {
  _fileDestination = destination;
  [self updateConsumersAnimated:YES];
}

#pragma mark - Private

// Updates consumers.
- (void)updateConsumersAnimated:(BOOL)animated {
  if (_accountPickerConsumer && _destinationPickerConsumer && !_prefsLoaded) {
    [self loadPrefs];
    _prefsLoaded = YES;
  }

  bool destinationIsFiles = _fileDestination == FileDestination::kFiles;
  [self.accountPickerConsumer setIdentityButtonHidden:destinationIsFiles
                                             animated:animated];
  [self.destinationPickerConsumer setSelectedDestination:_fileDestination];
}

- (void)loadPrefs {
  // Retrieve the last selected identity from prefs.
  const std::string defaultGaiaId =
      _prefService->GetString(prefs::kIosSaveToDriveDefaultGaiaId);
  id<SystemIdentity> defaultIdentity =
      _accountManagerService->GetIdentityWithGaiaID(defaultGaiaId);
  if (defaultIdentity) {
    // If an identity is associated with the memorized GAIA ID, use it.
    [self.accountPickerConsumer setSelectedIdentity:defaultIdentity];
    _fileDestination = FileDestination::kDrive;
  } else {
    // Otherwise, clear any memorized GAIA ID from prefs.
    _prefService->ClearPref(prefs::kIosSaveToDriveDefaultGaiaId);
    _fileDestination = FileDestination::kFiles;
  }
}

// Called when the storage quota has been fetched, with or without any error.
- (void)didReceiveStorageQuotaResult:(const DriveStorageQuotaResult&)result {
  // Report storage quota histograms.
  base::UmaHistogramBoolean(kDriveStorageQuotaResultSuccessful,
                            result.error == nil);
  if (result.error) {
    base::UmaHistogramSparse(kDriveStorageQuotaResultErrorCode,
                             result.error.code);
  }
  // Stop validation spinner.
  [_accountPickerConsumer stopValidationSpinner];
  // Check that there is enough storage space to store an additional file of the
  // given size. The upload is expected to fail if the storage space usage after
  // upload exceeds the storage capacity.
  if (!result.error) {
    int64_t usageAfterUpload =
        result.usage + std::max(0LL, _downloadTask->GetTotalBytes());
    if (result.limit != -1 && usageAfterUpload > result.limit) {
      [_manageStorageAlertHandler
          showManageStorageAlertForIdentity:_fileUploader->GetIdentity()];
      base::UmaHistogramBoolean(kSaveToDriveUIManageStorageAlertShown, true);
      return;
    }
  }
  base::UmaHistogramBoolean(kSaveToDriveUIManageStorageAlertShown, false);
  // If storage quota could not be fetched or if there is enough storage to
  // upload the file, add the download task to the Drive tab helper, start the
  // task through the Download Manager tab helper and hide the account picker
  // view.
  DriveTabHelper* driveTabHelper =
      DriveTabHelper::GetOrCreateForWebState(_webState);
  driveTabHelper->AddDownloadToSaveToDrive(_downloadTask,
                                           _fileUploader->GetIdentity());
  DownloadManagerTabHelper* downloadManagerTabHelper =
      DownloadManagerTabHelper::FromWebState(_webState);
  downloadManagerTabHelper->StartDownload(_downloadTask);
  base::UmaHistogramEnumeration(kSaveToDriveUIOutcome,
                                SaveToDriveOutcome::kSuccessSelectedDrive);
  [self recordCommonHistogramsWithSuffix:".SuccessSelectedDrive"];
  [_accountPickerHandler hideAccountPickerAnimated:YES];
}

- (void)recordCommonHistogramsWithSuffix:(const char*)histogramSuffix {
  base::UmaHistogramEnumeration(
      std::string(kSaveToDriveUIMimeType) + histogramSuffix,
      GetDownloadMimeTypeResultFromMimeType(_downloadTask->GetMimeType()));
  if (_downloadTask->GetTotalBytes() != -1) {
    base::UmaHistogramMemoryMB(
        std::string(kSaveToDriveUIFileSize) + histogramSuffix,
        _downloadTask->GetTotalBytes() / kBytesPerMegabyte);
  }
  base::UmaHistogramCounts100(
      std::string(kSaveToDriveUINumberOfAttempts) + histogramSuffix,
      _numberOfAttempts);
}

@end
