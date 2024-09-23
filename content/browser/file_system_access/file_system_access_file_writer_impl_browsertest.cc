// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "components/services/quarantine/test_support.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/file_system_chooser_test_helpers.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features_generated.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace content {

// This browser test implements end-to-end tests for
// FileSystemAccessFileWriterImpl.
class FileSystemAccessFileWriterBrowserTest : public ContentBrowserTest {
 public:
  FileSystemAccessFileWriterBrowserTest() {
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

  std::pair<base::FilePath, base::FilePath> CreateTestFilesAndEntry(
      const std::string& contents) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath test_file;
    EXPECT_TRUE(
        base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &test_file));
    EXPECT_TRUE(base::WriteFile(test_file, contents));

    ui::SelectFileDialog::SetFactory(
        std::make_unique<FakeSelectFileDialogFactory>(
            std::vector<base::FilePath>{test_file}));
    EXPECT_TRUE(NavigateToURL(shell(), test_url_));
    EXPECT_EQ(test_file.BaseName().AsUTF8Unsafe(),
              EvalJs(shell(),
                     "(async () => {"
                     "  let [e] = await self.showOpenFilePicker();"
                     "  self.entry = e;"
                     "  self.writers = [];"
                     "  return e.name; })()"));

    const base::FilePath swap_file =
        base::FilePath(test_file).AddExtensionASCII(".crswap");
    return std::make_pair(test_file, swap_file);
  }

  std::pair<base::FilePath, base::FilePath>
  CreateQuarantineTestFilesAndEntry() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath test_file =
        temp_dir_.GetPath().AppendASCII("to_be_quarantined.exe");
    std::string file_data = "hello world!";
    EXPECT_TRUE(base::WriteFile(test_file, file_data));

    ui::SelectFileDialog::SetFactory(
        std::make_unique<FakeSelectFileDialogFactory>(
            std::vector<base::FilePath>{test_file}));
    EXPECT_TRUE(NavigateToURL(shell(), test_url_));
    EXPECT_EQ(test_file.BaseName().AsUTF8Unsafe(),
              EvalJs(shell(),
                     "(async () => {"
                     "  let [e] = await self.showOpenFilePicker();"
                     "  self.entry = e;"
                     "  self.writers = [];"
                     "  return e.name; })()"));

    const base::FilePath swap_file =
        base::FilePath(test_file).AddExtensionASCII(".crswap");
    return std::make_pair(test_file, swap_file);
  }

 protected:
  base::ScopedTempDir temp_dir_;
  GURL test_url_;

 private:
  base::test::ScopedFeatureList scoped_features_;
};

