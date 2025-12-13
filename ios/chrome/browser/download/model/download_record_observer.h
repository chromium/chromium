// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_OBSERVER_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_OBSERVER_H_

#import <string_view>
#import <vector>

#import "base/observer_list_types.h"

struct DownloadRecord;

// Observer interface for download record changes.
class DownloadRecordObserver : public base::CheckedObserver {
 public:
  // Called when a new download started.
  virtual void OnDownloadAdded(const DownloadRecord& record) {}

  // Called when a download updated.
  virtual void OnDownloadUpdated(const DownloadRecord& record) {}

  // Called when downloads were removed.
  virtual void OnDownloadsRemoved(
      const std::vector<std::string_view>& download_ids) {}
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_OBSERVER_H_
