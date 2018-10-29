// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/fileapi/file_system_chooser_test_helpers.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "third_party/blink/public/common/features.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"

namespace content {

// This browser test implements end-to-end tests for the chooseFileSystemEntry
// API.
class FileSystemChooserBrowserTest : public ContentBrowserTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kWritableFilesAPI);
    ContentBrowserTest::SetUp();
  }

  void TearDown() override {
    ContentBrowserTest::TearDown();
    ASSERT_TRUE(temp_dir_.Delete());
    ui::SelectFileDialog::SetFactory(nullptr);
  }

  base::FilePath CreateTestFile(const std::string& contents) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath result;
    EXPECT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &result));
    EXPECT_EQ(int{contents.size()},
              base::WriteFile(result, contents.data(), contents.size()));
    return result;
  }

  base::FilePath CreateTestDir() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath result;
    EXPECT_TRUE(base::CreateTemporaryDirInDir(
        temp_dir_.GetPath(), FILE_PATH_LITERAL("test"), &result));
    return result;
  }

 protected:
  const std::string kBlankHtml = "<!DOCTYPE html><html><body>";
  const GURL kTestUrl = GURL("https://foobar.com/");

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir temp_dir_;
};

IN_PROC_BROWSER_TEST_F(FileSystemChooserBrowserTest, CancelDialog) {
  ui::SelectFileDialog::SetFactory(new CancellingSelectFileDialogFactory);
  LoadDataWithBaseURL(shell(), kTestUrl, kBlankHtml, kTestUrl);
  auto result = EvalJs(shell(), "self.chooseFileSystemEntries()");
  EXPECT_TRUE(result.error.find("AbortError") != std::string::npos)
      << result.error;
}

IN_PROC_BROWSER_TEST_F(FileSystemChooserBrowserTest, OpenFile) {
  const std::string file_contents = "hello world!";
  const base::FilePath test_file = CreateTestFile(file_contents);
  SelectFileDialogParams dialog_params;
  ui::SelectFileDialog::SetFactory(
      new FakeSelectFileDialogFactory({test_file}, &dialog_params));
  LoadDataWithBaseURL(shell(), kTestUrl, kBlankHtml, kTestUrl);
  EXPECT_EQ(test_file.BaseName().AsUTF8Unsafe(),
            EvalJs(shell(),
                   "(async () => {"
                   "  let e = await self.chooseFileSystemEntries();"
                   "  self.selected_entry = e;"
                   "  return e.name; })()"));
  EXPECT_EQ(ui::SelectFileDialog::SELECT_OPEN_FILE, dialog_params.type);
  EXPECT_EQ(
      file_contents,
      EvalJs(shell(),
             "(async () => { const file = await self.selected_entry.getFile(); "
             "return await new Response(file).text(); })()"));
}

IN_PROC_BROWSER_TEST_F(FileSystemChooserBrowserTest, SaveFile) {
  const std::string file_contents = "file contents to write";
  const base::FilePath test_file = CreateTestFile("");
  {
    // Delete file, since SaveFile should be able to deal with non-existing
    // files.
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::DeleteFile(test_file, false));
  }
  SelectFileDialogParams dialog_params;
  ui::SelectFileDialog::SetFactory(
      new FakeSelectFileDialogFactory({test_file}, &dialog_params));
  LoadDataWithBaseURL(shell(), kTestUrl, kBlankHtml, kTestUrl);
  EXPECT_EQ(test_file.BaseName().AsUTF8Unsafe(),
            EvalJs(shell(),
                   "(async () => {"
                   "  let e = await self.chooseFileSystemEntries("
                   "      {type: 'saveFile'});"
                   "  self.entry = e;"
                   "  return e.name; })()"));
  EXPECT_EQ(ui::SelectFileDialog::SELECT_SAVEAS_FILE, dialog_params.type);
  EXPECT_EQ(int{file_contents.size()},
            EvalJs(shell(),
                   JsReplace("(async () => {"
                             "  const w = await self.entry.createWriter();"
                             "  await w.write(0, new Blob([$1]));"
                             "  return (await self.entry.getFile()).size; })()",
                             file_contents)));
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string read_contents;
    EXPECT_TRUE(base::ReadFileToString(test_file, &read_contents));
    EXPECT_EQ(file_contents, read_contents);
  }
}

