// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/native_file_system/file_system_chooser_test_helpers.h"
#include "content/browser/native_file_system/mock_native_file_system_permission_context.h"
#include "content/browser/native_file_system/native_file_system_manager_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"

namespace content {

using base::test::RunOnceCallback;
using blink::mojom::PermissionStatus;
using SensitiveDirectoryResult =
    NativeFileSystemPermissionContext::SensitiveDirectoryResult;

// This browser test implements end-to-end tests for the chooseFileSystemEntry
// API.
class FileSystemChooserBrowserTest : public ContentBrowserTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kNativeFileSystemAPI);

    ASSERT_TRUE(embedded_test_server()->Start());

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

  bool IsFullscreen() {
    WebContents* web_contents = shell()->web_contents();
    return web_contents->IsFullscreenForCurrentTab();
  }

  void EnterFullscreen(GURL url) {
    WebContentsImpl* web_contents_impl =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    web_contents_impl->EnterFullscreenMode(url,
                                           blink::mojom::FullscreenOptions());
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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir temp_dir_;
};

IN_PROC_BROWSER_TEST_F(FileSystemChooserBrowserTest, CancelDialog) {
  ui::SelectFileDialog::SetFactory(new CancellingSelectFileDialogFactory);
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  auto result = EvalJs(shell(), "self.chooseFileSystemEntries()");
  EXPECT_TRUE(result.error.find("aborted") != std::string::npos)
      << result.error;
}

IN_PROC_BROWSER_TEST_F(FileSystemChooserBrowserTest, OpenFile) {
  const std::string file_contents = "hello world!";
  const base::FilePath test_file = CreateTestFile(file_contents);
  SelectFileDialogParams dialog_params;
  ui::SelectFileDialog::SetFactory(
      new FakeSelectFileDialogFactory({test_file}, &dialog_params));
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  EXPECT_EQ(test_file.BaseName().AsUTF8Unsafe(),
            EvalJs(shell(),
                   "(async () => {"
                   "  let e = await self.chooseFileSystemEntries();"
                   "  self.selected_entry = e;"
                   "  return e.name; })()"));
  EXPECT_EQ(ui::SelectFileDialog::SELECT_OPEN_FILE, dialog_params.type);
  EXPECT_EQ(shell()->web_contents()->GetTopLevelNativeWindow(),
            dialog_params.owning_window);
  EXPECT_EQ(
      file_contents,
      EvalJs(shell(),
             "(async () => { const file = await self.selected_entry.getFile(); "
             "return await file.text(); })()"));
}

IN_PROC_BROWSER_TEST_F(FileSystemChooserBrowserTest, FullscreenOpenFile) {
  const std::string file_contents = "hello world!";
  const base::FilePath test_file = CreateTestFile(file_contents);
  SelectFileDialogParams dialog_params;
  ui::SelectFileDialog::SetFactory(
      new FakeSelectFileDialogFactory({test_file}, &dialog_params));
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  EnterFullscreen(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(IsFullscreen());
  EXPECT_EQ(test_file.BaseName().AsUTF8Unsafe(),
            EvalJs(shell(),
                   "(async () => {"
                   "  let e = await self.chooseFileSystemEntries();"
                   "  self.selected_entry = e;"
                   "  return e.name; })()"));
  EXPECT_FALSE(IsFullscreen());
}

IN_PROC_BROWSER_TEST_F(FileSystemChooserBrowserTest, SaveFile_NonExistingFile) {
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
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
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
                             "  await w.close();"
                             "  return (await self.entry.getFile()).size; })()",
                             file_contents)));
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string read_contents;
    EXPECT_TRUE(base::ReadFileToString(test_file, &read_contents));
    EXPECT_EQ(file_contents, read_contents);
  }
}

IN_PROC_BROWSER_TEST_F(FileSystemChooserBrowserTest,
                       SaveFile_TruncatesExistingFile) {
  const base::FilePath test_file = CreateTestFile("Hello World");

  SelectFileDialogParams dialog_params;
  ui::SelectFileDialog::SetFactory(
      new FakeSelectFileDialogFactory({test_file}, &dialog_params));
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  EXPECT_EQ(test_file.BaseName().AsUTF8Unsafe(),
            EvalJs(shell(),
                   "(async () => {"
                   "  let e = await self.chooseFileSystemEntries("
                   "      {type: 'saveFile'});"
                   "  self.entry = e;"
                   "  return e.name; })()"));
  EXPECT_EQ(ui::SelectFileDialog::SELECT_SAVEAS_FILE, dialog_params.type);
  EXPECT_EQ("",
            EvalJs(shell(),
                   "(async () => { const file = await self.entry.getFile(); "
                   "return await file.text(); })()"));
}

