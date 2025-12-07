// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_SPYING_SHARED_HANDLE_STATE_FACTORY_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_SPYING_SHARED_HANDLE_STATE_FACTORY_H_

#include "base/memory/scoped_refptr.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"

namespace content {

class MockFileSystemAccessPermissionGrant;

// A factory for creating SharedHandleState instances that use spying mock
// permission grants. This is useful for tests that want to verify that the
// correct permission grants are requested.
class SpyingSharedHandleStateFactory {
 public:
  SpyingSharedHandleStateFactory();
  ~SpyingSharedHandleStateFactory();

  // Creates a new SharedHandleState with spying mock permission grants that
  // spy on the passed in `read_grant` and `write_grant`.
  FileSystemAccessManagerImpl::SharedHandleState Build(
      scoped_refptr<FileSystemAccessPermissionGrant> read_grant,
      scoped_refptr<FileSystemAccessPermissionGrant> write_grant);

  // Returns the last built spying read grant.
  MockFileSystemAccessPermissionGrant* spying_read_grant() const {
    return spying_read_grant_.get();
  }

  // Returns the last built spying write grant.
  MockFileSystemAccessPermissionGrant* spying_write_grant() const {
    return spying_write_grant_.get();
  }

  // Returns the number of SharedHandleState instances that have been built.
  size_t build_count() const { return build_count_; }

 private:
  // The last built spying mock permission grants.
  scoped_refptr<MockFileSystemAccessPermissionGrant> spying_read_grant_;
  scoped_refptr<MockFileSystemAccessPermissionGrant> spying_write_grant_;

  // The number of times Build() has been called.
  size_t build_count_ = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_SPYING_SHARED_HANDLE_STATE_FACTORY_H_
