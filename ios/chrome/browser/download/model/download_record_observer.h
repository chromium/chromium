// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_OBSERVER_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_OBSERVER_H_

#import <string_view>

#import "base/observer_list_types.h"
#import "ios/web/public/download/download_task.h"

struct DownloadRecord;

// Observer interface for download record changes.
class DownloadRecordObserver : public base::CheckedObserver {
 public:
  // Called when a new download started.
  virtual void OnDownloadAdded(const DownloadRecord& record) {}

  // Called when a download's state changed.
  virtual void OnDownloadUpdated(std::string_view download_id,
                                 web::DownloadTask::State new_state) {}
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_OBSERVER_H_
