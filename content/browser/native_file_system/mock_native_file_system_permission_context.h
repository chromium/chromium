// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NATIVE_FILE_SYSTEM_MOCK_NATIVE_FILE_SYSTEM_PERMISSION_CONTEXT_H_
#define CONTENT_BROWSER_NATIVE_FILE_SYSTEM_MOCK_NATIVE_FILE_SYSTEM_PERMISSION_CONTEXT_H_

#include "content/public/browser/native_file_system_permission_context.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {
// Mock NativeFileSystemPermissionContext implementation.
class MockNativeFileSystemPermissionContext
    : public NativeFileSystemPermissionContext {
 public:
  MockNativeFileSystemPermissionContext();
  ~MockNativeFileSystemPermissionContext();

  MOCK_METHOD5(
      GetReadPermissionGrant,
      scoped_refptr<NativeFileSystemPermissionGrant>(const url::Origin& origin,
                                                     const base::FilePath& path,
                                                     bool is_directory,
                                                     int process_id,
                                                     int frame_id));

  MOCK_METHOD6(GetWritePermissionGrant,
               scoped_refptr<NativeFileSystemPermissionGrant>(
                   const url::Origin& origin,
                   const base::FilePath& path,
                   bool is_directory,
                   int process_id,
                   int frame_id,
                   NativeFileSystemPermissionContext::UserAction user_action));

  void ConfirmDirectoryReadAccess(
      const url::Origin& origin,
      const base::FilePath& path,
      int process_id,
      int frame_id,
      base::OnceCallback<void(PermissionStatus)> callback) override;
  MOCK_METHOD5(ConfirmDirectoryReadAccess_,
               void(const url::Origin& origin,
                    const base::FilePath& path,
                    int process_id,
                    int frame_id,
                    base::OnceCallback<void(PermissionStatus)>& callback));

  void ConfirmSensitiveDirectoryAccess(
      const url::Origin& origin,
      const std::vector<base::FilePath>& paths,
      bool is_directory,
      int process_id,
      int frame_id,
      base::OnceCallback<void(SensitiveDirectoryResult)> callback) override;
  MOCK_METHOD6(
      ConfirmSensitiveDirectoryAccess_,
      void(const url::Origin& origin,
           const std::vector<base::FilePath>& paths,
           bool is_directory,
           int process_id,
           int frame_id,
           base::OnceCallback<void(SensitiveDirectoryResult)>& callback));

  void PerformAfterWriteChecks(
      std::unique_ptr<NativeFileSystemWriteItem> item,
      int process_id,
      int frame_id,
      base::OnceCallback<void(AfterWriteCheckResult)> callback) override;
  MOCK_METHOD4(PerformAfterWriteChecks_,
               void(NativeFileSystemWriteItem* item,
                    int process_id,
                    int frame_id,
                    base::OnceCallback<void(AfterWriteCheckResult)>& callback));

  MOCK_METHOD1(CanRequestWritePermission, bool(const url::Origin& origin));
};

}  // namespace content

#endif  // CONTENT_BROWSER_NATIVE_FILE_SYSTEM_MOCK_NATIVE_FILE_SYSTEM_PERMISSION_CONTEXT_H_
