// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive/model/test_upload_task_observer.h"

#import "ios/chrome/browser/drive/model/upload_task.h"

TestUploadTaskObserver::TestUploadTaskObserver() = default;

TestUploadTaskObserver::~TestUploadTaskObserver() = default;

#pragma mark - Public

UploadTask* TestUploadTaskObserver::GetUpdatedUpload() const {
  return updated_upload_;
}

UploadTask* TestUploadTaskObserver::GetDestroyedUpload() const {
  return destroyed_upload_;
}

void TestUploadTaskObserver::ResetUpdatedUpload() {
  updated_upload_ = nullptr;
}

void TestUploadTaskObserver::ResetDestroyedUpload() {
  destroyed_upload_ = nullptr;
}

#pragma mark - TestUploadTaskObserver

void TestUploadTaskObserver::OnUploadUpdated(UploadTask* task) {
  updated_upload_ = task;
}

void TestUploadTaskObserver::OnUploadDestroyed(UploadTask* task) {
  destroyed_upload_ = task;
  task->RemoveObserver(this);
}