IN_PROC_BROWSER_TEST_F(FileSystemChooserBrowserTest,
                       SaveFile_BlockedPermission) {
  const base::FilePath test_file = CreateTestFile("Save File");
  SelectFileDialogParams dialog_params;
  ui::SelectFileDialog::SetFactory(
      new FakeSelectFileDialogFactory({test_file}, &dialog_params));

  testing::StrictMock<MockNativeFileSystemPermissionContext> permission_context;
  static_cast<NativeFileSystemManagerImpl*>(
      BrowserContext::GetStoragePartition(
          shell()->web_contents()->GetBrowserContext(),
          shell()->web_contents()->GetSiteInstance())
          ->GetNativeFileSystemEntryFactory())
      ->SetPermissionContextForTesting(&permission_context);

  EXPECT_CALL(permission_context,
              CanRequestWritePermission(url::Origin::Create(
                  embedded_test_server()->GetURL("/title1.html"))))
      .WillOnce(testing::Return(false));

  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  auto result =
      EvalJs(shell(), "self.chooseFileSystemEntries({type: 'saveFile'})");
  EXPECT_TRUE(result.error.find("not allowed") != std::string::npos)
      << result.error;
  EXPECT_EQ(ui::SelectFileDialog::SELECT_NONE, dialog_params.type);
}

