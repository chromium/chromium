// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "file_path_watcher_histogram.h"

#include "base/metrics/histogram_functions.h"

namespace content {

void RecordWatchWithChangeInfoResultUma(WatchWithChangeInfoResult result) {
  base::UmaHistogramEnumeration(
      "Storage.FileSystemAccess.WatchWithChangeInfoResult", result);
}

void RecordCallbackErrorUma(WatchWithChangeInfoResult result) {
  base::UmaHistogramEnumeration(
      "Storage.FileSystemAccess.FilePathWatcherCallbackError", result);
}

void RecordInotifyWatchCountUma(int count) {
  base::UmaHistogramCounts10000(
      "Storage.FileSystemAccess.InotifyWatchCountPerFilePathWatcher", count);
}

}  // namespace content
