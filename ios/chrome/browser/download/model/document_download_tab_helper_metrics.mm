// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/document_download_tab_helper_metrics.h"

const char kIOSDocumentDownloadMimeType[] = "IOS.DocumentDownload.MimeType";
const char kIOSDocumentDownloadSizeInMB[] = "IOS.DocumentDownload.SizeInMB";
const char kIOSDocumentDownloadConflictResolution[] =
    "IOS.DocumentDownload.ConflictResolution";
const char kIOSDocumentDownloadStateAtNavigation[] =
    "IOS.DocumentDownload.StateAtNavigation";
const char kIOSDocumentDownloadFinalState[] = "IOS.DocumentDownload.FinalState";

DocumentDownloadState TaskStateToWebStateContentDownloadState(
    web::DownloadTask::State state) {
  switch (state) {
    case web::DownloadTask::State::kComplete:
      return DocumentDownloadState::kComplete;
    case web::DownloadTask::State::kFailed:
      return DocumentDownloadState::kFailed;
    case web::DownloadTask::State::kFailedNotResumable:
      return DocumentDownloadState::kFailedNotResumable;
    case web::DownloadTask::State::kNotStarted:
      return DocumentDownloadState::kNotStarted;
    case web::DownloadTask::State::kInProgress:
      return DocumentDownloadState::kInProgress;
    case web::DownloadTask::State::kCancelled:
      return DocumentDownloadState::kCancelled;
  }
}
