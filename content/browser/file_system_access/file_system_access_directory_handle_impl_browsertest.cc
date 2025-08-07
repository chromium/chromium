// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/browser/file_system_access/mock_file_system_access_permission_context.h"
#include "content/browser/file_system_access/mock_file_system_access_permission_grant.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fake_file_system_access_permission_context.h"
#include "content/public/test/file_system_chooser_test_helpers.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"

namespace content {

namespace {

using blink::mojom::FileSystemAccessPermissionMode;
using testing::_;
using testing::Invoke;
using testing::Return;

// For injecting mock permission grants into the FileSystemAccessManager.
class TestPermissionContext : public FakeFileSystemAccessPermissionContext {
 public:
  TestPermissionContext() {
    read_grant_ = base::MakeRefCounted<
        testing::StrictMock<MockFileSystemAccessPermissionGrant>>();
    write_grant_ = base::MakeRefCounted<
        testing::StrictMock<MockFileSystemAccessPermissionGrant>>();
  }
  ~TestPermissionContext() override = default;

  scoped_refptr<FileSystemAccessPermissionGrant> GetReadPermissionGrant(
      const url::Origin& origin,
      const PathInfo& path,
      HandleType handle_type,
      UserAction user_action) override {
    return read_grant_;
  }

  scoped_refptr<FileSystemAccessPermissionGrant> GetWritePermissionGrant(
      const url::Origin& origin,
      const PathInfo& path,
      HandleType handle_type,
      UserAction user_action) override {
    return write_grant_;
  }

  MockFileSystemAccessPermissionGrant& read_grant() { return *read_grant_; }
  MockFileSystemAccessPermissionGrant& write_grant() { return *write_grant_; }

 private:
  scoped_refptr<testing::StrictMock<MockFileSystemAccessPermissionGrant>>
      read_grant_;
  scoped_refptr<testing::StrictMock<MockFileSystemAccessPermissionGrant>>
      write_grant_;
};

}  // namespace

class FileSystemAccessDirectoryHandleImplBrowserTestBase
    : public ContentBrowserTest {
 public:
  FileSystemAccessDirectoryHandleImplBrowserTestBase() {
    scoped_features_.InitAndEnableFeature(
        blink::features::kFileSystemAccessLocal);
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(embedded_test_server()->Start());
    test_url_ = embedded_test_server()->GetURL("/title1.html");

    ContentBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Enable experimental web platform features to enable write access.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void TearDown() override {
    ContentBrowserTest::TearDown();
    ASSERT_TRUE(temp_dir_.Delete());
    ui::SelectFileDialog::SetFactory(nullptr);
  }

  // Creates a test directory in the temporary directory. This will also create
  // a corresponding FileSystemAccessDirectoryHandle in the renderer process.
  void CreateTestDirectoryInDirectory(const base::FilePath& directory_path) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath result;
    EXPECT_TRUE(base::CreateTemporaryDirInDir(
        directory_path, FILE_PATH_LITERAL("test"), &result));

    ui::SelectFileDialog::SetFactory(
        std::make_unique<FakeSelectFileDialogFactory>(
            std::vector<base::FilePath>{result}));
    EXPECT_TRUE(NavigateToURL(shell(), test_url_));
    EXPECT_EQ(result.BaseName().AsUTF8Unsafe(),
              EvalJs(shell(),
                     "(async () => {"
                     "  let d = await self.showDirectoryPicker();"
                     "  self.localDir = d;"
                     "  return d.name; })()"));
  }

 protected:
  base::ScopedTempDir temp_dir_;
  GURL test_url_;

 private:
  base::test::ScopedFeatureList scoped_features_;
};

// Test params for `FileSystemAccessDirectoryHandleImplWriteModeBrowserTest`.
struct WriteModeTestParams {
  const char* test_name_suffix;
  bool is_feature_enabled;
  FileSystemAccessPermissionMode expected_mode;
};

// Browser tests for FileSystemAccessDirectoryHandleImpl-related APIs, with a
// mock permission context to verify write permissions-related logic.
class FileSystemAccessDirectoryHandleImplWriteModeBrowserTest
    : public FileSystemAccessDirectoryHandleImplBrowserTestBase,
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

  void SetUpOnMainThread() override {
    FileSystemAccessDirectoryHandleImplBrowserTestBase::SetUpOnMainThread();

    permission_context_ = std::make_unique<TestPermissionContext>();
  }

 protected:
  // Sets the permission context for the File System Access manager.
  // This must be called after the manager has been created.
  // After calling this method, it's users responsibility to set up proper
  // EXPECT_CALL on `permission_context_`.
  void SetPermissionContextForTesting() {
    static_cast<FileSystemAccessManagerImpl*>(
        shell()
            ->web_contents()
            ->GetBrowserContext()
            ->GetStoragePartition(shell()->web_contents()->GetSiteInstance())
            ->GetFileSystemAccessEntryFactory())
        ->SetPermissionContextForTesting(permission_context_.get());
  }

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

  // Configures the mock permission context to return GRANTED for the specified
  // permission mode. This is used to simulate the user granting permission for
  // a particular operation.
  //
  // Due to the use of `StrictMock` for the underlying permission grants, only
  // the `GetStatus()` calls for the grants relevant to `expected_mode` are
  // expected. Any unexpected calls to `GetStatus()` on the other grant (e.g.,
  // `read_grant` when `kWrite` mode is expected) will result in a test
  // failure. This helps capture any unexpected permission status checks.
  void ExpectGetPermissionStatusAndReturnGranted(
      FileSystemAccessPermissionMode expected_mode) {
    switch (expected_mode) {
      case FileSystemAccessPermissionMode::kRead:
        EXPECT_CALL(permission_context_->read_grant(), GetStatus())
            .WillRepeatedly(Return(blink::mojom::PermissionStatus::GRANTED));
        return;
      case FileSystemAccessPermissionMode::kReadWrite:
        EXPECT_CALL(permission_context_->read_grant(), GetStatus())
            .WillRepeatedly(Return(blink::mojom::PermissionStatus::GRANTED));
        EXPECT_CALL(permission_context_->write_grant(), GetStatus())
            .WillRepeatedly(Return(blink::mojom::PermissionStatus::GRANTED));
        return;
      case FileSystemAccessPermissionMode::kWrite:
        EXPECT_CALL(permission_context_->write_grant(), GetStatus())
            .WillRepeatedly(Return(blink::mojom::PermissionStatus::GRANTED));
        return;
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestPermissionContext> permission_context_ = nullptr;
};

// Verifies that `remove()` on a local FS requests the correct
// permission mode depending on whether the `kFileSystemAccessWriteMode`
// feature is enabled.
IN_PROC_BROWSER_TEST_P(FileSystemAccessDirectoryHandleImplWriteModeBrowserTest,
                       Local_Remove_RequestsCorrectPermissions) {
  CreateTestDirectoryInDirectory(temp_dir_.GetPath());
  CreateTestFile("test.txt", "some content");

  SetPermissionContextForTesting();
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

  SetPermissionContextForTesting();
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

  SetPermissionContextForTesting();
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

  SetPermissionContextForTesting();
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
