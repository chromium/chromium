// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_handle_impl_browsertest_util.h"

#include "base/files/file_util.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/browser/file_system_access/mock_file_system_access_permission_grant.h"
#include "content/browser/file_system_access/spying_shared_handle_state_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/file_system_chooser_test_helpers.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features_generated.h"

namespace content {

FileSystemAccessHandleImplBrowserTestBase::
    FileSystemAccessHandleImplBrowserTestBase() {
  scoped_features_.InitAndEnableFeature(
      blink::features::kFileSystemAccessLocal);
}

FileSystemAccessHandleImplBrowserTestBase::
    ~FileSystemAccessHandleImplBrowserTestBase() = default;

void FileSystemAccessHandleImplBrowserTestBase::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  ASSERT_TRUE(embedded_test_server()->Start());
  test_url_ = embedded_test_server()->GetURL("/title1.html");

  ContentBrowserTest::SetUp();
}

void FileSystemAccessHandleImplBrowserTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  // Enable experimental web platform features to enable write access.
  command_line->AppendSwitch(switches::kEnableExperimentalWebPlatformFeatures);
}

void FileSystemAccessHandleImplBrowserTestBase::TearDown() {
  ContentBrowserTest::TearDown();
  ASSERT_TRUE(temp_dir_.Delete());
  ui::SelectFileDialog::SetFactory(nullptr);
}

void FileSystemAccessHandleImplBrowserTestBase::CreateTestDirectoryInDirectory(
    const base::FilePath& directory_path) {
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

void FileSystemAccessHandleImplBrowserTestBase::CreateTestFileInDirectory(
    const base::FilePath& directory_path,
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

FileSystemAccessHandleImplPermissionBrowserTestBase::
    FileSystemAccessHandleImplPermissionBrowserTestBase() = default;
FileSystemAccessHandleImplPermissionBrowserTestBase::
    ~FileSystemAccessHandleImplPermissionBrowserTestBase() = default;

void FileSystemAccessHandleImplPermissionBrowserTestBase::SetUpOnMainThread() {
  FileSystemAccessHandleImplBrowserTestBase::SetUpOnMainThread();

  auto* manager = static_cast<FileSystemAccessManagerImpl*>(
      shell()
          ->web_contents()
          ->GetBrowserContext()
          ->GetStoragePartition(shell()->web_contents()->GetSiteInstance())
          ->GetFileSystemAccessEntryFactory());

  spying_factory_ = std::make_unique<SpyingSharedHandleStateFactory>();
  manager->SetSharedHandleStateCallbackForTesting(
      base::BindRepeating(&SpyingSharedHandleStateFactory::Build,
                          // Safe because `spying_factory_` is owned by this
                          // test fixture, which outlives the manager.
                          base::Unretained(spying_factory_.get())));
}

void FileSystemAccessHandleImplPermissionBrowserTestBase::
    TearDownOnMainThread() {
  auto* manager = static_cast<FileSystemAccessManagerImpl*>(
      shell()
          ->web_contents()
          ->GetBrowserContext()
          ->GetStoragePartition(shell()->web_contents()->GetSiteInstance())
          ->GetFileSystemAccessEntryFactory());
  manager->SetSharedHandleStateCallbackForTesting(base::NullCallback());
  FileSystemAccessHandleImplBrowserTestBase::TearDownOnMainThread();
}

void FileSystemAccessHandleImplPermissionBrowserTestBase::
    ExpectGetPermissionStatusAndReturnGranted(
        blink::mojom::FileSystemAccessPermissionMode expected_mode,
        size_t expected_shared_handle_state_count) {
  ASSERT_EQ(spying_factory_->build_count(),
            expected_shared_handle_state_count);

  auto* read_grant = static_cast<MockFileSystemAccessPermissionGrant*>(
      spying_factory_->spying_read_grant());
  auto* write_grant = static_cast<MockFileSystemAccessPermissionGrant*>(
      spying_factory_->spying_write_grant());

  // Clears all previous default actions and expectations before setting the
  // new ones we care about.
  testing::Mock::VerifyAndClear(read_grant);
  testing::Mock::VerifyAndClear(write_grant);

  switch (expected_mode) {
    case blink::mojom::FileSystemAccessPermissionMode::kRead:
      EXPECT_CALL(*read_grant, GetStatus())
          .WillRepeatedly(
              testing::Return(blink::mojom::PermissionStatus::GRANTED));
      return;
    case blink::mojom::FileSystemAccessPermissionMode::kReadWrite:
      EXPECT_CALL(*read_grant, GetStatus())
          .WillRepeatedly(
              testing::Return(blink::mojom::PermissionStatus::GRANTED));
      EXPECT_CALL(*write_grant, GetStatus())
          .WillRepeatedly(
              testing::Return(blink::mojom::PermissionStatus::GRANTED));
      return;
    case blink::mojom::FileSystemAccessPermissionMode::kWrite:
      EXPECT_CALL(*write_grant, GetStatus())
          .WillRepeatedly(
              testing::Return(blink::mojom::PermissionStatus::GRANTED));
      return;
  }
}

}  // namespace content
