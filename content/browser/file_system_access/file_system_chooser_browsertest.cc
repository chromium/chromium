// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/file_system_access/file_system_chooser_test_helpers.h"
#include "content/browser/file_system_access/fixed_native_file_system_permission_grant.h"
#include "content/browser/file_system_access/mock_native_file_system_permission_context.h"
#include "content/browser/file_system_access/mock_native_file_system_permission_grant.h"
#include "content/browser/file_system_access/native_file_system_manager_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace content {

using base::test::RunOnceCallback;
using blink::mojom::PermissionStatus;
using SensitiveDirectoryResult =
    NativeFileSystemPermissionContext::SensitiveDirectoryResult;

static constexpr char kTestMountPoint[] = "testfs";

// This browser test implements end-to-end tests for the file picker
// APIs.
class FileSystemChooserBrowserTest : public ContentBrowserTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // Register an external mount point to test support for virtual paths.
    // This maps the virtual path a native local path to make these tests work
    // on all platforms. We're not testing more complicated ChromeOS specific
    // file system backends here.
    storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        kTestMountPoint, storage::kFileSystemTypeNativeLocal,
        storage::FileSystemMountOption(), temp_dir_.GetPath());

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
    storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
        kTestMountPoint);
    ui::SelectFileDialog::SetFactory(nullptr);
    ASSERT_TRUE(temp_dir_.Delete());
  }

  bool IsFullscreen() {
    WebContents* web_contents = shell()->web_contents();
    return web_contents->IsFullscreen();
  }

  void EnterFullscreen() {
    WebContentsImpl* web_contents_impl =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    web_contents_impl->EnterFullscreenMode(web_contents_impl->GetMainFrame(),
                                           {});
  }

  base::FilePath CreateTestFile(const std::string& contents) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath result;
    EXPECT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &result));
    EXPECT_TRUE(base::WriteFile(result, contents));
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
  base::ScopedTempDir temp_dir_;
};

IN_PROC_BROWSER_TEST_F(FileSystemChooserBrowserTest, CancelDialog) {
  ui::SelectFileDialog::SetFactory(new CancellingSelectFileDialogFactory);
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  auto result = EvalJs(shell(), "self.showOpenFilePicker()");
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
                   "  let [e] = await self.showOpenFilePicker();"
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

IN_PROC_BROWSER_TEST_F(FileSystemChooserBrowserTest, OpenFileNonASCII) {
  const std::string file_contents = "hello world!";
  const base::FilePath test_file =
      temp_dir_.GetPath().Append(base::FilePath::FromUTF8Unsafe("😋.txt"));
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::WriteFile(test_file, file_contents));
  }

  SelectFileDialogParams dialog_params;
  ui::SelectFileDialog::SetFactory(
      new FakeSelectFileDialogFactory({test_file}, &dialog_params));
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  EXPECT_EQ(test_file.BaseName().AsUTF8Unsafe(),
            EvalJs(shell(),
                   "(async () => {"
                   "  let [e] = await self.showOpenFilePicker();"
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
  EnterFullscreen();
  EXPECT_TRUE(IsFullscreen());
  EXPECT_EQ(test_file.BaseName().AsUTF8Unsafe(),
            EvalJs(shell(),
                   "(async () => {"
                   "  let [e] = await self.showOpenFilePicker();"
                   "  self.selected_entry = e;"
                   "  return e.name; })()"));
  EXPECT_FALSE(IsFullscreen());
}

IN_PROC_BROWSER_TEST_F(FileSystemChooserBrowserTest,
                       OpenFile_BlockedPermission) {
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
              CanObtainReadPermission(url::Origin::Create(
                  embedded_test_server()->GetURL("/title1.html"))))
      .WillOnce(testing::Return(false));

  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  auto result = EvalJs(shell(), "self.showOpenFilePicker()");
  EXPECT_TRUE(result.error.find("not allowed") != std::string::npos)
      << result.error;
  EXPECT_EQ(ui::SelectFileDialog::SELECT_NONE, dialog_params.type);
}

