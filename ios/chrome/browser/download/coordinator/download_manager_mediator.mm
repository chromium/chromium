// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/coordinator/download_manager_mediator.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/download/model/document_download_tab_helper.h"
#import "ios/chrome/browser/download/model/download_directory_util.h"
#import "ios/chrome/browser/download/model/download_manager_tab_helper.h"
#import "ios/chrome/browser/download/model/download_record_service.h"
#import "ios/chrome/browser/download/model/external_app_util.h"
#import "ios/chrome/browser/drive/model/drive_availability.h"
#import "ios/chrome/browser/drive/model/drive_tab_helper.h"
#import "ios/chrome/browser/drive/model/upload_task.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/download/download_task.h"
#import "net/base/net_errors.h"
#import "ui/base/l10n/l10n_util.h"

DownloadManagerMediator::DownloadManagerMediator() : weak_ptr_factory_(this) {}

DownloadManagerMediator::~DownloadManagerMediator() {
  DCHECK(!application_foregrounding_observer_);
  SetDownloadTask(nullptr);
  identity_manager_observation_.Reset();
  identity_manager_ = nullptr;
}

#pragma mark - Public

void DownloadManagerMediator::SetIsIncognito(bool is_incognito) {
  is_incognito_ = is_incognito;
}

void DownloadManagerMediator::SetIdentityManager(
    signin::IdentityManager* identity_manager) {
  identity_manager_observation_.Reset();
  identity_manager_ = identity_manager;
  if (identity_manager_) {
    identity_manager_observation_.Observe(identity_manager_);
    UpdateConsumer();
  }
}

void DownloadManagerMediator::SetDriveService(
    drive::DriveService* drive_service) {
  drive_service_ = drive_service;
}

void DownloadManagerMediator::SetPrefService(PrefService* pref_service) {
  pref_service_ = pref_service;
}

void DownloadManagerMediator::SetDownloadRecordService(
    DownloadRecordService* download_record_service) {
  CHECK(IsDownloadListEnabled());
  download_record_service_ = download_record_service;
}

void DownloadManagerMediator::SetConsumer(
    id<DownloadManagerConsumer> consumer) {
  consumer_ = consumer;
  SetGoogleDriveAppInstalled(IsGoogleDriveAppInstalled());
  UpdateConsumer();
}

void DownloadManagerMediator::SetDownloadTask(web::DownloadTask* task) {
  if (download_task_) {
    download_task_->GetWebState()->RemoveObserver(this);
    download_task_->RemoveObserver(this);
  }
  download_task_ = task;
  if (download_task_) {
    download_task_->AddObserver(this);
    download_task_->GetWebState()->AddObserver(this);
  }
  // Update upload task associated with `download_task_`.
  UpdateUploadTask();
  // In case download updates were missed, check for any.
  if (download_task_) {
    OnDownloadUpdated(download_task_);
  }
}

base::FilePath DownloadManagerMediator::GetDownloadPath() {
  DCHECK(download_task_);

  DownloadManagerTabHelper* tab_helper =
      DownloadManagerTabHelper::FromWebState(download_task_->GetWebState());
  return tab_helper->GetDownloadTaskFinalFilePath();
}

UploadTask* DownloadManagerMediator::GetUploadTask() {
  return upload_task_;
}

void DownloadManagerMediator::StartDownloading() {
  base::FilePath download_dir;
  if (!GetTempDownloadsDirectory(&download_dir)) {
    [consumer_ setState:DownloadManagerState::kFailed];
    return;
  }

  // Download will start once writer is created by background task, however it
  // OK to change consumer state now to preven further user interactions with
  // "Start Download" button.
  [consumer_ setState:DownloadManagerState::kInProgress];

  download_task_->Start(
      download_dir.Append(download_task_->GenerateFileName()));
  // If an upload task associated with the current download task exists, start
  // to observe it.
  UpdateUploadTask();

  // Record regular downloads (excludes Drive uploads).
  if (download_record_service_ && download_task_ && upload_task_ == nullptr) {
    download_record_service_->RecordDownload(download_task_);
  }
}

