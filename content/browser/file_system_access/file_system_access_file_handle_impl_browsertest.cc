// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
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
using testing::Eq;
using testing::Return;

}  // namespace

class FileSystemAccessFileHandleImplBrowserTest
    : public FileSystemAccessHandleImplBrowserTestBase {};

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
    : public FileSystemAccessHandleImplPermissionBrowserTestBase,
      public testing::WithParamInterface<WriteModeTestParams> {
 public:
  FileSystemAccessFileHandleImplWriteModeBrowserTest() {
    scoped_feature_list_.InitWithFeatureState(
        blink::features::kFileSystemAccessWriteMode,
        GetParam().is_feature_enabled);
  }

 protected:
  // Creates a test file and a test directory in the same JS context to avoid
  // navigation-related errors.
  void CreateTestFileAndDirectory(const base::FilePath& directory_path,
                                  const std::string& file_contents) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath file_path;
    EXPECT_TRUE(base::CreateTemporaryFileInDir(directory_path, &file_path));
    EXPECT_TRUE(base::WriteFile(file_path, file_contents));

    base::FilePath dir_path;
    EXPECT_TRUE(base::CreateTemporaryDirInDir(
        directory_path, FILE_PATH_LITERAL("test"), &dir_path));

    // Set up file picker to select the file.
    ui::SelectFileDialog::SetFactory(
        std::make_unique<FakeSelectFileDialogFactory>(
            std::vector<base::FilePath>{file_path}));
    EXPECT_TRUE(NavigateToURL(shell(), test_url_));
    EXPECT_EQ(file_path.BaseName().AsUTF8Unsafe(),
              EvalJs(shell(),
                     "(async () => {"
                     "  let [e] = await self.showOpenFilePicker();"
                     "  self.localFile = e;"
                     "  return e.name; })()"));

    // Set up directory picker to select the directory.
    ui::SelectFileDialog::SetFactory(
        std::make_unique<FakeSelectFileDialogFactory>(
            std::vector<base::FilePath>{dir_path}));
    EXPECT_EQ(dir_path.BaseName().AsUTF8Unsafe(),
              EvalJs(shell(),
                     "(async () => {"
                     "  let d = await self.showDirectoryPicker();"
                     "  self.localDir = d;"
                     "  return d.name; })()"));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
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
  ASSERT_TRUE(NavigateToURL(shell(), test_url_));
  ASSERT_TRUE(ExecJs(
      shell(),
      "(async () => {"
      "  self.sandboxDir = await navigator.storage.getDirectory();"
      "  self.fileHandle = await self.sandboxDir.getFileHandle('test.txt', "
      "                                        {create: true});"
      "})()"));

  ExpectGetPermissionStatusAndReturnGranted(GetParam().expected_mode);

  EXPECT_TRUE(ExecJs(shell(),
                     "(async () => {"
                     "  return await self.fileHandle.createWritable();"
                     "})()"));
}

// Verifies that `remove()` on a local FS requests the correct
// permission mode depending on whether the `kFileSystemAccessWriteMode`
// feature is enabled.
IN_PROC_BROWSER_TEST_P(FileSystemAccessFileHandleImplWriteModeBrowserTest,
                       Local_Remove_RequestsCorrectPermissions) {
  CreateTestFileInDirectory(temp_dir_.GetPath(), "test file");

  ExpectGetPermissionStatusAndReturnGranted(GetParam().expected_mode);

  EXPECT_TRUE(
      ExecJs(shell(), "(async () => { await self.localFile.remove(); })()"));
}

// Verifies that `remove()` on a sandboxed FS requests the correct
// permission mode depending on whether the `kFileSystemAccessWriteMode`
// feature is enabled.
IN_PROC_BROWSER_TEST_P(FileSystemAccessFileHandleImplWriteModeBrowserTest,
                       Sandboxed_Remove_RequestsCorrectPermissions) {
  ASSERT_TRUE(NavigateToURL(shell(), test_url_));
  ASSERT_TRUE(ExecJs(shell(), R"((async () => {
      self.sandboxDir = await navigator.storage.getDirectory();
      self.fileHandle = await self.sandboxDir.getFileHandle(
          'test.txt', {create: true});
      const writable = await self.fileHandle.createWritable();
      await writable.write('some content');
      await writable.close();
    })())"));

  ExpectGetPermissionStatusAndReturnGranted(GetParam().expected_mode);

  EXPECT_TRUE(ExecJs(shell(),
                     "(async () => {"
                     "  await self.fileHandle.remove();"
                     "})()"));
}