IN_PROC_BROWSER_TEST_F(FileSystemChooserBrowserTest, OpenMultipleFiles) {
  const base::FilePath test_file1 = CreateTestFile("file1");
  const base::FilePath test_file2 = CreateTestFile("file2");
  SelectFileDialogParams dialog_params;
  ui::SelectFileDialog::SetFactory(new FakeSelectFileDialogFactory(
      {test_file1, test_file2}, &dialog_params));
  LoadDataWithBaseURL(shell(), kTestUrl, kBlankHtml, kTestUrl);
  EXPECT_EQ(ListValueOf(test_file1.BaseName().AsUTF8Unsafe(),
                        test_file2.BaseName().AsUTF8Unsafe()),
            EvalJs(shell(),
                   "(async () => {"
                   "  let e = await self.chooseFileSystemEntries("
                   "      {multiple: true});"
                   "  return e.map(x => x.name); })()"));
  EXPECT_EQ(ui::SelectFileDialog::SELECT_OPEN_MULTI_FILE, dialog_params.type);
}

IN_PROC_BROWSER_TEST_F(FileSystemChooserBrowserTest, OpenDirectory) {
  base::FilePath test_dir = CreateTestDir();
  SelectFileDialogParams dialog_params;
  ui::SelectFileDialog::SetFactory(
      new FakeSelectFileDialogFactory({test_dir}, &dialog_params));
  LoadDataWithBaseURL(shell(), kTestUrl, kBlankHtml, kTestUrl);
  EXPECT_EQ(test_dir.BaseName().AsUTF8Unsafe(),
            EvalJs(shell(),
                   "(async () => {"
                   "  let e = await self.chooseFileSystemEntries("
                   "      {type: 'openDirectory'});"
                   "  self.selected_entry = e;"
                   "  return e.name; })()"));
  EXPECT_EQ(ui::SelectFileDialog::SELECT_FOLDER, dialog_params.type);
}

IN_PROC_BROWSER_TEST_F(FileSystemChooserBrowserTest, AcceptsOptions) {
  SelectFileDialogParams dialog_params;
  ui::SelectFileDialog::SetFactory(
      new CancellingSelectFileDialogFactory(&dialog_params));
  LoadDataWithBaseURL(shell(), kTestUrl, kBlankHtml, kTestUrl);
  auto result = EvalJs(shell(),
                       "self.chooseFileSystemEntries({accepts: ["
                       "  {description: 'no-extensions'},"
                       "  {description: 'foo', extensions: ['txt', 'Js']},"
                       "  {mimeTypes: ['image/jpeg']}"
                       "]})");
  EXPECT_TRUE(result.error.find("AbortError") != std::string::npos)
      << result.error;

  ASSERT_TRUE(dialog_params.file_types);
  EXPECT_TRUE(dialog_params.file_types->include_all_files);
  ASSERT_EQ(2u, dialog_params.file_types->extensions.size());
  ASSERT_EQ(2u, dialog_params.file_types->extensions[0].size());
  EXPECT_EQ(FILE_PATH_LITERAL("Js"),
            dialog_params.file_types->extensions[0][0]);
  EXPECT_EQ(FILE_PATH_LITERAL("txt"),
            dialog_params.file_types->extensions[0][1]);
  EXPECT_TRUE(base::ContainsValue(dialog_params.file_types->extensions[1],
                                  FILE_PATH_LITERAL("jpg")));
  EXPECT_TRUE(base::ContainsValue(dialog_params.file_types->extensions[1],
                                  FILE_PATH_LITERAL("jpeg")));

  ASSERT_EQ(2u,
            dialog_params.file_types->extension_description_overrides.size());
  EXPECT_EQ(base::ASCIIToUTF16("foo"),
            dialog_params.file_types->extension_description_overrides[0]);
  EXPECT_EQ(base::ASCIIToUTF16(""),
            dialog_params.file_types->extension_description_overrides[1]);
}

}  // namespace content