DownloadManagerState DownloadManagerMediator::GetDownloadManagerState() const {
  // Returns the `DownloadManagerState`, depending on the state of
  // `download_task_` and `upload_task_`.
  switch (download_task_->GetState()) {
    case web::DownloadTask::State::kNotStarted:
      return DownloadManagerState::kNotStarted;
    case web::DownloadTask::State::kInProgress:
      return DownloadManagerState::kInProgress;
    case web::DownloadTask::State::kComplete:
      if (!upload_task_) {
        return DownloadManagerState::kSucceeded;
      }
      switch (upload_task_->GetState()) {
        case UploadTask::State::kNotStarted:
        case UploadTask::State::kInProgress:
          return DownloadManagerState::kInProgress;
        case UploadTask::State::kCancelled:
          return DownloadManagerState::kNotStarted;
        case UploadTask::State::kComplete:
          return DownloadManagerState::kSucceeded;
        case UploadTask::State::kFailed:
          return DownloadManagerState::kFailed;
      }
    case web::DownloadTask::State::kFailed:
      return DownloadManagerState::kFailed;
    case web::DownloadTask::State::kFailedNotResumable:
      return DownloadManagerState::kFailedNotResumable;
    case web::DownloadTask::State::kCancelled:
      // Download Manager should dismiss the UI after download cancellation.
      return DownloadManagerState::kNotStarted;
  }
}

bool DownloadManagerMediator::IsSaveToDriveAvailable() const {
  return drive::IsSaveToDriveAvailable(is_incognito_, identity_manager_,
                                       drive_service_, pref_service_);
}

void DownloadManagerMediator::StartObservingNotifications() {
  DCHECK(!application_foregrounding_observer_);
  application_foregrounding_observer_ = [[NSNotificationCenter defaultCenter]
      addObserverForName:UIApplicationWillEnterForegroundNotification
                  object:nil
                   queue:nil
              usingBlock:
                  base::CallbackToBlock(
                      base::IgnoreArgs<NSNotification*>(base::BindRepeating(
                          &DownloadManagerMediator::AppWillEnterForeground,
                          weak_ptr_factory_.GetWeakPtr())))];
}

void DownloadManagerMediator::StopObservingNotifications() {
  if (application_foregrounding_observer_) {
    [[NSNotificationCenter defaultCenter]
        removeObserver:application_foregrounding_observer_];
    application_foregrounding_observer_ = nil;
  }
}

#pragma mark - Private

void DownloadManagerMediator::UpdateConsumer() {
  if (base::FeatureList::IsEnabled(kIOSDownloadNoUIUpdateInBackground) &&
      UIApplication.sharedApplication.applicationState ==
          UIApplicationStateBackground) {
    // If the app is in the background, do nothing.
    return;
  }
  if (!download_task_) {
    // If there is no download task, keep the latest state (not started or
    // finished) as it is not possible to determine what is the new state).
    return;
  }
  DownloadManagerState state = GetDownloadManagerState();

  base::FilePath download_path = GetDownloadPath();
  base::FilePath filename = download_path.empty()
                                ? download_task_->GenerateFileName()
                                : download_path.BaseName();
  [consumer_ setMultipleDestinationsAvailable:IsSaveToDriveAvailable()];
  DownloadFileDestination destination = upload_task_ == nullptr
                                            ? DownloadFileDestination::kFiles
                                            : DownloadFileDestination::kDrive;
  [consumer_ setDownloadFileDestination:destination];
  // Feed the identity user email to the consumer. If there is no upload task,
  // then `identity` and `identity.userEmail` will be nil, which is fine.
  id<SystemIdentity> identity =
      upload_task_ ? upload_task_->GetIdentity() : nil;
  [consumer_ setSaveToDriveUserEmail:identity.userEmail];
  [consumer_ setInstallDriveButtonVisible:!is_google_drive_app_installed_
                                 animated:NO];

  // A file can be opened if it is not already presented in the web state and
  // of type PDF.
  DocumentDownloadTabHelper* document_download_tab_helper =
      DocumentDownloadTabHelper::FromWebState(download_task_->GetWebState());
  BOOL can_open_file = !document_download_tab_helper
                            ->IsDownloadTaskCreatedByCurrentTabHelper() &&
                       filename.MatchesExtension(".pdf");
  [consumer_ setCanOpenFile:can_open_file];

  [consumer_ setState:state];
  [consumer_ setCountOfBytesReceived:download_task_->GetReceivedBytes()];
  [consumer_ setCountOfBytesExpectedToReceive:download_task_->GetTotalBytes()];
  [consumer_ setProgress:GetDownloadManagerProgress()];

  [consumer_ setFileName:base::apple::FilePathToNSString(filename)];

  NSString* originating_host = nil;
  bool display_originating_host = false;
  if (@available(iOS 18.2, *)) {
    // The originating host is only populated when compiled with iOS18.2 SDK
    // and running on iOS18.2.
    if ([download_task_->GetOriginatingHost() length]) {
      // Use the originating host provided by WKWebView.
      originating_host = download_task_->GetOriginatingHost();
    } else if (download_task_->GetRedirectedUrl().GetHost().size()) {
      // If originating host is not available (e.g. the download is triggered
      // by a data:// frame, use the download host instead).
      originating_host =
          base::SysUTF8ToNSString(download_task_->GetRedirectedUrl().GetHost());
    }
    // Only show the compute the originating host if it is not what is displayed
    // in the omnibox.
    display_originating_host =
        download_task_->GetWebState()->GetLastCommittedURL().GetHost() !=
        base::SysNSStringToUTF8(originating_host);

    // If the host was already displayed, keep it displayed
    display_originating_host = display_originating_host || should_show_origin_;
    should_show_origin_ = display_originating_host;
  }

  [consumer_ setOriginatingHost:originating_host
                        display:display_originating_host];

  int a11y_announcement = GetDownloadManagerA11yAnnouncement();
  if (a11y_announcement != -1) {
    UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification,
                                    l10n_util::GetNSString(a11y_announcement));
  }
}