// Verifies that `move()` on a local FS requests the correct permission mode
// depending on whether the `kFileSystemAccessWriteMode` feature is enabled.
IN_PROC_BROWSER_TEST_P(FileSystemAccessFileHandleImplWriteModeBrowserTest,
                       Local_Move_RequestsCorrectPermissions) {
  CreateTestFileAndDirectory(temp_dir_.GetPath(), "test file");

  // Calling the above setup method creates two shared handle states.
  ExpectGetPermissionStatusAndReturnGranted(
      GetParam().expected_mode,
      /*expected_shared_handle_state_count=*/2u);

  EXPECT_TRUE(ExecJs(shell(), R"((async () => {
      await self.localFile.move(self.localDir);
  })())"));
}

// Verifies that `move()` on a sandboxed FS requests the correct permission
// mode depending on whether the `kFileSystemAccessWriteMode` feature is
// enabled.
IN_PROC_BROWSER_TEST_P(FileSystemAccessFileHandleImplWriteModeBrowserTest,
                       Sandboxed_Move_RequestsCorrectPermissions) {
  ASSERT_TRUE(NavigateToURL(shell(), test_url_));
  ASSERT_TRUE(ExecJs(shell(), R"((async () => {
      self.sandboxDir = await navigator.storage.getDirectory();
      self.fileHandle = await self.sandboxDir.getFileHandle(
          'test.txt', {create: true});
      const writable = await self.fileHandle.createWritable();
      await writable.write('some content');
      await writable.close();
    })())"));

  ExpectGetPermissionStatusAndReturnGranted(GetParam().expected_mode);

  EXPECT_TRUE(ExecJs(shell(), R"((async () => {
      await self.fileHandle.move(self.sandboxDir);
  })())"));
}

// Verifies that `rename()` on a local FS requests the correct permission mode
// depending on whether the `kFileSystemAccessWriteMode` feature is enabled.
IN_PROC_BROWSER_TEST_P(FileSystemAccessFileHandleImplWriteModeBrowserTest,
                       Local_Rename_RequestsCorrectPermissions) {
  CreateTestFileInDirectory(temp_dir_.GetPath(), "test file");

  ExpectGetPermissionStatusAndReturnGranted(GetParam().expected_mode);

  EXPECT_TRUE(ExecJs(shell(), R"((async () => {
      await self.localFile.move('test2.txt');
  })())"));
}

// Verifies that `rename()` on a sandboxed FS requests the correct permission
// mode depending on whether the `kFileSystemAccessWriteMode` feature is
// enabled.
IN_PROC_BROWSER_TEST_P(FileSystemAccessFileHandleImplWriteModeBrowserTest,
                       Sandboxed_Rename_RequestsCorrectPermissions) {
  ASSERT_TRUE(NavigateToURL(shell(), test_url_));
  ASSERT_TRUE(ExecJs(shell(), R"((async () => {
      self.sandboxDir = await navigator.storage.getDirectory();
      self.fileHandle = await self.sandboxDir.getFileHandle(
          'test.txt', {create: true});
      const writable = await self.fileHandle.createWritable();
      await writable.write('some content');
      await writable.close();
    })())"));

  ExpectGetPermissionStatusAndReturnGranted(GetParam().expected_mode);

  EXPECT_TRUE(ExecJs(shell(), R"((async () => {
      await self.fileHandle.move('test2.txt');
  })())"));
}

INSTANTIATE_TEST_SUITE_P(
    All,
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
    : public FileSystemAccessHandleImplBrowserTestBase {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    FileSystemAccessHandleImplBrowserTestBase::SetUpCommandLine(command_line);
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
