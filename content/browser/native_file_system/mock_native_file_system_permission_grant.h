// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NATIVE_FILE_SYSTEM_MOCK_NATIVE_FILE_SYSTEM_PERMISSION_GRANT_H_
#define CONTENT_BROWSER_NATIVE_FILE_SYSTEM_MOCK_NATIVE_FILE_SYSTEM_PERMISSION_GRANT_H_

#include "content/public/browser/native_file_system_permission_grant.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

// Mock NativeFileSystemPermissionGrant implementation.
class MockNativeFileSystemPermissionGrant
    : public NativeFileSystemPermissionGrant {
 public:
  MockNativeFileSystemPermissionGrant();

  MOCK_METHOD0(GetStatus, PermissionStatus());
  void RequestPermission(
      int process_id,
      int frame_id,
      base::OnceCallback<void(PermissionRequestOutcome)> callback) override;
  MOCK_METHOD3(RequestPermission_,
               void(int process_id,
                    int frame_id,
                    base::OnceCallback<void(PermissionRequestOutcome)>&));

  using NativeFileSystemPermissionGrant::NotifyPermissionStatusChanged;

 protected:
  ~MockNativeFileSystemPermissionGrant();
};

}  // namespace content

#endif  // CONTENT_BROWSER_NATIVE_FILE_SYSTEM_MOCK_NATIVE_FILE_SYSTEM_PERMISSION_GRANT_H_
