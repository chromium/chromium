// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/save_to_drive/save_to_drive_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/download/model/download_manager_tab_helper.h"
#import "ios/chrome/browser/drive/model/drive_file_uploader.h"
#import "ios/chrome/browser/drive/model/drive_service.h"
#import "ios/chrome/browser/drive/model/drive_tab_helper.h"
#import "ios/chrome/browser/shared/public/commands/account_picker_commands.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/manage_storage_alert_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/save_to_drive_commands.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/ui/account_picker/account_picker_coordinator.h"
#import "ios/chrome/browser/ui/save_to_drive/file_destination.h"
#import "ios/chrome/browser/ui/save_to_drive/file_destination_picker_consumer.h"
#import "ios/web/public/download/download_task.h"
#import "ios/web/public/download/download_task_observer_bridge.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "net/base/url_util.h"
// TODO(crbug.com/1495352): Depend on account_picker_consumer.h directly.

@interface SaveToDriveMediator () <CRWWebStateObserver, CRWDownloadTaskObserver>

// Called when the storage quota has been fetched, with or without any error.
- (void)didReceiveStorageQuotaResult:(DriveStorageQuotaResult)result;

@end

namespace {

// URL template to redirect a user to the URL given as the `continue` query
// parameter, using the Google account given as the `Email` query parameter.
// This way, if the URL given as the `continue` query parameter is
// `kManageStorageURL`, the user will be redirected to manage the storage
// of the account associated with the given email.
constexpr char kAccountChooserRedirectionUrlTemplate[] =
    "https://accounts.google.com/AccountChooser";
constexpr char kAccountChooserRedirectionUrlEmailParameterName[] = "Email";
constexpr char kAccountChooserRedirectionUrlContinueParameterName[] =
    "continue";
// URL to let the user manage their Google storage.
constexpr char kManageStorageURL[] = "https://one.google.com/storage";

void StorageQuotaCompletionHelper(__weak SaveToDriveMediator* mediator,
                                  DriveStorageQuotaResult result) {
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
  FileDestination _fileDestination;
  // The file uploader is used to fetch the storage quota for a given identity.
  std::unique_ptr<DriveFileUploader> _fileUploader;
}

- (instancetype)initWithDownloadTask:(web::DownloadTask*)downloadTask
                  saveToDriveHandler:(id<SaveToDriveCommands>)saveToDriveHandler
           manageStorageAlertHandler:
               (id<ManageStorageAlertCommands>)manageStorageAlertHandler
                  applicationHandler:(id<ApplicationCommands>)applicationHandler
                accountPickerHandler:
                    (id<AccountPickerCommands>)accountPickerHandler
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
    _driveService = driveService;
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
  _driveService = nullptr;
  _saveToDriveHandler = nil;
}

- (void)saveWithSelectedIdentity:(id<SystemIdentity>)identity {
  if (!_downloadTask || !_webState) {
    [_saveToDriveHandler hideSaveToDrive];
    return;
  }
  switch (_fileDestination) {
    case FileDestination::kFiles: {
      // If the selected file destination is Files, start the download
      // immediately and hide the account picker.
      DownloadManagerTabHelper* downloadManagerTabHelper =
          DownloadManagerTabHelper::FromWebState(_webState);
      downloadManagerTabHelper->StartDownload(_downloadTask);
      [_accountPickerHandler hideAccountPickerAnimated:YES];
      break;
    }
    case FileDestination::kDrive: {
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
  GURL manageStorageURL(kAccountChooserRedirectionUrlTemplate);
  // Set 'Email' query parameter.
  manageStorageURL = net::AppendQueryParameter(
      manageStorageURL, kAccountChooserRedirectionUrlEmailParameterName,
      base::SysNSStringToUTF8(identity.userEmail));
  // Set 'continue' query parameter.
  manageStorageURL = net::AppendQueryParameter(
      manageStorageURL, kAccountChooserRedirectionUrlContinueParameterName,
      kManageStorageURL);
  OpenNewTabCommand* newTabCommand =
      [OpenNewTabCommand commandWithURLFromChrome:manageStorageURL];
  [_applicationHandler openURLInNewTab:newTabCommand];
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
  [_saveToDriveHandler hideSaveToDrive];
}

#pragma mark - CRWWebStateObserver

- (void)webStateWasHidden:(web::WebState*)webState {
  [_saveToDriveHandler hideSaveToDrive];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  CHECK_EQ(webState, _webState);
  webState->RemoveObserver(_webStateObserverBridge.get());
  _webStateObserverBridge = nullptr;
  _webState = nullptr;
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
  bool destinationIsFiles = _fileDestination == FileDestination::kFiles;
  [self.accountPickerConsumer setIdentityButtonHidden:destinationIsFiles
                                             animated:animated];
  [self.destinationPickerConsumer setSelectedDestination:_fileDestination];
}

// Called when the storage quota has been fetched, with or without any error.
- (void)didReceiveStorageQuotaResult:(DriveStorageQuotaResult)result {
  [_accountPickerConsumer stopValidationSpinner];
  if (result.error) {
    // TODO(crbug.com/1495352): Show an error message.
    return;
  }
  // Check that there is enough storage space to store an additional file of the
  // given size. The upload is expected to fail if the storage space usage after
  // upload exceeds the storage capacity.
  int64_t usageAfterUpload =
      result.usage + std::max(0LL, _downloadTask->GetTotalBytes());
  if (result.limit != -1 && usageAfterUpload > result.limit) {
    [_manageStorageAlertHandler
        showManageStorageAlertForIdentity:_fileUploader->GetIdentity()];
    return;
  }
  // If there is enough storage to upload the file, add the download task to the
  // Drive tab helper, start the task through the Download Manager tab helper
  // and hide the account picker view.
  DriveTabHelper* driveTabHelper = DriveTabHelper::FromWebState(_webState);
  driveTabHelper->AddDownloadToSaveToDrive(_downloadTask,
                                           _fileUploader->GetIdentity());
  DownloadManagerTabHelper* downloadManagerTabHelper =
      DownloadManagerTabHelper::FromWebState(_webState);
  downloadManagerTabHelper->StartDownload(_downloadTask);
  [_accountPickerHandler hideAccountPickerAnimated:YES];
}

@end
