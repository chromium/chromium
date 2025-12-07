// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/spying_shared_handle_state_factory.h"

#include "base/memory/scoped_refptr.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/browser/file_system_access/mock_file_system_access_permission_grant.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

SpyingSharedHandleStateFactory::SpyingSharedHandleStateFactory() = default;

SpyingSharedHandleStateFactory::~SpyingSharedHandleStateFactory() = default;

FileSystemAccessManagerImpl::SharedHandleState
SpyingSharedHandleStateFactory::Build(
    scoped_refptr<FileSystemAccessPermissionGrant> read_grant,
    scoped_refptr<FileSystemAccessPermissionGrant> write_grant) {
  spying_read_grant_ = base::MakeRefCounted<
      testing::StrictMock<MockFileSystemAccessPermissionGrant>>(read_grant);
  spying_write_grant_ = base::MakeRefCounted<
      testing::StrictMock<MockFileSystemAccessPermissionGrant>>(write_grant);
  build_count_++;
  return FileSystemAccessManagerImpl::SharedHandleState(spying_read_grant_,
                                                        spying_write_grant_);
}

}  // namespace content
