// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SHARING_SHARE_FILE_DOWNLOAD_METRICS_H_
#define IOS_CHROME_BROWSER_UI_SHARING_SHARE_FILE_DOWNLOAD_METRICS_H_

// UMA histogram names.
extern const char kOpenInDownloadHistogram[];

// Enum for the IOS.OpenIn.DownloadResult UMA histogram to log the result of
// the file download initiated when the user tap on "open in" button.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class OpenInDownloadResult {
  kSucceeded = 0,
  kCanceled = 1,
  kFailed = 2,
  kMaxValue = kFailed,
};

#endif  // IOS_CHROME_BROWSER_UI_SHARING_SHARE_FILE_DOWNLOAD_METRICS_H_