IN_PROC_BROWSER_TEST_F(FileSystemAccessFileWriterBrowserTest,
                       ContentsWrittenToSwapFileFirst) {
  auto [test_file, swap_file] = CreateTestFilesAndEntry("");
  const std::string file_contents = "file contents to write";

  EXPECT_EQ(0,
            EvalJs(shell(),
                   JsReplace("(async () => {"
                             "  const w = await self.entry.createWritable();"
                             "  await w.write(new Blob([$1]));"
                             "  self.writer = w;"
                             "  return (await self.entry.getFile()).size; })()",
                             file_contents)));
  {
    // Destination file should be empty, contents written in the swap file.
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string read_contents;
    EXPECT_TRUE(base::ReadFileToString(test_file, &read_contents));
    EXPECT_EQ("", read_contents);
    std::string swap_contents;
    EXPECT_TRUE(base::ReadFileToString(swap_file, &swap_contents));
    EXPECT_EQ(file_contents, swap_contents);
  }

  // Contents now in destination file.
  EXPECT_EQ(static_cast<int>(file_contents.size()),
            EvalJs(shell(),
                   "(async () => {"
                   "  await self.writer.close();"
                   "  return (await self.entry.getFile()).size; })()"));
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string read_contents;
    EXPECT_TRUE(base::ReadFileToString(test_file, &read_contents));
    EXPECT_EQ(file_contents, read_contents);

    EXPECT_FALSE(base::PathExists(swap_file));
  }
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessFileWriterBrowserTest,
                       KeepExistingDataHasPreviousContent) {
  const std::string initial_contents = "fooks";
  const std::string expected_contents = "barks";
  auto [test_file, swap_file] = CreateTestFilesAndEntry(initial_contents);

  EXPECT_EQ(nullptr, EvalJs(shell(),
                            "(async () => {"
                            "    const w = await self.entry.createWritable({"
                            "      keepExistingData: true });"
                            "    self.writer = w;"
                            "})()"));
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::PathExists(swap_file));
    std::string swap_contents;
    EXPECT_TRUE(base::ReadFileToString(swap_file, &swap_contents));
    EXPECT_EQ(initial_contents, swap_contents);
  }

  EXPECT_EQ(static_cast<int>(expected_contents.size()),
            EvalJs(shell(),
                   "(async () => {"
                   "  await self.writer.write(new Blob(['bar']));"
                   "  await self.writer.close();"
                   "  return (await self.entry.getFile()).size; })()"));
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string read_contents;
    EXPECT_TRUE(base::ReadFileToString(test_file, &read_contents));
    EXPECT_EQ(expected_contents, read_contents);
  }
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessFileWriterBrowserTest,
                       CreateWriterNoKeepExistingWithEmptyFile) {
  const std::string initial_contents = "very long string";
  const std::string expected_contents = "bar";
  auto [test_file, swap_file] = CreateTestFilesAndEntry(initial_contents);

  EXPECT_EQ(nullptr, EvalJs(shell(),
                            "(async () => {"
                            "  const w = await self.entry.createWritable({"
                            "    keepExistingData: false });"
                            "  self.writer = w;"
                            "})()"));
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::PathExists(swap_file));
    std::string swap_contents;
    EXPECT_TRUE(base::ReadFileToString(swap_file, &swap_contents));
    EXPECT_EQ("", swap_contents);
  }

  EXPECT_EQ(static_cast<int>(expected_contents.size()),
            EvalJs(shell(),
                   "(async () => {"
                   "  await self.writer.write(new Blob(['bar']));"
                   "  await self.writer.close();"
                   "  return (await self.entry.getFile()).size; })()"));
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string read_contents;
    EXPECT_TRUE(base::ReadFileToString(test_file, &read_contents));
    EXPECT_EQ(expected_contents, read_contents);
  }
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessFileWriterBrowserTest,
                       EachWriterHasUniqueSwapFile) {
  auto [test_file, base_swap_file] = CreateTestFilesAndEntry("");

  int num_writers = 5;
  for (int index = 0; index < num_writers; index++) {
    EXPECT_EQ(nullptr, EvalJs(shell(),
                              "(async () => {"
                              "  const w = await self.entry.createWritable();"
                              "  self.writers.push(w);"
                              "})()"));
  }

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    for (int index = 0; index < num_writers; index++) {
      base::FilePath swap_file = base_swap_file;
      if (index != 0) {
        swap_file = base::FilePath(test_file).AddExtensionASCII(
            base::StringPrintf(".%d.crswap", index));
      }
      EXPECT_TRUE(base::PathExists(swap_file));
    }
  }
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessFileWriterBrowserTest,
                       EachWriterHasUniqueSwapFileRacy) {
  auto [test_file, base_swap_file] = CreateTestFilesAndEntry("");

  int num_writers = 5;
  for (int index = 0; index < num_writers; index++) {
    EXPECT_EQ(
        nullptr,
        EvalJs(shell(),
               JsReplace("(async () => {"
                         "  for(let i = 0; i < $1; i++ ) {"
                         "    self.writers.push(self.entry.createWritable());"
                         "  }"
                         "  await Promise.all(self.writers);"
                         "})()",
                         num_writers)));
  }

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    for (int index = 0; index < num_writers; index++) {
      base::FilePath swap_file = base_swap_file;
      if (index != 0) {
        swap_file = base::FilePath(test_file).AddExtensionASCII(
            base::StringPrintf(".%d.crswap", index));
      }
      EXPECT_TRUE(base::PathExists(swap_file));
    }
  }
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessFileWriterBrowserTest,
                       EachWriterHasUniqueSwapFileRacyKeepExistingData) {
  auto [test_file, base_swap_file] = CreateTestFilesAndEntry("");

  int num_writers = 5;
  for (int index = 0; index < num_writers; index++) {
    EXPECT_EQ(
        nullptr,
        EvalJs(shell(), JsReplace("(async () => {"
                                  "  for(let i = 0; i < $1; i++ ) {"
                                  "self.writers.push(self.entry.createWritable("
                                  "{keepExistingData: true}));"
                                  "  }"
                                  "  await Promise.all(self.writers);"
                                  "})()",
                                  num_writers)));
  }

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    for (int index = 0; index < num_writers; index++) {
      base::FilePath swap_file = base_swap_file;
      if (index != 0) {
        swap_file = base::FilePath(test_file).AddExtensionASCII(
            base::StringPrintf(".%d.crswap", index));
      }
      EXPECT_TRUE(base::PathExists(swap_file));
    }
  }
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessFileWriterBrowserTest,
                       WriteOffsetAndSeekInSameWritable) {
  // Performing a second write operation with a valid offset, after performing
  // a seek operation occurs at the expected index (https://crbug.com/1427819).
  auto [test_file, base_swap_file] = CreateTestFilesAndEntry("");
  EXPECT_EQ("abc1234h",
            EvalJs(shell(),
                   "(async () => {"
                   "const w = await self.entry.createWritable();"
                   "await w.write('abcdefgh');"
                   "await w.seek(0);"
                   "await w.write({type:'write',data:'123',position:3});"
                   "await w.write('4');"
                   "await w.close();"
                   "return (await (await self.entry.getFile()).text());"
                   "})()"));
}

