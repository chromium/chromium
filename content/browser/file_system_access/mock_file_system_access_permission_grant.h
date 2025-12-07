// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_MOCK_FILE_SYSTEM_ACCESS_PERMISSION_GRANT_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_MOCK_FILE_SYSTEM_ACCESS_PERMISSION_GRANT_H_

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/file_system_access_permission_grant.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

// Mock FileSystemAccessPermissionGrant implementation.
class MockFileSystemAccessPermissionGrant
    : public FileSystemAccessPermissionGrant {
 public:
  MockFileSystemAccessPermissionGrant();
  // Creates a mock grant that spies on the passed in `grant`.
  // All calls will then be forwarded to the real grant, unless a mock
  // expectation is set.
  explicit MockFileSystemAccessPermissionGrant(
      scoped_refptr<FileSystemAccessPermissionGrant> grant);

  MOCK_METHOD(PermissionStatus, GetStatus, (), (override));
  MOCK_METHOD(base::FilePath, GetPath, (), (override));
  MOCK_METHOD(std::string, GetDisplayName, (), (override));
  void RequestPermission(
      GlobalRenderFrameHostId frame_id,
      UserActivationState user_activation_state,
      base::OnceCallback<void(PermissionRequestOutcome)> callback) override;
  MOCK_METHOD(void,
              RequestPermission_,
              (GlobalRenderFrameHostId frame_id,
               UserActivationState user_activation_state,
               base::OnceCallback<void(PermissionRequestOutcome)>&));

  using FileSystemAccessPermissionGrant::NotifyPermissionStatusChanged;

 protected:
  ~MockFileSystemAccessPermissionGrant() override;

 private:
  scoped_refptr<FileSystemAccessPermissionGrant> grant_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_MOCK_FILE_SYSTEM_ACCESS_PERMISSION_GRANT_H_
