// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/download_manager_tab_helper.h"

#import "base/check_op.h"
#import "base/feature_list.h"
#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/strings/sys_string_conversions.h"
#import "components/enterprise/common/proto/connectors.pb.h"
#import "components/enterprise/connectors/core/analysis_settings.h"
#import "components/enterprise/connectors/core/common.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/download/model/auto_deletion/auto_deletion_service.h"
#import "ios/chrome/browser/download/model/download_directory_util.h"
#import "ios/chrome/browser/download/model/download_file_service.h"
#import "ios/chrome/browser/download/model/download_file_service_factory.h"
#import "ios/chrome/browser/download/model/download_manager_tab_helper_delegate.h"
#import "ios/chrome/browser/drive/model/drive_availability.h"
#import "ios/chrome/browser/drive/model/drive_policy.h"
#import "ios/chrome/browser/drive/model/drive_service_factory.h"
#import "ios/chrome/browser/drive/model/drive_tab_helper.h"
#import "ios/chrome/browser/drive/model/upload_task.h"
#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/ios_analysis_request_handler.h"
#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/scan_decision_helper.h"
#import "ios/chrome/browser/enterprise/connectors/analysis/content_analysis_info.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/browser_util.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/download/download_task.h"
#import "ui/base/l10n/l10n_util_mac.h"

DownloadManagerTabHelper::DownloadManagerTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  DCHECK(web_state_);
  web_state_->AddObserver(this);
}

DownloadManagerTabHelper::~DownloadManagerTabHelper() {
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }

  CleanupCurrentDownload();
}

#pragma mark - Public methods

// static
bool DownloadManagerTabHelper::ShouldRestrictDownloadToFile(
    web::WebState* web_state) {
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state->GetBrowserState());
  PrefService* pref_service = profile->GetPrefs();
  return static_cast<policy::DownloadRestriction>(pref_service->GetInteger(
             policy::policy_prefs::kDownloadRestrictions)) ==
         policy::DownloadRestriction::ALL_FILES;
}

// static
bool DownloadManagerTabHelper::ShouldRestrictDownload(
    web::WebState* web_state) {
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state->GetBrowserState());
  PrefService* pref_service = profile->GetPrefs();
  AuthenticationService* auth_service =
      AuthenticationServiceFactory::GetForProfile(profile);
  bool is_save_to_drive_available = drive::IsSaveToDriveAvailable(
      profile->IsOffTheRecord(), IdentityManagerFactory::GetForProfile(profile),
      drive::DriveServiceFactory::GetForProfile(profile), pref_service,
      auth_service);
  return ShouldRestrictDownloadToFile(web_state) && !is_save_to_drive_available;
}

void DownloadManagerTabHelper::SetCurrentDownload(
    std::unique_ptr<web::DownloadTask> task) {
  // Check if the download should be restricted.
  if (task && ShouldRestrictDownload(web_state_)) {
    if (web_state_->IsVisible()) {
      ShowRestrictDownloadSnackbar();
    }
    return;
  }

  // If downloads are persistent, they cannot be lost once completed.
  if (!task_ || (task_->GetState() == web::DownloadTask::State::kComplete &&
                 !WillDownloadTaskBeSavedToDrive())) {
    // The task is the first download for this web state.
    DidCreateDownload(std::move(task));
    return;
  }

  // If there is no new task and an existing task is present, remove the
  // observer and reset the task.
  if (!task) {
    CleanupCurrentDownload();
    return;
  }

  // Capture a raw pointer to `task` before moving it into `callback`.
  web::DownloadTask* task_ptr = task.get();
  auto callback =
      base::BindOnce(&DownloadManagerTabHelper::OnDownloadPolicyDecision,
                     weak_ptr_factory_.GetWeakPtr(), std::move(task));

  [delegate_
      downloadManagerTabHelper:this
       decidePolicyForDownload:task_ptr
             completionHandler:base::CallbackToBlock(std::move(callback))];
}

const base::FilePath& DownloadManagerTabHelper::GetDownloadTaskFinalFilePath()
    const {
  return task_final_file_path_;
}

void DownloadManagerTabHelper::SetDelegate(
    id<DownloadManagerTabHelperDelegate> delegate) {
  if (delegate == delegate_) {
    return;
  }

  if (delegate_ && task_ && delegate_started_) {
    [delegate_ downloadManagerTabHelper:this
                        didHideDownload:task_.get()
                               animated:NO];
  }

  delegate_started_ = false;
  delegate_ = delegate;
}

void DownloadManagerTabHelper::SetSnackbarHandler(
    id<SnackbarCommands> snackbar_handler) {
  snackbar_handler_ = snackbar_handler;
}

void DownloadManagerTabHelper::StartDownload(web::DownloadTask* task) {
  DCHECK_EQ(task, task_.get());
  [delegate_ downloadManagerTabHelper:this wantsToStartDownload:task_.get()];
}