IN_PROC_BROWSER_TEST_F(FileSystemChooserBrowserTest, OpenFile_ExternalPath) {
  const std::string file_contents = "hello world!";
  const base::FilePath test_file = CreateTestFile(file_contents);
  const base::FilePath virtual_path =
      base::FilePath::FromUTF8Unsafe(kTestMountPoint)
          .Append(test_file.BaseName());

  ui::SelectedFileInfo selected_file = {base::FilePath(), base::FilePath()};
  selected_file.virtual_path = virtual_path;

  SelectFileDialogParams dialog_params;
  ui::SelectFileDialog::SetFactory(
      new FakeSelectFileDialogFactory({selected_file}, &dialog_params));
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  EXPECT_EQ(virtual_path.BaseName().AsUTF8Unsafe(),
            EvalJs(shell(),
                   "(async () => {"
                   "  let [e] = await self.showOpenFilePicker();"
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

IN_PROC_BROWSER_TEST_F(FileSystemChooserBrowserTest, SaveFile_NonExistingFile) {
  const std::string file_contents = "file contents to write";
  const base::FilePath test_file = CreateTestFile("");
  {
    // Delete file, since SaveFile should be able to deal with non-existing
    // files.
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::DeleteFile(test_file));
  }
  SelectFileDialogParams dialog_params;
  ui::SelectFileDialog::SetFactory(
      new FakeSelectFileDialogFactory({test_file}, &dialog_params));
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  EXPECT_EQ(test_file.BaseName().AsUTF8Unsafe(),
            EvalJs(shell(),
                   "(async () => {"
                   "  let e = await self.showSaveFilePicker();"
                   "  self.entry = e;"
                   "  return e.name; })()"));
  EXPECT_EQ(ui::SelectFileDialog::SELECT_SAVEAS_FILE, dialog_params.type);
  EXPECT_EQ(int{file_contents.size()},
            EvalJs(shell(),
                   JsReplace("(async () => {"
                             "  const w = await self.entry.createWritable();"
                             "  await w.write(new Blob([$1]));"
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
                   "  let e = await self.showSaveFilePicker();"
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
              CanObtainReadPermission(url::Origin::Create(
                  embedded_test_server()->GetURL("/title1.html"))))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(permission_context,
              CanObtainWritePermission(url::Origin::Create(
                  embedded_test_server()->GetURL("/title1.html"))))
      .WillOnce(testing::Return(false));

  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  auto result = EvalJs(shell(), "self.showSaveFilePicker()");
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
  EnterFullscreen();
  EXPECT_EQ(test_file.BaseName().AsUTF8Unsafe(),
            EvalJs(shell(),
                   "(async () => {"
                   "  let e = await self.showSaveFilePicker();"
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
                   "  let e = await self.showOpenFilePicker("
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
  EnterFullscreen();
  EXPECT_EQ(ListValueOf(test_file1.BaseName().AsUTF8Unsafe(),
                        test_file2.BaseName().AsUTF8Unsafe()),
            EvalJs(shell(),
                   "(async () => {"
                   "  let e = await self.showOpenFilePicker("
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
                   "  let e = await self.showDirectoryPicker();"
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
  EnterFullscreen();
  EXPECT_EQ(test_dir.BaseName().AsUTF8Unsafe(),
            EvalJs(shell(),
                   "(async () => {"
                   "  let e = await self.showDirectoryPicker();"
                   "  self.selected_entry = e;"
                   "  return e.name; })()"));
  EXPECT_FALSE(IsFullscreen());
}

IN_PROC_BROWSER_TEST_F(FileSystemChooserBrowserTest,
                       OpenDirectory_BlockedPermission) {
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

  EXPECT_CALL(permission_context,
              CanObtainReadPermission(url::Origin::Create(
                  embedded_test_server()->GetURL("/title1.html"))))
      .WillOnce(testing::Return(false));

  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  auto result = EvalJs(shell(), "self.showDirectoryPicker()");
  EXPECT_TRUE(result.error.find("not allowed") != std::string::npos)
      << result.error;
  EXPECT_EQ(ui::SelectFileDialog::SELECT_NONE, dialog_params.type);
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

  auto read_grant = base::MakeRefCounted<
      testing::StrictMock<MockNativeFileSystemPermissionGrant>>();
  auto write_grant = base::MakeRefCounted<FixedNativeFileSystemPermissionGrant>(
      PermissionStatus::ASK, base::FilePath());

  EXPECT_CALL(permission_context,
              CanObtainReadPermission(url::Origin::Create(
                  embedded_test_server()->GetURL("/title1.html"))))
      .WillOnce(testing::Return(true));

  EXPECT_CALL(permission_context,
              ConfirmSensitiveDirectoryAccess_(
                  testing::_, testing::_, testing::_, testing::_, testing::_))
      .WillOnce(RunOnceCallback<4>(SensitiveDirectoryResult::kAllowed));

  auto origin =
      url::Origin::Create(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_CALL(permission_context,
              GetReadPermissionGrant(
                  origin, test_dir,
                  NativeFileSystemPermissionContext::HandleType::kDirectory,
                  NativeFileSystemPermissionContext::UserAction::kOpen))
      .WillOnce(testing::Return(read_grant));
  EXPECT_CALL(permission_context,
              GetWritePermissionGrant(
                  origin, test_dir,
                  NativeFileSystemPermissionContext::HandleType::kDirectory,
                  NativeFileSystemPermissionContext::UserAction::kOpen))
      .WillOnce(testing::Return(write_grant));

  EXPECT_CALL(
      *read_grant,
      RequestPermission_(
          GlobalFrameRoutingId(
              shell()->web_contents()->GetMainFrame()->GetProcess()->GetID(),
              shell()->web_contents()->GetMainFrame()->GetRoutingID()),
          NativeFileSystemPermissionGrant::UserActivationState::kNotRequired,
          testing::_))
      .WillOnce(RunOnceCallback<2>(NativeFileSystemPermissionGrant::
                                       PermissionRequestOutcome::kUserDenied));
  EXPECT_CALL(*read_grant, GetStatus())
      .WillRepeatedly(testing::Return(PermissionStatus::ASK));

  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  auto result = EvalJs(shell(), "self.showDirectoryPicker()");
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

  EXPECT_CALL(permission_context,
              ConfirmSensitiveDirectoryAccess_(
                  testing::_, testing::_, testing::_, testing::_, testing::_))
      .WillOnce(RunOnceCallback<4>(SensitiveDirectoryResult::kAbort));

  EXPECT_CALL(permission_context,
              CanObtainReadPermission(url::Origin::Create(
                  embedded_test_server()->GetURL("/title1.html"))))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(permission_context,
              CanObtainWritePermission(url::Origin::Create(
                  embedded_test_server()->GetURL("/title1.html"))))
      .WillOnce(testing::Return(true));

  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  auto result = EvalJs(shell(), "self.showSaveFilePicker()");
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
    ASSERT_TRUE(base::DeleteFile(test_file));
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

  EXPECT_CALL(permission_context,
              ConfirmSensitiveDirectoryAccess_(
                  testing::_, testing::_, testing::_, testing::_, testing::_))
      .WillOnce(RunOnceCallback<4>(SensitiveDirectoryResult::kAbort));

  EXPECT_CALL(permission_context,
              CanObtainReadPermission(url::Origin::Create(
                  embedded_test_server()->GetURL("/title1.html"))))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(permission_context,
              CanObtainWritePermission(url::Origin::Create(
                  embedded_test_server()->GetURL("/title1.html"))))
      .WillOnce(testing::Return(true));

  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  auto result = EvalJs(shell(), "self.showSaveFilePicker()");
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
  auto result =
      EvalJs(shell(),
             "self.showOpenFilePicker({types: ["
             "  {description: 'foo', accept: {'text/custom': ['.txt', '.Js']}},"
             "  {accept: {'image/jpeg': []}},"
             "  {accept: {'image/svg+xml': '.svg'}},"
             "]})");
  EXPECT_TRUE(result.error.find("aborted") != std::string::npos)
      << result.error;

  ASSERT_TRUE(dialog_params.file_types);
  EXPECT_TRUE(dialog_params.file_types->include_all_files);
  ASSERT_EQ(3u, dialog_params.file_types->extensions.size());
  ASSERT_EQ(2u, dialog_params.file_types->extensions[0].size());
  EXPECT_EQ(FILE_PATH_LITERAL("txt"),
            dialog_params.file_types->extensions[0][0]);
  EXPECT_EQ(FILE_PATH_LITERAL("Js"),
            dialog_params.file_types->extensions[0][1]);
  EXPECT_TRUE(base::Contains(dialog_params.file_types->extensions[1],
                             FILE_PATH_LITERAL("jpg")));
  EXPECT_TRUE(base::Contains(dialog_params.file_types->extensions[1],
                             FILE_PATH_LITERAL("jpeg")));
  EXPECT_TRUE(base::Contains(dialog_params.file_types->extensions[2],
                             FILE_PATH_LITERAL("svg")));

  ASSERT_EQ(3u,
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
                   "  let [e] = await self.showOpenFilePicker();"
                   "  self.selected_entry = e;"
                   "  return e.name; })()"));
  EXPECT_TRUE(tester.IsDisabledForFrameWithReason(
      shell()->web_contents()->GetMainFrame()->GetProcess()->GetID(),
      shell()->web_contents()->GetMainFrame()->GetRoutingID(),
      "NativeFileSystem"));
}

}  // namespace content
