// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/document_download_tab_helper.h"

#import "base/files/file_path.h"
#import "base/functional/callback.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/string_number_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/download/model/document_download_tab_helper_metrics.h"
#import "ios/chrome/browser/download/model/download_manager_tab_helper.h"
#import "ios/chrome/browser/download/model/download_mimetype_util.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/web/public/download/download_controller.h"
#import "ios/web/public/download/download_task.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "net/http/http_response_headers.h"

namespace {
// Whether `state` indicates a final state.
bool TaskStateIsDone(web::DownloadTask::State state) {
  switch (state) {
    case web::DownloadTask::State::kNotStarted:
    case web::DownloadTask::State::kInProgress:
      return false;
    case web::DownloadTask::State::kCancelled:
    case web::DownloadTask::State::kComplete:
    case web::DownloadTask::State::kFailed:
    case web::DownloadTask::State::kFailedNotResumable:
      return true;
  }
}

// Transitions `old_state` based on a new observed state.
// In some cases, it is possible that `Cancel()` will be called on a task which
// is already "done" i.e. Cancelled, Complete, Failed or FailedNotResumable. In
// that case the relevant state for metrics is the state before `Cancel()` is
// called. The `Cancelled` bucket only makes sense if the task went from
// `NotStarted`/`InProgress` to `Cancelled`.
web::DownloadTask::State NewObservedTaskState(
    web::DownloadTask::State old_state,
    web::DownloadTask::State new_state) {
  if (new_state != web::DownloadTask::State::kCancelled) {
    return new_state;
  }
  if (TaskStateIsDone(old_state)) {
    return old_state;
  }
  return new_state;
}

}  // namespace

DocumentDownloadTabHelper::DocumentDownloadTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  DCHECK(web_state);
  web_state->AddObserver(this);
}

DocumentDownloadTabHelper::~DocumentDownloadTabHelper() {
  if (observed_task_) {
    if (current_task_is_document_download_) {
      base::UmaHistogramEnumeration(
          kIOSDocumentDownloadFinalState,
          TaskStateToWebStateContentDownloadState(observed_task_state_));
      if (task_uuid_) {
        base::UmaHistogramEnumeration(
            kIOSDocumentDownloadStateAtNavigation,
            TaskStateToWebStateContentDownloadState(observed_task_state_));
      }
    }
    observed_task_->RemoveObserver(this);
    observed_task_ = nullptr;
  }
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
}

bool DocumentDownloadTabHelper::IsDownloadTaskCreatedByCurrentTabHelper() {
  return current_task_is_document_download_;
}

#pragma mark - web::WebStateObserver

void DocumentDownloadTabHelper::WebStateDestroyed(web::WebState* web_state) {
  DetachFullscreen();
  if (observed_task_) {
    if (current_task_is_document_download_) {
      base::UmaHistogramEnumeration(
          kIOSDocumentDownloadFinalState,
          TaskStateToWebStateContentDownloadState(observed_task_state_));
      if (task_uuid_) {
        base::UmaHistogramEnumeration(
            kIOSDocumentDownloadStateAtNavigation,
            TaskStateToWebStateContentDownloadState(observed_task_state_));
      }
    }
    observed_task_->RemoveObserver(this);
    observed_task_ = nullptr;
  }

  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}

void DocumentDownloadTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  DetachFullscreen();
  if (waiting_for_previous_task_) {
    base::UmaHistogramEnumeration(
        kIOSDocumentDownloadConflictResolution,
        DocumentDownloadConflictResolution::kPreviousDownloadDidNotFinish);
  }
  waiting_for_previous_task_ = false;
  if (task_uuid_) {
    // If a task was created on the previous navigation but was never started
    // by the user, cancel it.
    DownloadManagerTabHelper* tab_helper =
        DownloadManagerTabHelper::FromWebState(web_state_);
    CHECK(tab_helper);
    web::DownloadTask* active_task = tab_helper->GetActiveDownloadTask();
    if (active_task &&
        [active_task->GetIdentifier() isEqualToString:task_uuid_]) {
      base::UmaHistogramEnumeration(
          kIOSDocumentDownloadStateAtNavigation,
          TaskStateToWebStateContentDownloadState(observed_task_state_));
      if (active_task->GetState() == web::DownloadTask::State::kNotStarted) {
        base::UmaHistogramEnumeration(
            kIOSDocumentDownloadFinalState,
            TaskStateToWebStateContentDownloadState(
                web::DownloadTask::State::kNotStarted));
        if (observed_task_) {
          observed_task_->RemoveObserver(this);
          current_task_is_document_download_ = false;
          observed_task_ = nullptr;
        }
        active_task->Cancel();
      }
    }
    task_uuid_ = nil;
  }
}

void DocumentDownloadTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  file_size_ = -1;
  if (navigation_context->GetError() != nil) {
    return;
  }
  net::HttpResponseHeaders* headers = navigation_context->GetResponseHeaders();
  if (!headers) {
    return;
  }
  std::string content_size;
  if (!headers->GetNormalizedHeader("Content-Length", &content_size)) {
    return;
  }
  int64_t file_size;
  if (!base::StringToInt64(content_size, &file_size)) {
    return;
  }
  file_size_ = file_size <= 0 ? -1 : file_size;
}

void DocumentDownloadTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  DownloadManagerTabHelper* tab_helper =
      DownloadManagerTabHelper::FromWebState(web_state_);
  // Only trigger on success.
  bool should_trigger =
      load_completion_status == web::PageLoadCompletionStatus::SUCCESS;

  // Only trigger for non HTML document.
  should_trigger =
      should_trigger &&
      (!web_state->ContentIsHTML() &&
       !base::StartsWith(web_state->GetContentsMimeType(), "video/"));

  // Only triggers on http(s).
  GURL url = web_state->GetLastCommittedURL();
  should_trigger = should_trigger && url.SchemeIsHTTPOrHTTPS();

  if (should_trigger) {
    base::UmaHistogramEnumeration(kIOSDocumentDownloadMimeType,
                                  GetDownloadMimeTypeResultFromMimeType(
                                      web_state->GetContentsMimeType()));
    // -1 will be reported in the underflow bucket, the same as the 0 bucket.
    base::UmaHistogramMemoryMB(
        kIOSDocumentDownloadSizeInMB,
        file_size_ == -1 ? file_size_ : file_size_ / 1024 / 1024);

    web::DownloadTask* active_task = tab_helper->GetActiveDownloadTask();
    if (active_task) {
      if (observed_task_) {
        observed_task_->RemoveObserver(this);
      }
      // There is already an active download task. We don't want to prompt the
      // user on page load, so just observe this task. If it ever finishes while
      // the document is still displayed, we will trigger the new download.
      waiting_for_previous_task_ = true;
      active_task->AddObserver(this);
      current_task_is_document_download_ = false;
      observed_task_ = active_task;
      observed_task_state_ = observed_task_->GetState();
    } else {
      // There is no download running at the moment.
      // Create a new one to download the document on the current page.
      task_uuid_ = [NSUUID UUID].UUIDString;
      web::DownloadController::FromBrowserState(web_state_->GetBrowserState())
          ->CreateWebStateDownloadTask(web_state_, task_uuid_, file_size_);
      web::DownloadTask* new_task = tab_helper->GetActiveDownloadTask();
      if (new_task) {
        new_task->AddObserver(this);
        observed_task_ = new_task;
        observed_task_state_ = observed_task_->GetState();
        current_task_is_document_download_ = true;
      }
      AttachFullscreen();
      base::UmaHistogramEnumeration(
          kIOSDocumentDownloadConflictResolution,
          DocumentDownloadConflictResolution::kNoConflict);
      return;
    }
  }
  DetachFullscreen();
}

void DocumentDownloadTabHelper::WasShown(web::WebState* web_state) {
  AttachFullscreen();
}

void DocumentDownloadTabHelper::WasHidden(web::WebState* web_state) {
  DetachFullscreen();
}

#pragma mark - web::DownloadTaskObserver

