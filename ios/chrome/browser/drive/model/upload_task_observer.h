// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_MODEL_UPLOAD_TASK_OBSERVER_H_
#define IOS_CHROME_BROWSER_DRIVE_MODEL_UPLOAD_TASK_OBSERVER_H_

#import "base/observer_list_types.h"

class UploadTask;

// Allows observation of UploadTask updates. All methods are called on UI
// thread.
class UploadTaskObserver : public base::CheckedObserver {
 public:
  UploadTaskObserver();
  UploadTaskObserver(const UploadTaskObserver&) = delete;
  UploadTaskObserver& operator=(const UploadTaskObserver&) = delete;
  ~UploadTaskObserver() override;

  // Called when the Upload task has started, uploaded a chunk of data or
  // the upload has been completed. Clients may call UploadTask::IsDone() to
  // check if the task has completed, UploadTask::GetProgress() to check
  // the upload progress.
  virtual void OnUploadUpdated(UploadTask* task);

  // Called when the upload task is about to be destructed. After this
  // callback all references to provided UploadTask should be cleared.
  virtual void OnUploadDestroyed(UploadTask* task) = 0;
};

#endif  // IOS_CHROME_BROWSER_DRIVE_MODEL_UPLOAD_TASK_OBSERVER_H_
