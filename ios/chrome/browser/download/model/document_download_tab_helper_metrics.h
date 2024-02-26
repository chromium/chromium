// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOCUMENT_DOWNLOAD_TAB_HELPER_METRICS_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOCUMENT_DOWNLOAD_TAB_HELPER_METRICS_H_

#import <Foundation/Foundation.h>

#import "ios/web/public/download/download_task.h"

// Used as enum for the following histograms.
// - IOS.DocumentDownload.StateAtNavigation
// - IOS.DocumentDownload.FinalState
// Keep in sync with "DocumentDownloadState"
// in tools/metrics/histograms/metadata/ios/enums.xml.
// LINT.IfChange
enum class DocumentDownloadState {
  kNotStarted = 0,
  kInProgress,
  kCancelled,
  kComplete,
  kFailed,
  kFailedNotResumable,
  kMaxValue = kFailedNotResumable,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml)

// Converts a web::DownloadTask::State to a DocumentDownloadState;
DocumentDownloadState TaskStateToWebStateContentDownloadState(
    web::DownloadTask::State state);

// Used as enum for the following histograms.
// - IOS.DocumentDownload.ConflictResolution
// Keep in sync with "DocumentDownloadConflictResolution"
// in tools/metrics/histograms/metadata/ios/enums.xml.
// LINT.IfChange
enum class DocumentDownloadConflictResolution {
  kNoConflict = 0,
  kPreviousDownloadCompleted,
  kPreviousDownloadWasCancelled,
  kPreviousDownloadDidNotFinish,
  kMaxValue = kPreviousDownloadDidNotFinish,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml)

// Metric reporting the mime type of the document.
extern const char kIOSDocumentDownloadMimeType[];

// Metric reporting the size of the document in MB.
extern const char kIOSDocumentDownloadSizeInMB[];

// Metric reporting the outcome of a conflict if a download was already in
// progress.
extern const char kIOSDocumentDownloadConflictResolution[];

// Metric reporting the state of the document download when the next navigation
// is triggered.
extern const char kIOSDocumentDownloadStateAtNavigation[];

// Metric reporting the final state of the document download.
extern const char kIOSDocumentDownloadFinalState[];

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOCUMENT_DOWNLOAD_TAB_HELPER_METRICS_H_
