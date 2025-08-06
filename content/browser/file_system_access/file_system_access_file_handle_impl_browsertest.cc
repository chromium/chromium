// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/file_system_access/features.h"
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
using testing::Eq;
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

class FileSystemAccessFileHandleImplBrowserTestBase
    : public ContentBrowserTest {
 public:
  FileSystemAccessFileHandleImplBrowserTestBase() {
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

  void CreateTestFileInDirectory(const base::FilePath& directory_path,
                                 const std::string& contents) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath result;
    EXPECT_TRUE(base::CreateTemporaryFileInDir(directory_path, &result));
    EXPECT_TRUE(base::WriteFile(result, contents));

    ui::SelectFileDialog::SetFactory(
        std::make_unique<FakeSelectFileDialogFactory>(
            std::vector<base::FilePath>{result}));
    EXPECT_TRUE(NavigateToURL(shell(), test_url_));
    EXPECT_EQ(result.BaseName().AsUTF8Unsafe(),
              EvalJs(shell(),
                     "(async () => {"
                     "  let [e] = await self.showOpenFilePicker();"
                     "  self.localFile = e;"
                     "  return e.name; })()"));
  }

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

class FileSystemAccessFileHandleImplBrowserTest
    : public FileSystemAccessFileHandleImplBrowserTestBase {};

// TODO(crbug.com/40888337): Make this a WPT once crbug.com/1114920 is fixed.
IN_PROC_BROWSER_TEST_F(FileSystemAccessFileHandleImplBrowserTest,
                       MoveLocalToSandboxed) {
  std::string file_contents = "move me to a sandboxed file system";
  CreateTestFileInDirectory(temp_dir_.GetPath(), file_contents);

  auto result =
      EvalJs(shell(),
             "(async () => {"
             "const sandboxRoot = await navigator.storage.getDirectory();"
             "return await self.localFile.move(sandboxRoot); })()");
  EXPECT_THAT(result, EvalJsResult::ErrorIs(testing::HasSubstr(
                          "can not be modified in this way")))
      << result;
}

// TODO(crbug.com/40888337): Make this a WPT once crbug.com/1114920 is fixed.
IN_PROC_BROWSER_TEST_F(FileSystemAccessFileHandleImplBrowserTest,
                       MoveSandboxedToLocal) {
  CreateTestDirectoryInDirectory(temp_dir_.GetPath());

  auto result =
      EvalJs(shell(),
             "(async () => {"
             "const sandboxRoot = await navigator.storage.getDirectory();"
             "const sandboxFile = await sandboxRoot.getFileHandle('file.txt',"
             "  { create: true });"
             "const writable = await sandboxFile.createWritable();"
             "await writable.write('move me to the local file system');"
             "await writable.close();"
             "return await sandboxFile.move(localDir); })()");
  EXPECT_THAT(result, EvalJsResult::ErrorIs(testing::HasSubstr(
                          "can not be modified in this way")))
      << result;
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessFileHandleImplBrowserTest, RenameLocal) {
  std::string file_contents = "move me";
  CreateTestFileInDirectory(temp_dir_.GetPath(), file_contents);

  EXPECT_TRUE(ExecJs(shell(),
                     "(async () => {"
                     "return await self.localFile.move('renamed.txt'); })()"));
}

// Test params for `FileSystemAccessFileHandleImplWriteModeBrowserTest`.
struct WriteModeTestParams {
  const char* test_name_suffix;
  bool is_feature_enabled;
  FileSystemAccessPermissionMode expected_mode;
};

// Browser tests for FileSystemAccessFileHandleImpl-related APIs, with a mock
// permission context to verify write permissions-related logic.
class FileSystemAccessFileHandleImplWriteModeBrowserTest
    : public FileSystemAccessFileHandleImplBrowserTestBase,
      public testing::WithParamInterface<WriteModeTestParams> {
 public:
  FileSystemAccessFileHandleImplWriteModeBrowserTest() {
    if (GetParam().is_feature_enabled) {
      scoped_feature_list_.InitAndEnableFeature(
          blink::features::kFileSystemAccessWriteMode);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          blink::features::kFileSystemAccessWriteMode);
    }
  }

  void SetUpOnMainThread() override {
    FileSystemAccessFileHandleImplBrowserTestBase::SetUpOnMainThread();

    permission_context_ = std::make_unique<TestPermissionContext>();
    static_cast<FileSystemAccessManagerImpl*>(
        shell()
            ->web_contents()
            ->GetBrowserContext()
            ->GetStoragePartition(shell()->web_contents()->GetSiteInstance())
            ->GetFileSystemAccessEntryFactory())
        ->SetPermissionContextForTesting(permission_context_.get());
  }

 protected:
  // Configures the mock permission context to return GRANTED for the
  // specified permission mode. This is used to simulate the user granting
  // permission for a particular operation.
  //
  // Due to the use of `StrictMock` for the underlying permission grants, only
  // the `GetStatus()` calls for the grants relevant to `expected_mode` are
  // expected. Any unexpected calls to `GetStatus()` on the other grant (e.g.,
  // `read_grant` when `kWrite` mode is expected) will result in a test failure.
  // This helps capture any unexpected permission status checks.
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

// Verifies that `createWritable()` on a local FS requests the correct
// permission mode depending on whether the `kFileSystemAccessWriteMode` feature
// is enabled.
IN_PROC_BROWSER_TEST_P(FileSystemAccessFileHandleImplWriteModeBrowserTest,
                       Local_CreateFileWriter_RequestsCorrectPermissions) {
  CreateTestFileInDirectory(temp_dir_.GetPath(), "test file");

  ExpectGetPermissionStatusAndReturnGranted(GetParam().expected_mode);

  EXPECT_TRUE(ExecJs(shell(),
                     "(async () => {"
                     "return await self.localFile.createWritable(); })()"));
}

// Verifies that `createWritable()` on a sandboxed FS requests the correct
// permission mode depending on whether the `kFileSystemAccessWriteMode`
// feature is enabled.
IN_PROC_BROWSER_TEST_P(FileSystemAccessFileHandleImplWriteModeBrowserTest,
                       Sandboxed_CreateFileWriter_RequestsCorrectPermissions) {
  EXPECT_TRUE(NavigateToURL(shell(), test_url_));

  ExpectGetPermissionStatusAndReturnGranted(GetParam().expected_mode);

  EXPECT_TRUE(ExecJs(shell(),
                     "(async () => {"
                     "  const root = await navigator.storage.getDirectory();"
                     "  const file = await root.getFileHandle('test.txt', "
                     "                                        {create: true});"
                     "  return await file.createWritable();"
                     "})()"));
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    FileSystemAccessFileHandleImplWriteModeBrowserTest,
    testing::Values(
        WriteModeTestParams{"WriteModeDisabled", false,
                            FileSystemAccessPermissionMode::kReadWrite},
        WriteModeTestParams{"WriteModeEnabled", true,
                            FileSystemAccessPermissionMode::kWrite}),
    [](const testing::TestParamInfo<WriteModeTestParams>& info) {
      return info.param.test_name_suffix;
    });

class FileSystemAccessFileHandleGetUniqueIdBrowserTest
    : public FileSystemAccessFileHandleImplBrowserTestBase {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    FileSystemAccessFileHandleImplBrowserTestBase::SetUpCommandLine(
        command_line);
    // Enable File System Access experimental features, which includes the
    // getUniqueId() method.
    command_line->AppendSwitch(
        "--enable-blink-features=FileSystemAccessAPIExperimental");
  }
};

