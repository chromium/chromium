// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive/model/upload_task.h"

#import "ios/chrome/browser/drive/model/upload_task_observer.h"

UploadTask::UploadTask() = default;

UploadTask::~UploadTask() {
  for (auto& observer : observers_) {
    observer.OnUploadDestroyed(this);
  }
}

#pragma mark - Public

bool UploadTask::IsDone() const {
  switch (GetState()) {
    case State::kNotStarted:
    case State::kInProgress:
      return false;
    case State::kCancelled:
    case State::kComplete:
    case State::kFailed:
      return true;
  }
}

void UploadTask::AddObserver(UploadTaskObserver* observer) {
  observers_.AddObserver(observer);
}

void UploadTask::RemoveObserver(UploadTaskObserver* observer) {
  observers_.RemoveObserver(observer);
}

void UploadTask::OnUploadUpdated() {
  for (auto& observer : observers_) {
    observer.OnUploadUpdated(this);
  }
}