IN_PROC_BROWSER_TEST_F(FileSystemChooserBrowserTest, FullscreenSaveFile) {
  const base::FilePath test_file = CreateTestFile("Hello World");

  SelectFileDialogParams dialog_params;
  ui::SelectFileDialog::SetFactory(
      new FakeSelectFileDialogFactory({test_file}, &dialog_params));
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  EnterFullscreen(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_EQ(test_file.BaseName().AsUTF8Unsafe(),
            EvalJs(shell(),
                   "(async () => {"
                   "  let e = await self.chooseFileSystemEntries("
                   "      {type: 'saveFile'});"
                   "  self.entry = e;"
                   "  return e.name; })()"));
  EXPECT_FALSE(IsFullscreen());
}

IN_PROC_BROWSER_TEST_F(FileSystemChooserBrowserTest, OpenMultipleFiles) {
  const base::FilePath test_file1 = CreateTestFile("file1");
  const base::FilePath test_file2 = CreateTestFile("file2");
  SelectFileDialogParams dialog_params;
  ui::SelectFileDialog::SetFactory(new FakeSelectFileDialogFactory(
      {test_file1, test_file2}, &dialog_params));
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  EXPECT_EQ(ListValueOf(test_file1.BaseName().AsUTF8Unsafe(),
                        test_file2.BaseName().AsUTF8Unsafe()),
            EvalJs(shell(),
                   "(async () => {"
                   "  let e = await self.chooseFileSystemEntries("
                   "      {multiple: true});"
                   "  return e.map(x => x.name); })()"));
  EXPECT_EQ(ui::SelectFileDialog::SELECT_OPEN_MULTI_FILE, dialog_params.type);
}

IN_PROC_BROWSER_TEST_F(FileSystemChooserBrowserTest,
                       FullscreenOpenMultipleFiles) {
  const base::FilePath test_file1 = CreateTestFile("file1");
  const base::FilePath test_file2 = CreateTestFile("file2");
  SelectFileDialogParams dialog_params;
  ui::SelectFileDialog::SetFactory(new FakeSelectFileDialogFactory(
      {test_file1, test_file2}, &dialog_params));
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  EnterFullscreen(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_EQ(ListValueOf(test_file1.BaseName().AsUTF8Unsafe(),
                        test_file2.BaseName().AsUTF8Unsafe()),
            EvalJs(shell(),
                   "(async () => {"
                   "  let e = await self.chooseFileSystemEntries("
                   "      {multiple: true});"
                   "  return e.map(x => x.name); })()"));
  EXPECT_FALSE(IsFullscreen());
}

IN_PROC_BROWSER_TEST_F(FileSystemChooserBrowserTest, OpenDirectory) {
  base::FilePath test_dir = CreateTestDir();
  SelectFileDialogParams dialog_params;
  ui::SelectFileDialog::SetFactory(
      new FakeSelectFileDialogFactory({test_dir}, &dialog_params));
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  EXPECT_EQ(test_dir.BaseName().AsUTF8Unsafe(),
            EvalJs(shell(),
                   "(async () => {"
                   "  let e = await self.chooseFileSystemEntries("
                   "      {type: 'openDirectory'});"
                   "  self.selected_entry = e;"
                   "  return e.name; })()"));
  EXPECT_EQ(ui::SelectFileDialog::SELECT_FOLDER, dialog_params.type);
}

IN_PROC_BROWSER_TEST_F(FileSystemChooserBrowserTest, FullscreenOpenDirectory) {
  base::FilePath test_dir = CreateTestDir();
  SelectFileDialogParams dialog_params;
  ui::SelectFileDialog::SetFactory(
      new FakeSelectFileDialogFactory({test_dir}, &dialog_params));
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  EnterFullscreen(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_EQ(test_dir.BaseName().AsUTF8Unsafe(),
            EvalJs(shell(),
                   "(async () => {"
                   "  let e = await self.chooseFileSystemEntries("
                   "      {type: 'openDirectory'});"
                   "  self.selected_entry = e;"
                   "  return e.name; })()"));
  EXPECT_FALSE(IsFullscreen());
}

IN_PROC_BROWSER_TEST_F(FileSystemChooserBrowserTest, OpenDirectory_DenyAccess) {
  base::FilePath test_dir = CreateTestDir();
  SelectFileDialogParams dialog_params;
  ui::SelectFileDialog::SetFactory(
      new FakeSelectFileDialogFactory({test_dir}, &dialog_params));

  testing::StrictMock<MockNativeFileSystemPermissionContext> permission_context;
  static_cast<NativeFileSystemManagerImpl*>(
      BrowserContext::GetStoragePartition(
          shell()->web_contents()->GetBrowserContext(),
          shell()->web_contents()->GetSiteInstance())
          ->GetNativeFileSystemEntryFactory())
      ->SetPermissionContextForTesting(&permission_context);

  EXPECT_CALL(permission_context, ConfirmSensitiveDirectoryAccess_(
                                      testing::_, testing::_, testing::_,
                                      testing::_, testing::_, testing::_))
      .WillOnce(RunOnceCallback<5>(SensitiveDirectoryResult::kAllowed));

  EXPECT_CALL(
      permission_context,
      ConfirmDirectoryReadAccess_(
          url::Origin::Create(embedded_test_server()->GetURL("/title1.html")),
          test_dir,
          shell()->web_contents()->GetMainFrame()->GetProcess()->GetID(),
          shell()->web_contents()->GetMainFrame()->GetRoutingID(), testing::_))
      .WillOnce(RunOnceCallback<4>(PermissionStatus::DENIED));

  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  auto result =
      EvalJs(shell(), "self.chooseFileSystemEntries({type: 'openDirectory'})");
  EXPECT_TRUE(result.error.find("aborted") != std::string::npos)
      << result.error;
}

IN_PROC_BROWSER_TEST_F(FileSystemChooserBrowserTest,
                       SaveFile_SensitiveDirectory_ExistingFile) {
  const std::string file_contents = "Hello World";
  const base::FilePath test_file = CreateTestFile(file_contents);

  SelectFileDialogParams dialog_params;
  ui::SelectFileDialog::SetFactory(
      new FakeSelectFileDialogFactory({test_file}, &dialog_params));

  testing::StrictMock<MockNativeFileSystemPermissionContext> permission_context;
  static_cast<NativeFileSystemManagerImpl*>(
      BrowserContext::GetStoragePartition(
          shell()->web_contents()->GetBrowserContext(),
          shell()->web_contents()->GetSiteInstance())
          ->GetNativeFileSystemEntryFactory())
      ->SetPermissionContextForTesting(&permission_context);

  EXPECT_CALL(permission_context, ConfirmSensitiveDirectoryAccess_(
                                      testing::_, testing::_, testing::_,
                                      testing::_, testing::_, testing::_))
      .WillOnce(RunOnceCallback<5>(SensitiveDirectoryResult::kAbort));

  EXPECT_CALL(permission_context,
              CanRequestWritePermission(url::Origin::Create(
                  embedded_test_server()->GetURL("/title1.html"))))
      .WillOnce(testing::Return(true));

  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  auto result =
      EvalJs(shell(), "self.chooseFileSystemEntries({type: 'saveFile'})");
  EXPECT_TRUE(result.error.find("aborted") != std::string::npos)
      << result.error;

  {
    // File should still exist, and be unmodified.
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string read_contents;
    EXPECT_TRUE(base::ReadFileToString(test_file, &read_contents));
    EXPECT_EQ(file_contents, read_contents);
  }
}

IN_PROC_BROWSER_TEST_F(FileSystemChooserBrowserTest,
                       SaveFile_SensitiveDirectory_NonExistingFile) {
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

  testing::StrictMock<MockNativeFileSystemPermissionContext> permission_context;
  static_cast<NativeFileSystemManagerImpl*>(
      BrowserContext::GetStoragePartition(
          shell()->web_contents()->GetBrowserContext(),
          shell()->web_contents()->GetSiteInstance())
          ->GetNativeFileSystemEntryFactory())
      ->SetPermissionContextForTesting(&permission_context);

  EXPECT_CALL(permission_context, ConfirmSensitiveDirectoryAccess_(
                                      testing::_, testing::_, testing::_,
                                      testing::_, testing::_, testing::_))
      .WillOnce(RunOnceCallback<5>(SensitiveDirectoryResult::kAbort));

  EXPECT_CALL(permission_context,
              CanRequestWritePermission(url::Origin::Create(
                  embedded_test_server()->GetURL("/title1.html"))))
      .WillOnce(testing::Return(true));

  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  auto result =
      EvalJs(shell(), "self.chooseFileSystemEntries({type: 'saveFile'})");
  EXPECT_TRUE(result.error.find("aborted") != std::string::npos)
      << result.error;

  {
    // File should not have been created.
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_FALSE(base::PathExists(test_file));
  }
}

IN_PROC_BROWSER_TEST_F(FileSystemChooserBrowserTest, AcceptsOptions) {
  SelectFileDialogParams dialog_params;
  ui::SelectFileDialog::SetFactory(
      new CancellingSelectFileDialogFactory(&dialog_params));
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  auto result = EvalJs(shell(),
                       "self.chooseFileSystemEntries({accepts: ["
                       "  {description: 'no-extensions'},"
                       "  {description: 'foo', extensions: ['txt', 'Js']},"
                       "  {mimeTypes: ['image/jpeg']}"
                       "]})");
  EXPECT_TRUE(result.error.find("aborted") != std::string::npos)
      << result.error;

  ASSERT_TRUE(dialog_params.file_types);
  EXPECT_TRUE(dialog_params.file_types->include_all_files);
  ASSERT_EQ(2u, dialog_params.file_types->extensions.size());
  ASSERT_EQ(2u, dialog_params.file_types->extensions[0].size());
  EXPECT_EQ(FILE_PATH_LITERAL("Js"),
            dialog_params.file_types->extensions[0][0]);
  EXPECT_EQ(FILE_PATH_LITERAL("txt"),
            dialog_params.file_types->extensions[0][1]);
  EXPECT_TRUE(base::Contains(dialog_params.file_types->extensions[1],
                             FILE_PATH_LITERAL("jpg")));
  EXPECT_TRUE(base::Contains(dialog_params.file_types->extensions[1],
                             FILE_PATH_LITERAL("jpeg")));

  ASSERT_EQ(2u,
            dialog_params.file_types->extension_description_overrides.size());
  EXPECT_EQ(base::ASCIIToUTF16("foo"),
            dialog_params.file_types->extension_description_overrides[0]);
  EXPECT_EQ(base::ASCIIToUTF16(""),
            dialog_params.file_types->extension_description_overrides[1]);
}

IN_PROC_BROWSER_TEST_F(FileSystemChooserBrowserTest,
                       NativeFileSystemUsageDisablesBackForwardCache) {
  BackForwardCacheDisabledTester tester;

  const base::FilePath test_file = CreateTestFile("file contents");
  SelectFileDialogParams dialog_params;
  ui::SelectFileDialog::SetFactory(
      new FakeSelectFileDialogFactory({test_file}, &dialog_params));
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  EXPECT_EQ(test_file.BaseName().AsUTF8Unsafe(),
            EvalJs(shell(),
                   "(async () => {"
                   "  let e = await self.chooseFileSystemEntries();"
                   "  self.selected_entry = e;"
                   "  return e.name; })()"));
  EXPECT_TRUE(tester.IsDisabledForFrameWithReason(
      shell()->web_contents()->GetMainFrame()->GetProcess()->GetID(),
      shell()->web_contents()->GetMainFrame()->GetRoutingID(),
      "NativeFileSystem"));
}

}  // namespace content