IN_PROC_BROWSER_TEST_F(FileSystemAccessFileHandleGetUniqueIdBrowserTest,
                       SameFileFromDifferentPickerInvocations) {
  base::FilePath file_path;
  std::string file_contents = "I am unique";
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(
        base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &file_path));
    EXPECT_TRUE(base::WriteFile(file_path, file_contents));
  }

  ui::SelectFileDialog::SetFactory(
      std::make_unique<FakeSelectFileDialogFactory>(
          std::vector<base::FilePath>{file_path}));
  EXPECT_TRUE(NavigateToURL(shell(), test_url_));
  EXPECT_EQ(file_path.BaseName().AsUTF8Unsafe(),
            EvalJs(shell(),
                   "(async () => {"
                   "  let [e] = await self.showOpenFilePicker();"
                   "  self.file1 = e;"
                   "  return e.name; })()"));
  EXPECT_EQ(file_path.BaseName().AsUTF8Unsafe(),
            EvalJs(shell(),
                   "(async () => {"
                   "  let [e] = await self.showOpenFilePicker();"
                   "  self.file2 = e;"
                   "  return e.name; })()"));

  EXPECT_EQ(true,
            EvalJs(shell(),
                   "(async () => {"
                   "return await self.file2.isSameEntry(self.file1); })()"));
  auto uniqueId1 = EvalJs(shell(),
                          "(async () => {"
                          "return await self.file1.getUniqueId(); })()")
                       .ExtractString();
  auto uniqueId2 = EvalJs(shell(),
                          "(async () => {"
                          "return await self.file2.getUniqueId(); })()")
                       .ExtractString();
  EXPECT_EQ(uniqueId1, uniqueId2);
}

}  // namespace content
