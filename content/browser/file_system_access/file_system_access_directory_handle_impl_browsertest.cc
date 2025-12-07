// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_handle_impl_browsertest_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/file_system_chooser_test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"

namespace content {

namespace {

using blink::mojom::FileSystemAccessPermissionMode;
using testing::_;
using testing::Return;

}  // namespace

class FileSystemAccessDirectoryHandleImplBrowserTest
    : public FileSystemAccessHandleImplBrowserTestBase {};

// Test params for `FileSystemAccessDirectoryHandleImplWriteModeBrowserTest`.
struct WriteModeTestParams {
  const char* test_name_suffix;
  bool is_feature_enabled;
  FileSystemAccessPermissionMode expected_mode;
};

// Browser tests for FileSystemAccessDirectoryHandleImpl-related APIs, with a
// mock permission context to verify write permissions-related logic.
class FileSystemAccessDirectoryHandleImplWriteModeBrowserTest
    : public FileSystemAccessHandleImplPermissionBrowserTestBase,
      public testing::WithParamInterface<WriteModeTestParams> {
 public:
  FileSystemAccessDirectoryHandleImplWriteModeBrowserTest() {
    if (GetParam().is_feature_enabled) {
      scoped_feature_list_.InitAndEnableFeature(
          blink::features::kFileSystemAccessWriteMode);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          blink::features::kFileSystemAccessWriteMode);
    }
  }

 protected:
  // Creates a test file in the directory created by
  // `CreateTestDirectoryInDirectory()`.
  // This must be called before `SetPermissionContextForTesting()`.
  void CreateTestFile(const std::string& file_name,
                      const std::string& contents) {
    EXPECT_EQ(true, EvalJs(shell(), JsReplace(R"((async () => {
        const handle = await self.localDir.getFileHandle($1, {create: true});
        const writable = await handle.createWritable();
        await writable.write($2);
        await writable.close();
        return true;
      })())",
                                              file_name, contents)));
  }

  // Creates a test file in the sandboxed file system.
  // This must be called before `SetPermissionContextForTesting()`.
  void CreateSandboxedTestFile(const std::string& file_name,
                               const std::string& contents) {
    EXPECT_EQ(true, EvalJs(shell(), JsReplace(R"((async () => {
        self.sandboxDir = await navigator.storage.getDirectory();
        const handle = await self.sandboxDir.getFileHandle($1, {create: true});
        const writable = await handle.createWritable();
        await writable.write($2);
        await writable.close();
        return true;
      })())",
                                              file_name, contents)));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies that `remove()` on a local FS requests the correct
// permission mode depending on whether the `kFileSystemAccessWriteMode`
// feature is enabled.
IN_PROC_BROWSER_TEST_P(FileSystemAccessDirectoryHandleImplWriteModeBrowserTest,
                       Local_Remove_RequestsCorrectPermissions) {
  CreateTestDirectoryInDirectory(temp_dir_.GetPath());
  CreateTestFile("test.txt", "some content");

  ExpectGetPermissionStatusAndReturnGranted(GetParam().expected_mode);

  EXPECT_TRUE(ExecJs(
      shell(),
      "(async () => { await self.localDir.remove({recursive: true}); })()"));
}

// Verifies that `remove()` on a sandboxed FS requests the correct
// permission mode depending on whether the `kFileSystemAccessWriteMode`
// feature is enabled.
IN_PROC_BROWSER_TEST_P(FileSystemAccessDirectoryHandleImplWriteModeBrowserTest,
                       Sandboxed_Remove_RequestsCorrectPermissions) {
  EXPECT_TRUE(NavigateToURL(shell(), test_url_));
  CreateSandboxedTestFile("test.txt", "some content");

  ExpectGetPermissionStatusAndReturnGranted(GetParam().expected_mode);

  EXPECT_TRUE(ExecJs(shell(),
                     "(async () => {"
                     "  await self.sandboxDir.remove({recursive: true});"
                     "})()"));
}

// Verifies that `removeEntry()` on a local FS requests the correct
// permission mode depending on whether the `kFileSystemAccessWriteMode`
// feature is enabled.
IN_PROC_BROWSER_TEST_P(FileSystemAccessDirectoryHandleImplWriteModeBrowserTest,
                       Local_RemoveEntry_RequestsCorrectPermissions) {
  CreateTestDirectoryInDirectory(temp_dir_.GetPath());
  CreateTestFile("test.txt", "some content");

  ExpectGetPermissionStatusAndReturnGranted(GetParam().expected_mode);

  EXPECT_TRUE(ExecJs(
      shell(),
      "(async () => { await self.localDir.removeEntry('test.txt'); })()"));
}

// Verifies that `removeEntry()` on a sandboxed FS requests the correct
// permission mode depending on whether the `kFileSystemAccessWriteMode`
// feature is enabled.
IN_PROC_BROWSER_TEST_P(FileSystemAccessDirectoryHandleImplWriteModeBrowserTest,
                       Sandboxed_RemoveEntry_RequestsCorrectPermissions) {
  EXPECT_TRUE(NavigateToURL(shell(), test_url_));
  CreateSandboxedTestFile("test.txt", "some content");

  ExpectGetPermissionStatusAndReturnGranted(GetParam().expected_mode);

  EXPECT_TRUE(ExecJs(shell(),
                     R"((async () => {
        await self.sandboxDir.removeEntry('test.txt');
      })())"));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    FileSystemAccessDirectoryHandleImplWriteModeBrowserTest,
    testing::Values(
        WriteModeTestParams{"WriteModeDisabled", false,
                            FileSystemAccessPermissionMode::kReadWrite},
        WriteModeTestParams{"WriteModeEnabled", true,
                            FileSystemAccessPermissionMode::kWrite}),
    [](const testing::TestParamInfo<WriteModeTestParams>& info) {
      return info.param.test_name_suffix;
    });

}  // namespace content
