// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/file_system_access/features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/file_system_chooser_test_helpers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"

namespace content {

class FileSystemAccessFileHandleImplBrowserTest : public ContentBrowserTest {
 public:
  FileSystemAccessFileHandleImplBrowserTest() {
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
  EXPECT_TRUE(base::Contains(result.error, "can not be modified in this way"))
      << result.error;
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
  EXPECT_TRUE(base::Contains(result.error, "can not be modified in this way"))
      << result.error;
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessFileHandleImplBrowserTest, RenameLocal) {
  std::string file_contents = "move me";
  CreateTestFileInDirectory(temp_dir_.GetPath(), file_contents);

  EXPECT_TRUE(ExecJs(shell(),
                     "(async () => {"
                     "return await self.localFile.move('renamed.txt'); })()"));
}

class FileSystemAccessFileHandleGetUniqueIdBrowserTest
    : public FileSystemAccessFileHandleImplBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    FileSystemAccessFileHandleImplBrowserTest::SetUpCommandLine(command_line);
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

  EXPECT_TRUE(EvalJs(shell(),
                     "(async () => {"
                     "return await self.file2.isSameEntry(self.file1); })()")
                  .ExtractBool());
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
