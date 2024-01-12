// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_MODEL_TEST_UPLOAD_TASK_OBSERVER_H_
#define IOS_CHROME_BROWSER_DRIVE_MODEL_TEST_UPLOAD_TASK_OBSERVER_H_

#import "ios/chrome/browser/drive/model/upload_task_observer.h"

// Testing implementation of `UploadTaskObserver`.
class TestUploadTaskObserver final : public UploadTaskObserver {
 public:
  TestUploadTaskObserver();
  ~TestUploadTaskObserver() final;

  UploadTask* GetUpdatedUpload() const;
  UploadTask* GetDestroyedUpload() const;
  void ResetUpdatedUpload();
  void ResetDestroyedUpload();

 private:
  // UploadTaskObserver overrides.
  void OnUploadUpdated(UploadTask* task) final;
  void OnUploadDestroyed(UploadTask* task) final;

  raw_ptr<UploadTask> updated_upload_ = nullptr;
  raw_ptr<UploadTask> destroyed_upload_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_DRIVE_MODEL_TEST_UPLOAD_TASK_OBSERVER_H_