void DocumentDownloadTabHelper::OnDownloadUpdated(web::DownloadTask* task) {
  observed_task_state_ =
      NewObservedTaskState(observed_task_state_, task->GetState());

  if (waiting_for_previous_task_) {
    return;
  }
  if (!task_uuid_) {
    task->RemoveObserver(this);
    observed_task_ = nullptr;
    DetachFullscreen();
    return;
  }
  if (![task->GetIdentifier() isEqualToString:task_uuid_]) {
    return;
  }
  DetachFullscreen();
}

void DocumentDownloadTabHelper::OnDownloadDestroyed(web::DownloadTask* task) {
  // Depending on the order in which observers are called,
  // OnDownloadUpdated(kCancelled) may or may not have been called.
  // To uniformize the reporting, update the state here.
  observed_task_state_ = NewObservedTaskState(
      observed_task_state_, web::DownloadTask::State::kCancelled);

  task->RemoveObserver(this);
  observed_task_ = nullptr;

  if (current_task_is_document_download_) {
    base::UmaHistogramEnumeration(
        kIOSDocumentDownloadFinalState,
        TaskStateToWebStateContentDownloadState(observed_task_state_));
    if (task_uuid_) {
      base::UmaHistogramEnumeration(
          kIOSDocumentDownloadStateAtNavigation,
          TaskStateToWebStateContentDownloadState(observed_task_state_));
    }
  }

  if (!waiting_for_previous_task_) {
    DetachFullscreen();
    return;
  }

  base::UmaHistogramEnumeration(
      kIOSDocumentDownloadConflictResolution,
      observed_task_state_ == web::DownloadTask::State::kCancelled
          ? DocumentDownloadConflictResolution::kPreviousDownloadWasCancelled
          : DocumentDownloadConflictResolution::kPreviousDownloadCompleted);

  // Post this task to let the other observer trigger (in particular,
  // DownloadManagerTabHelper will set a new active task).
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&DocumentDownloadTabHelper::OnPreviousTaskDeleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

#pragma mark - Private

void DocumentDownloadTabHelper::AttachFullscreen() {
  if (!task_uuid_) {
    return;
  }
  DownloadManagerTabHelper* tab_helper =
      DownloadManagerTabHelper::FromWebState(web_state_);
  web::DownloadTask* active_task = tab_helper->GetActiveDownloadTask();
  if (!active_task) {
    return;
  }
  if (![active_task->GetIdentifier() isEqualToString:task_uuid_] ||
      active_task->GetState() != web::DownloadTask::State::kNotStarted) {
    return;
  }
  tab_helper->AdaptToFullscreen(true);
}

void DocumentDownloadTabHelper::DetachFullscreen() {
  DownloadManagerTabHelper* tab_helper =
      DownloadManagerTabHelper::FromWebState(web_state_);
  tab_helper->AdaptToFullscreen(false);
}

void DocumentDownloadTabHelper::OnPreviousTaskDeleted() {
  DownloadManagerTabHelper* tab_helper =
      DownloadManagerTabHelper::FromWebState(web_state_);
  web::DownloadTask* active_task = tab_helper->GetActiveDownloadTask();
  if (active_task) {
    // There is still an active task. Still wait.
    active_task->AddObserver(this);
    current_task_is_document_download_ = false;
    observed_task_ = active_task;
    observed_task_state_ = observed_task_->GetState();
    waiting_for_previous_task_ = true;
    return;
  }

  // It is our turn. Trigger the download.
  waiting_for_previous_task_ = false;
  task_uuid_ = [NSUUID UUID].UUIDString;
  web::DownloadController::FromBrowserState(web_state_->GetBrowserState())
      ->CreateWebStateDownloadTask(web_state_, task_uuid_, file_size_);

  web::DownloadTask* new_task = tab_helper->GetActiveDownloadTask();
  if (new_task) {
    new_task->AddObserver(this);
    observed_task_ = new_task;
    observed_task_state_ = observed_task_->GetState();
    current_task_is_document_download_ = true;
  }
  AttachFullscreen();
}

WEB_STATE_USER_DATA_KEY_IMPL(DocumentDownloadTabHelper)
