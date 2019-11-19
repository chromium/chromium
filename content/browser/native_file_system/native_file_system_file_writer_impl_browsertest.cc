// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "components/services/quarantine/test_support.h"
#include "content/browser/native_file_system/file_system_chooser_test_helpers.h"
#include "content/browser/native_file_system/native_file_system_manager_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"

namespace content {

// This browser test implements end-to-end tests for
// NativeFileSystemFileWriterImpl.
class NativeFileSystemFileWriterBrowserTest : public ContentBrowserTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kNativeFileSystemAPI);

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
    EXPECT_EQ(int{contents.size()},
              base::WriteFile(test_file, contents.data(), contents.size()));

    ui::SelectFileDialog::SetFactory(
        new FakeSelectFileDialogFactory({test_file}));
    EXPECT_TRUE(NavigateToURL(shell(), test_url_));
    EXPECT_EQ(
        test_file.BaseName().AsUTF8Unsafe(),
        EvalJs(
            shell(),
            "(async () => {"
            "  let e = await self.chooseFileSystemEntries({type: 'openFile'});"
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
    int file_size = static_cast<int>(file_data.size());
    EXPECT_EQ(file_size,
              base::WriteFile(test_file, file_data.c_str(), file_size));

    ui::SelectFileDialog::SetFactory(
        new FakeSelectFileDialogFactory({test_file}));
    EXPECT_TRUE(NavigateToURL(shell(), test_url_));
    EXPECT_EQ(
        test_file.BaseName().AsUTF8Unsafe(),
        EvalJs(
            shell(),
            "(async () => {"
            "  let e = await self.chooseFileSystemEntries({type: 'openFile'});"
            "  self.entry = e;"
            "  self.writers = [];"
            "  return e.name; })()"));

    const base::FilePath swap_file =
        base::FilePath(test_file).AddExtensionASCII(".crswap");
    return std::make_pair(test_file, swap_file);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir temp_dir_;
  GURL test_url_;
};

IN_PROC_BROWSER_TEST_F(NativeFileSystemFileWriterBrowserTest,
                       ContentsWrittenToSwapFileFirst) {
  base::FilePath test_file, swap_file;
  std::tie(test_file, swap_file) = CreateTestFilesAndEntry("");
  const std::string file_contents = "file contents to write";

  EXPECT_EQ(0,
            EvalJs(shell(),
                   JsReplace("(async () => {"
                             "  const w = await self.entry.createWriter();"
                             "  await w.write(0, new Blob([$1]));"
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
  EXPECT_EQ(int{file_contents.size()},
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

IN_PROC_BROWSER_TEST_F(NativeFileSystemFileWriterBrowserTest,
                       KeepExistingDataHasPreviousContent) {
  const std::string initial_contents = "fooks";
  const std::string expected_contents = "barks";
  base::FilePath test_file, swap_file;
  std::tie(test_file, swap_file) = CreateTestFilesAndEntry(initial_contents);

  EXPECT_EQ(nullptr, EvalJs(shell(),
                            "(async () => {"
                            "    const w = await self.entry.createWriter({"
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

  EXPECT_EQ(int{expected_contents.size()},
            EvalJs(shell(),
                   "(async () => {"
                   "  await self.writer.write(0, new Blob(['bar']));"
                   "  await self.writer.close();"
                   "  return (await self.entry.getFile()).size; })()"));
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string read_contents;
    EXPECT_TRUE(base::ReadFileToString(test_file, &read_contents));
    EXPECT_EQ(expected_contents, read_contents);
  }
}

IN_PROC_BROWSER_TEST_F(NativeFileSystemFileWriterBrowserTest,
                       CreateWriterNoKeepExistingWithEmptyFile) {
  const std::string initial_contents = "very long string";
  const std::string expected_contents = "bar";
  base::FilePath test_file, swap_file;
  std::tie(test_file, swap_file) = CreateTestFilesAndEntry(initial_contents);

  EXPECT_EQ(nullptr, EvalJs(shell(),
                            "(async () => {"
                            "  const w = await self.entry.createWriter({"
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

  EXPECT_EQ(int{expected_contents.size()},
            EvalJs(shell(),
                   "(async () => {"
                   "  await self.writer.write(0, new Blob(['bar']));"
                   "  await self.writer.close();"
                   "  return (await self.entry.getFile()).size; })()"));
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string read_contents;
    EXPECT_TRUE(base::ReadFileToString(test_file, &read_contents));
    EXPECT_EQ(expected_contents, read_contents);
  }
}

IN_PROC_BROWSER_TEST_F(NativeFileSystemFileWriterBrowserTest,
                       EachWriterHasUniqueSwapFile) {
  base::FilePath test_file, base_swap_file;
  std::tie(test_file, base_swap_file) = CreateTestFilesAndEntry("");

  int num_writers = 5;
  for (int index = 0; index < num_writers; index++) {
    EXPECT_EQ(nullptr, EvalJs(shell(),
                              "(async () => {"
                              "  const w = await self.entry.createWriter();"
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

IN_PROC_BROWSER_TEST_F(NativeFileSystemFileWriterBrowserTest,
                       EachWriterHasUniqueSwapFileRacy) {
  base::FilePath test_file, base_swap_file;
  std::tie(test_file, base_swap_file) = CreateTestFilesAndEntry("");

  int num_writers = 5;
  for (int index = 0; index < num_writers; index++) {
    EXPECT_EQ(
        nullptr,
        EvalJs(shell(),
               JsReplace("(async () => {"
                         "  for(let i = 0; i < $1; i++ ) {"
                         "    self.writers.push(self.entry.createWriter());"
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

// TODO(https://crbug.com/992089): Files are only quarantined on windows in
// browsertests unfortunately. Change this when more platforms are enabled.
#if defined(OS_WIN)
#define MAYBE_FileAnnotated FileAnnotated
#else
#define MAYBE_FileAnnotated DISABLED_FileAnnotated
#endif  // defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(NativeFileSystemFileWriterBrowserTest,
                       MAYBE_FileAnnotated) {
  base::FilePath test_file, swap_file, lib_file;

  std::tie(test_file, swap_file) = CreateQuarantineTestFilesAndEntry();

  EXPECT_EQ(nullptr, EvalJs(shell(),
                            "(async () => {"
                            "  const w = await self.entry.createWriter("
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

}  // namespace content
