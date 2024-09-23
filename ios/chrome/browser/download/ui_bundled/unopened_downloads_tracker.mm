// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/ui_bundled/unopened_downloads_tracker.h"

#import <Foundation/Foundation.h>

#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/download/model/download_manager_metric_names.h"
#import "ios/chrome/browser/download/model/external_app_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/download/download_task.h"
#import "net/base/net_errors.h"

UnopenedDownloadsTracker::UnopenedDownloadsTracker() {}

UnopenedDownloadsTracker::~UnopenedDownloadsTracker() {
  for (web::DownloadTask* task : observed_tasks_) {
    task->RemoveObserver(this);
    DownloadAborted(task);
  }
}

void UnopenedDownloadsTracker::Add(web::DownloadTask* task) {
  task->AddObserver(this);
  observed_tasks_.insert(task);
}

void UnopenedDownloadsTracker::Remove(web::DownloadTask* task) {
  task->RemoveObserver(this);
  observed_tasks_.erase(task);
}

void UnopenedDownloadsTracker::OnDownloadUpdated(web::DownloadTask* task) {
  if (task->IsDone()) {
    base::UmaHistogramEnumeration("Download.IOSDownloadFileResult",
                                  task->GetErrorCode()
                                      ? DownloadFileResult::Failure
                                      : DownloadFileResult::Completed,
                                  DownloadFileResult::Count);
    if (task->GetErrorCode()) {
      base::UmaHistogramSparse("Download.IOSDownloadedFileNetError",
                               -task->GetErrorCode());
    } else {
      bool GoogleDriveIsInstalled = IsGoogleDriveAppInstalled();
      if (GoogleDriveIsInstalled) {
        base::UmaHistogramEnumeration(
            "Download.IOSDownloadFileUIGoogleDrive",
            DownloadFileUIGoogleDrive::GoogleDriveAlreadyInstalled,
            DownloadFileUIGoogleDrive::Count);
      } else {
        base::UmaHistogramEnumeration(
            "Download.IOSDownloadFileUIGoogleDrive",
            DownloadFileUIGoogleDrive::GoogleDriveNotInstalled,
            DownloadFileUIGoogleDrive::Count);
      }
    }

    bool backgrounded = task->HasPerformedBackgroundDownload();
    DownloadFileInBackground histogram_value =
        task->GetErrorCode()
            ? (backgrounded
                   ? DownloadFileInBackground::FailedWithBackgrounding
                   : DownloadFileInBackground::FailedWithoutBackgrounding)
            : (backgrounded
                   ? DownloadFileInBackground::SucceededWithBackgrounding
                   : DownloadFileInBackground::SucceededWithoutBackgrounding);
    base::UmaHistogramEnumeration("Download.IOSDownloadFileInBackground",
                                  histogram_value,
                                  DownloadFileInBackground::Count);
  }
}

void UnopenedDownloadsTracker::OnDownloadDestroyed(web::DownloadTask* task) {
  // This download task was never open by the user.
  task->RemoveObserver(this);
  observed_tasks_.erase(task);

  DownloadAborted(task);
}

void UnopenedDownloadsTracker::DownloadAborted(web::DownloadTask* task) {
  if (task->GetState() == web::DownloadTask::State::kInProgress) {
    base::UmaHistogramEnumeration("Download.IOSDownloadFileResult",
                                  DownloadFileResult::Other,
                                  DownloadFileResult::Count);

    if (did_close_web_state_without_user_action) {
      // web state can be closed without user action only during the app
      // shutdown.
      base::UmaHistogramEnumeration(
          "Download.IOSDownloadFileInBackground",
          DownloadFileInBackground::CanceledAfterAppQuit,
          DownloadFileInBackground::Count);
    }
  }

  if (task->IsDone() && task->GetErrorCode() == net::OK) {
    base::UmaHistogramEnumeration(
        "Download.IOSDownloadedFileAction",
        DownloadedFileAction::NoActionOrOpenedViaExtension,
        DownloadedFileAction::Count);
  }
}

void UnopenedDownloadsTracker::WebStateListWillChange(
    WebStateList* web_state_list,
    const WebStateListChangeDetach& detach_change,
    const WebStateListStatus& status) {
  if (!detach_change.is_closing()) {
    return;
  }

  if (!detach_change.is_user_action()) {
    did_close_web_state_without_user_action = true;
  }
}
