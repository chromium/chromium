// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_MANAGER_STATE_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_MANAGER_STATE_H_

enum class DownloadManagerState : NSInteger {
  // Download has not started yet.
  kNotStarted = 0,
  // Download is actively progressing.
  kInProgress,
  // Download is completely finished without errors.
  kSucceeded,
  // Download has failed with an error.
  kFailed,
  // Download has failed and cannot be resumed
  kFailedNotResumable,
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_MANAGER_STATE_H_
