// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/file_system_access_permission_grant.h"

#include "base/observer_list.h"

namespace content {

void FileSystemAccessPermissionGrant::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FileSystemAccessPermissionGrant::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

FileSystemAccessPermissionGrant::FileSystemAccessPermissionGrant() = default;
FileSystemAccessPermissionGrant::~FileSystemAccessPermissionGrant() = default;

void FileSystemAccessPermissionGrant::NotifyPermissionStatusChanged() {
  for (Observer& observer : observers_)
    observer.OnPermissionStatusChanged();
}

}  // namespace content
