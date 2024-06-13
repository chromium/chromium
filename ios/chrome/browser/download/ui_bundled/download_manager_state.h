// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_UI_BUNDLED_DOWNLOAD_MANAGER_STATE_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_UI_BUNDLED_DOWNLOAD_MANAGER_STATE_H_

typedef NS_ENUM(NSInteger, DownloadManagerState) {
  // Download has not started yet.
  kDownloadManagerStateNotStarted = 0,
  // Download is actively progressing.
  kDownloadManagerStateInProgress,
  // Download is completely finished without errors.
  kDownloadManagerStateSucceeded,
  // Download has failed with an error.
  kDownloadManagerStateFailed,
  // Download has failed and cannot be resumed
  kDownloadManagerStateFailedNotResumable,
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_UI_BUNDLED_DOWNLOAD_MANAGER_STATE_H_