void DownloadManagerMediator::SetGoogleDriveAppInstalled(bool installed) {
  is_google_drive_app_installed_ = installed;
}

int DownloadManagerMediator::GetDownloadManagerA11yAnnouncement() const {
  switch (GetDownloadManagerState()) {
    case DownloadManagerState::kNotStarted:
      return IDS_IOS_DOWNLOAD_MANAGER_REQUESTED_ACCESSIBILITY_ANNOUNCEMENT;
    case DownloadManagerState::kSucceeded:
    case DownloadManagerState::kFailed:
    case DownloadManagerState::kFailedNotResumable: {
      bool has_error = download_task_->GetErrorCode();
      if (!has_error && upload_task_) {
        has_error = upload_task_->GetError();
      }
      return has_error
                 ? IDS_IOS_DOWNLOAD_MANAGER_FAILED_ACCESSIBILITY_ANNOUNCEMENT
                 : IDS_IOS_DOWNLOAD_MANAGER_SUCCEEDED_ACCESSIBILITY_ANNOUNCEMENT;
    }
    case DownloadManagerState::kInProgress:
      return -1;
  }
}

float DownloadManagerMediator::GetDownloadManagerProgress() const {
  if (download_task_->GetPercentComplete() == -1) {
    return 0.0f;
  }
  float download_progress =
      static_cast<float>(download_task_->GetPercentComplete()) / 100.0f;
  if (!upload_task_) {
    return download_progress;
  }
  float save_to_drive_progress = upload_task_->GetProgress();
  // If the downloaded file needs to be uploaded to Drive, then the overall
  // progress is 50% once the download is complete, and then reaches 100% when
  // the upload is complete.
  return download_progress / 2.0 + save_to_drive_progress / 2.0;
}

void DownloadManagerMediator::UpdateUploadTask() {
  UploadTask* new_upload_task = nullptr;
  if (download_task_) {
    DriveTabHelper* drive_tab_helper =
        DriveTabHelper::GetOrCreateForWebState(download_task_->GetWebState());
    new_upload_task =
        drive_tab_helper->GetUploadTaskForDownload(download_task_);
  }
  SetUploadTask(new_upload_task);
}

void DownloadManagerMediator::SetUploadTask(UploadTask* task) {
  if (upload_task_) {
    upload_task_->RemoveObserver(this);
  }
  upload_task_ = task;
  if (upload_task_) {
    upload_task_->AddObserver(this);
    UpdateConsumer();
  }
}

void DownloadManagerMediator::AppWillEnterForeground() {
  CHECK(base::FeatureList::IsEnabled(kIOSDownloadNoUIUpdateInBackground));
  SetGoogleDriveAppInstalled(IsGoogleDriveAppInstalled());
  UpdateConsumer();
}

#pragma mark - web::WebStateObserver overrides

void DownloadManagerMediator::WebStateDestroyed(web::WebState* web_state) {
  // This should not be needed as DownloadTask should already be destroyed, but
  // if it is not the case, deattach anyway.
  SetDownloadTask(nullptr);
}

void DownloadManagerMediator::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  UpdateConsumer();
}

#pragma mark - web::DownloadTaskObserver overrides

void DownloadManagerMediator::OnDownloadUpdated(web::DownloadTask* task) {
  UpdateConsumer();
}

void DownloadManagerMediator::OnDownloadDestroyed(web::DownloadTask* task) {
  SetDownloadTask(nullptr);
}

#pragma mark - web::UploadTaskObserver overrides

void DownloadManagerMediator::OnUploadUpdated(UploadTask* task) {
  UpdateConsumer();
}

void DownloadManagerMediator::OnUploadDestroyed(UploadTask* task) {
  SetUploadTask(nullptr);
}

#pragma mark - signin::IdentityManager::Observer overrides

void DownloadManagerMediator::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  SetIdentityManager(nullptr);
}

void DownloadManagerMediator::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  UpdateConsumer();
}