web::DownloadTask* DownloadManagerTabHelper::GetActiveDownloadTask() {
  return task_.get();
}

void DownloadManagerTabHelper::CleanupCurrentDownload() {
  if (task_) {
    if (delegate_ && delegate_started_) {
      delegate_started_ = false;
      [delegate_ downloadManagerTabHelper:this didCleanupDownload:task_.get()];
    }
    task_->RemoveObserver(this);
    task_.reset();
    task_final_file_path_.clear();
  }
}

void DownloadManagerTabHelper::AdaptToFullscreen(bool adapt_to_fullscreen) {
  if (delegate_ && delegate_started_) {
    [delegate_ downloadManagerTabHelper:this
                      adaptToFullscreen:adapt_to_fullscreen];
  }
}

bool DownloadManagerTabHelper::WillDownloadTaskBeSavedToDrive() const {
  DriveTabHelper* drive_tab_helper =
      DriveTabHelper::FromWebState(task_->GetWebState());
  UploadTask* upload_task =
      drive_tab_helper->GetUploadTaskForDownload(task_.get());
  return upload_task && !upload_task->IsDone();
}

#pragma mark - web::WebStateObserver

void DownloadManagerTabHelper::WasShown(web::WebState* web_state) {
  if (task_ && delegate_ && !delegate_started_) {
    if (ShouldRestrictDownload(web_state_)) {
      SetCurrentDownload(nullptr);
      return;
    }

    delegate_started_ = true;
    [delegate_ downloadManagerTabHelper:this
                        didShowDownload:task_.get()
                               animated:NO];
  }
}

void DownloadManagerTabHelper::WasHidden(web::WebState* web_state) {
  if (task_ && delegate_ && delegate_started_) {
    delegate_started_ = false;
    [delegate_ downloadManagerTabHelper:this
                        didHideDownload:task_.get()
                               animated:NO];
  }
}

void DownloadManagerTabHelper::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
  CleanupCurrentDownload();
}

#pragma mark - web::DownloadTaskObserver

void DownloadManagerTabHelper::OnDownloadUpdated(web::DownloadTask* task) {
  DCHECK_EQ(task, task_.get());
  switch (task->GetState()) {
    case web::DownloadTask::State::kCancelled:
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&DownloadManagerTabHelper::CleanupCurrentDownload,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    case web::DownloadTask::State::kInProgress:
      break;
    case web::DownloadTask::State::kComplete:
      DownloadManagerTabHelper::ProcessCompleteDownloadTask();
      break;
    case web::DownloadTask::State::kFailed:
    case web::DownloadTask::State::kFailedNotResumable:
      break;
    case web::DownloadTask::State::kNotStarted:
      // OnDownloadUpdated cannot be called with this state.
      NOTREACHED();
  }
}

#pragma mark - Private

void DownloadManagerTabHelper::DidCreateDownload(
    std::unique_ptr<web::DownloadTask> task) {
  CleanupCurrentDownload();
  task_ = std::move(task);
  task_->AddObserver(this);
  if (web_state_->IsVisible() && delegate_) {
    delegate_started_ = true;
    [delegate_ downloadManagerTabHelper:this
                      didCreateDownload:task_.get()
                      webStateIsVisible:true];
  }

  MaybeEnrollFileForAutoDeletion(task_.get());
}

void DownloadManagerTabHelper::OnDownloadPolicyDecision(
    std::unique_ptr<web::DownloadTask> task,
    NewDownloadPolicy policy) {
  if (policy == kNewDownloadPolicyReplace) {
    DidCreateDownload(std::move(task));
  }
}

void DownloadManagerTabHelper::ShowRestrictDownloadSnackbar() {
  if (!snackbar_handler_) {
    return;
  }

  [snackbar_handler_
      showSnackbarWithMessage:l10n_util::GetNSString(
                                  IDS_IOS_DOWNLOAD_RESTRICTION_SNACKBAR_TEXT)
                   buttonText:
                       l10n_util::GetNSString(
                           IDS_IOS_DOWNLOAD_RESTRICTION_SNACKBAR_BUTTON_TEXT)
                messageAction:nil
             completionAction:nil];
}

void DownloadManagerTabHelper::UseAvailableUserDocumentsPath(
    base::FilePath user_documents_path) {
  if (!task_) {
    return;
  }

  task_final_file_path_ = std::move(user_documents_path);
  base::FilePath task_path = task_->GetResponsePath();

  GetDownloadFileService()->CheckFileExists(
      task_path,
      base::BindOnce(&DownloadManagerTabHelper::MoveToUserDocumentsIfFileExists,
                     weak_ptr_factory_.GetWeakPtr(), task_path));
}

