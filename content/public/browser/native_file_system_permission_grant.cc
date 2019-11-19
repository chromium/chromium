// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/native_file_system_permission_grant.h"

namespace content {

void NativeFileSystemPermissionGrant::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void NativeFileSystemPermissionGrant::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

NativeFileSystemPermissionGrant::NativeFileSystemPermissionGrant() = default;
NativeFileSystemPermissionGrant::~NativeFileSystemPermissionGrant() = default;

void NativeFileSystemPermissionGrant::NotifyPermissionStatusChanged() {
  for (Observer& observer : observers_)
    observer.OnPermissionStatusChanged();
}

}  // namespace content
