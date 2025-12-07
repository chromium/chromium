// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_HANDLE_IMPL_BROWSERTEST_UTIL_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_HANDLE_IMPL_BROWSERTEST_UTIL_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/test/content_browser_test.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_permission_mode.mojom.h"
#include "url/gurl.h"

namespace content {

class SpyingSharedHandleStateFactory;

// FileSystemAccessHandleImplBrowserTestBase is the base class for File System
// Access handle browser tests.
// It sets up a temporary directory and an embedded test server.
class FileSystemAccessHandleImplBrowserTestBase : public ContentBrowserTest {
 public:
  FileSystemAccessHandleImplBrowserTestBase();
  ~FileSystemAccessHandleImplBrowserTestBase() override;

  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void TearDown() override;

  // Creates a test directory in the temporary directory. This will also create
  // a corresponding FileSystemAccessDirectoryHandle in the renderer process.
  void CreateTestDirectoryInDirectory(const base::FilePath& directory_path);

  // Creates a test file in the temporary directory. This will also create
  // a corresponding FileSystemAccessFileHandle in the renderer process.
  void CreateTestFileInDirectory(const base::FilePath& directory_path,
                                 const std::string& contents);

 protected:
  base::ScopedTempDir temp_dir_;
  GURL test_url_;

 private:
  base::test::ScopedFeatureList scoped_features_;
};

// FileSystemAccessHandleImplPermissionBrowserTestBase is the base class for
// File System Access handle browser tests that need to mock and verify
// permission grants.
//
// In content_browsertests, the FileSystemAccessManagerImpl has a nullptr
// `permission_context_`. This test fixture sets up a
// SpyingSharedHandleStateFactory which provides mock grants for both read and
// write permissions. This allows tests to verify permission-related logic.
//
// After setup, tests can use `ExpectGetPermissionStatusAndReturnGranted()` to
// set up specific expectations for the mock permission grants' behavior.
class FileSystemAccessHandleImplPermissionBrowserTestBase
    : public FileSystemAccessHandleImplBrowserTestBase {
 public:
  FileSystemAccessHandleImplPermissionBrowserTestBase();
  ~FileSystemAccessHandleImplPermissionBrowserTestBase() override;

  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

 protected:
  // Sets up expectations on the mock permission grants.
  //
  // `expected_mode`: The permission mode that is expected to be requested.
  // `expected_shared_handle_state_count`: The number of shared handle states
  // that are expected to have been created.
  void ExpectGetPermissionStatusAndReturnGranted(
      blink::mojom::FileSystemAccessPermissionMode expected_mode,
      size_t expected_shared_handle_state_count = 1u);

  std::unique_ptr<SpyingSharedHandleStateFactory> spying_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_HANDLE_IMPL_BROWSERTEST_UTIL_H_
