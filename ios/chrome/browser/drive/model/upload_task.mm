// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive/model/upload_task.h"

UploadTask::UploadTask() = default;

UploadTask::~UploadTask() {
  // TODO(crbug.com/1495354): Notify all observers of destruction.
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
  // TODO(crbug.com/1495354): Add `observer` to list of observers.
}

void UploadTask::RemoveObserver(UploadTaskObserver* observer) {
  // TODO(crbug.com/1495354): Remove `observer` from list of observers.
}

void UploadTask::OnUploadUpdated() {
  // TODO(crbug.com/1495354): Notify all observers of update.
}