void DownloadManagerTabHelper::MoveToUserDocumentsIfFileExists(
    base::FilePath task_path,
    bool file_exists) {
  if (!file_exists || !task_) {
    return;
  }

  std::string download_id = base::SysNSStringToUTF8(task_->GetIdentifier());

  GetDownloadFileService()->MoveDownloadFile(
      download_id, task_path, GetDownloadTaskFinalFilePath(),
      base::BindOnce(&DownloadManagerTabHelper::MoveComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DownloadManagerTabHelper::MoveComplete(bool move_completed,
                                            const std::string& download_id,
                                            const base::FilePath& source_path,
                                            const base::FilePath& final_path) {
  DCHECK(move_completed);
  MaybeSetDownloadPathForAutoDeletion();
}

void DownloadManagerTabHelper::MaybeEnrollFileForAutoDeletion(
    web::DownloadTask* task) {
  PrefService* localState = GetApplicationContext()->GetLocalState();
  BOOL isAutoDeletionEnabled =
      localState->GetBoolean(prefs::kDownloadAutoDeletionEnabled);
  if (!IsDownloadAutoDeletionFeatureEnabled() || !isAutoDeletionEnabled) {
    return;
  }

  auto_deletion::AutoDeletionService* service =
      GetApplicationContext()->GetAutoDeletionService();
  service->SetDownloadTask(task);
}

void DownloadManagerTabHelper::MaybeSetDownloadPathForAutoDeletion() {
  PrefService* localState = GetApplicationContext()->GetLocalState();
  BOOL isAutoDeletionEnabled =
      localState->GetBoolean(prefs::kDownloadAutoDeletionEnabled);
  if (!IsDownloadAutoDeletionFeatureEnabled() || !isAutoDeletionEnabled) {
    return;
  }

  auto_deletion::AutoDeletionService* service =
      GetApplicationContext()->GetAutoDeletionService();
  service->SetDownloadPath(GetDownloadTaskFinalFilePath());
}

DownloadFileService* DownloadManagerTabHelper::GetDownloadFileService() {
  CHECK(web_state_);

  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  DownloadFileService* download_file_service =
      DownloadFileServiceFactory::GetForProfile(profile);

  CHECK(download_file_service);
  return download_file_service;
}

void DownloadManagerTabHelper::MaybeMoveDownloadToDownloadsDirectory(
    bool shouldProceed) {
  // Ensure the handler is destroyed as soon as it is no longer necessary.
  base::ScopedClosureRunner cleanup(base::BindOnce(
      [](std::unique_ptr<enterprise_connectors::IOSAnalysisRequestHandler>
             handler) {},
      std::move(analysis_request_handler_)));

  if (!shouldProceed) {
    CleanupCurrentDownload();
    return;
  }

  base::FilePath user_download_path;
  GetDownloadsDirectory(&user_download_path);
  base::FilePath base_file_name = task_->GenerateFileName();

  GetDownloadFileService()->ResolveAvailableFilePath(
      user_download_path, base_file_name,
      base::BindOnce(&DownloadManagerTabHelper::UseAvailableUserDocumentsPath,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DownloadManagerTabHelper::ProcessCompleteDownloadTask() {
  if (WillDownloadTaskBeSavedToDrive()) {
    return;
  }

  CHECK(web_state_);
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());

  const GURL& url = task_->GetRedirectedUrl();
  std::optional<enterprise_connectors::AnalysisSettings> settings =
      std::nullopt;

  enterprise_connectors::ConnectorsService* connectors_service =
      enterprise_connectors::ConnectorsServiceFactory::GetForProfile(profile);
  if (connectors_service) {
    settings = connectors_service->GetAnalysisSettings(
        url, enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED);
  }

  auto content_analysis_info =
      std::make_unique<enterprise_connectors::ContentAnalysisInfo>(
          url,
          settings.has_value() ? std::move(settings.value())
                               : enterprise_connectors::AnalysisSettings(),
          enterprise_connectors::ContentAnalysisRequest::NORMAL_DOWNLOAD,
          web_state_->GetWeakPtr());

  // Send the download file for enterprise DLP download content scanning.
  analysis_request_handler_ = std::make_unique<
      enterprise_connectors::IOSAnalysisRequestHandler>(
      std::move(content_analysis_info), profile, "",
      enterprise_connectors::DeepScanAccessPoint::DOWNLOAD,
      task_->GetResponsePath(),
      base::BindOnce(
          &enterprise_connectors::HandleScanDecision, web_state_->GetWeakPtr(),
          enterprise_connectors::TriggerType::kSavePrompt,
          base::BindOnce(
              &DownloadManagerTabHelper::MaybeMoveDownloadToDownloadsDirectory,
              weak_ptr_factory_.GetWeakPtr())));
  analysis_request_handler_->PrepareContentAnalysisRequest();
}