// Ideally this would be tested by WPTs, but the location of the swap file is
// not specified and not easily accessible.
IN_PROC_BROWSER_TEST_F(FileSystemAccessFileWriterBrowserTest,
                       CannotCreateWritableToSwapFile) {
  {
    base::FilePath test_dir;
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::CreateTemporaryDirInDir(
        temp_dir_.GetPath(), FILE_PATH_LITERAL("parent"), &test_dir));

    ui::SelectFileDialog::SetFactory(
        std::make_unique<FakeSelectFileDialogFactory>(
            std::vector<base::FilePath>{test_dir}));
    EXPECT_TRUE(NavigateToURL(shell(), test_url_));
    EXPECT_EQ(test_dir.BaseName().AsUTF8Unsafe(),
              EvalJs(shell(),
                     "(async () => {"
                     "  let dir = await self.showDirectoryPicker();"
                     "  self.parent = dir;"
                     "  return dir.name; })()"));
  }

  // Unsuccessfully attempt to create a writer to the swap file, which is
  // locked.
  auto result = EvalJs(
      shell(),
      "(async () => {"
      "const file = await self.parent.getFileHandle('file.txt', {create:true});"
      "self.writer = await file.createWritable();"
      "await self.writer.write('abcdefgh');"
      "self.swapFile = await self.parent.getFileHandle('file.txt.crswap', "
      "{create:false});"
      "return (await self.swapFile.createWritable());"
      "})()");
  EXPECT_TRUE(base::Contains(result.error, "modifications are not allowed."))
      << result.error;

  auto close_result = EvalJs(shell(),
                             "(async () => {"
                             "await self.writer.close();"
                             "})()");
  EXPECT_TRUE(close_result.error.empty()) << close_result.error;
}

// TODO(crbug.com/40639570): Files are only quarantined on windows in
// browsertests unfortunately. Change this when more platforms are enabled.
#if BUILDFLAG(IS_WIN)
#define MAYBE_FileAnnotated FileAnnotated
#else
#define MAYBE_FileAnnotated DISABLED_FileAnnotated
#endif  // BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(FileSystemAccessFileWriterBrowserTest,
                       MAYBE_FileAnnotated) {
  auto [test_file, swap_file] = CreateQuarantineTestFilesAndEntry();

  EXPECT_EQ(nullptr, EvalJs(shell(),
                            "(async () => {"
                            "  const w = await self.entry.createWritable("
                            "    {keepExistingData: true},"
                            "  );"
                            "  self.writer = w;"
                            "  await self.writer.close();"
                            "})()"));

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_FALSE(base::PathExists(swap_file));
    EXPECT_TRUE(quarantine::IsFileQuarantined(test_file, GURL(), test_url_));
  }
}

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(FileSystemAccessFileWriterBrowserTest,
                       RespectOSPermissions) {
  auto [test_file, swap_file] = CreateTestFilesAndEntry("");

  // Make the file read-only.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
#if BUILDFLAG(IS_POSIX)
    int mode = 0444;
    EXPECT_TRUE(base::SetPosixFilePermissions(test_file, mode));
#elif BUILDFLAG(IS_WIN)
    DWORD attributes = ::GetFileAttributes(test_file.value().c_str());
    ASSERT_NE(attributes, INVALID_FILE_ATTRIBUTES);
    attributes |= FILE_ATTRIBUTE_READONLY;
    EXPECT_TRUE(::SetFileAttributes(test_file.value().c_str(), attributes));
#endif  // BUILDFLAG(IS_POSIX)
  }

  auto result = EvalJs(shell(),
                       "(async () => {"
                       "  return (await self.entry.createWritable()); })()");
  EXPECT_TRUE(base::Contains(result.error, "Cannot write to a read-only file."))
      << result.error;
}
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_WIN)

}  // namespace content
