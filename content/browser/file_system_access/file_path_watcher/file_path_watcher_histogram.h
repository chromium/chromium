// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_PATH_WATCHER_FILE_PATH_WATCHER_HISTOGRAM_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_PATH_WATCHER_FILE_PATH_WATCHER_HISTOGRAM_H_

namespace content {

enum class WatchWithChangeInfoResult {
  kSuccess = 0,
  kInotifyWatchLimitExceeded = 1,
  kWinCreateFileHandleErrorFatal = 2,
  kWinReachedRootDirectory = 3,
  kWinCreateIoCompletionPortError = 4,
  kWinReadDirectoryChangesWError = 5,
  kMaxValue = kWinReadDirectoryChangesWError,
};

void RecordWatchWithChangeInfoResultUma(WatchWithChangeInfoResult result);

void RecordCallbackErrorUma(WatchWithChangeInfoResult result);

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_PATH_WATCHER_FILE_PATH_WATCHER_HISTOGRAM_H_
