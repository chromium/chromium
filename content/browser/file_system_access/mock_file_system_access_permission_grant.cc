// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/mock_file_system_access_permission_grant.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace content {

MockFileSystemAccessPermissionGrant::MockFileSystemAccessPermissionGrant() =
    default;

MockFileSystemAccessPermissionGrant::MockFileSystemAccessPermissionGrant(
    scoped_refptr<FileSystemAccessPermissionGrant> grant)
    : grant_(grant) {
  ON_CALL(*this, GetStatus()).WillByDefault([this]() {
    return grant_->GetStatus();
  });
  ON_CALL(*this, GetPath()).WillByDefault([this]() {
    return grant_->GetPath();
  });
  ON_CALL(*this, GetDisplayName()).WillByDefault([this]() {
    return grant_->GetDisplayName();
  });
  ON_CALL(*this, RequestPermission_)
      .WillByDefault(
          [this](GlobalRenderFrameHostId frame_id,
                 UserActivationState user_activation_state,
                 base::OnceCallback<void(PermissionRequestOutcome)>& callback) {
            grant_->RequestPermission(frame_id, user_activation_state,
                                      std::move(callback));
          });

  // Defaults to allowing any number of calls to any method.
  // Users can override these by adding more specific expectations or entirely
  // clearing all by calling `testing::Mock::VerifyAndClear()`.
  EXPECT_CALL(*this, GetStatus()).Times(testing::AnyNumber());
  EXPECT_CALL(*this, GetPath()).Times(testing::AnyNumber());
  EXPECT_CALL(*this, GetDisplayName()).Times(testing::AnyNumber());
  EXPECT_CALL(*this, RequestPermission_).Times(testing::AnyNumber());
}

MockFileSystemAccessPermissionGrant::~MockFileSystemAccessPermissionGrant() =
    default;

void MockFileSystemAccessPermissionGrant::RequestPermission(
    GlobalRenderFrameHostId frame_id,
    UserActivationState user_activation_state,
    base::OnceCallback<void(PermissionRequestOutcome)> callback) {
  RequestPermission_(frame_id, user_activation_state, callback);
}

}  // namespace content
