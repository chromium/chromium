// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_DOWNLOAD_DOWNLOAD_TASK_OBSERVER_H_
#define IOS_WEB_PUBLIC_DOWNLOAD_DOWNLOAD_TASK_OBSERVER_H_

#include "base/observer_list_types.h"

namespace web {

class DownloadTask;

// Allows observation of DownloadTask updates. All methods are called on UI
// thread.
class DownloadTaskObserver : public base::CheckedObserver {
 public:
  // Called when the download task has started, downloaded a chunk of data or
  // the download has been completed. Clients may call DownloadTask::IsDone() to
  // check if the task has completed, call DownloadTask::GetErrorCode() to check
  // if the download has failed, DownloadTask::GetPercentComplete() to check
  // the download progress, and DownloadTask::GetResponseWriter() to obtain the
  // downloaded data.
  virtual void OnDownloadUpdated(DownloadTask* task) {}

  // Called when the download task is about to be destructed. After this
  // callback all references to provided DownloadTask should be cleared.
  virtual void OnDownloadDestroyed(DownloadTask* task) {}

  DownloadTaskObserver() = default;

  DownloadTaskObserver(const DownloadTaskObserver&) = delete;
  DownloadTaskObserver& operator=(const DownloadTaskObserver&) = delete;

  ~DownloadTaskObserver() override;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_DOWNLOAD_DOWNLOAD_TASK_OBSERVER_H_
