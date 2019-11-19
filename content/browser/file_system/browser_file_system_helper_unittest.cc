// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/single_thread_task_runner.h"
#include "base/test/null_task_runner.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/file_system/browser_file_system_helper.h"
#include "content/public/common/drop_data.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "net/base/filename_util.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_options.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace browser_file_system_helper_unittest {

const int kRendererID = 42;

TEST(BrowserFileSystemHelperTest,
     PrepareDropDataForChildProcess_FileSystemFiles) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  BrowserTaskEnvironment task_environment;
  TestBrowserContext browser_context;
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();
  p->Add(kRendererID, &browser_context);

  // Prepare |original_file| FileSystemURL that comes from a |sensitive_origin|.
  // This attempts to simulate for unit testing the drive URL from
  // https://crbug.com/705295#c23.
  const GURL kSensitiveOrigin(GetWebUIURL("hhaomjibdihmijegdhdafkllkbggdgoj"));
  const char kMountName[] = "drive-testuser%40gmail.com-hash";
  const base::FilePath kTestPath(FILE_PATH_LITERAL("root/dir/testfile.jpg"));
  base::FilePath mount_path = temp_dir.GetPath().AppendASCII(kMountName);
  scoped_refptr<storage::ExternalMountPoints> external_mount_points =
      storage::ExternalMountPoints::CreateRefCounted();
  EXPECT_TRUE(external_mount_points->RegisterFileSystem(
      kMountName, storage::FileSystemType::kFileSystemTypeTest,
      storage::FileSystemMountOption(), mount_path));
  storage::FileSystemURL original_file =
      external_mount_points->CreateExternalFileSystemURL(kSensitiveOrigin,
                                                         kMountName, kTestPath);
  EXPECT_TRUE(original_file.is_valid());
  EXPECT_EQ(kSensitiveOrigin, original_file.origin().GetURL());

  // Prepare fake FileSystemContext to use in the test.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner(
      new base::NullTaskRunner);
  scoped_refptr<base::SequencedTaskRunner> file_task_runner(
      new base::NullTaskRunner);
  storage::FileSystemOptions file_system_options(
      storage::FileSystemOptions::PROFILE_MODE_NORMAL,
      false /* force_in_memory */, std::vector<std::string>());
  scoped_refptr<storage::FileSystemContext> test_file_system_context(
      new storage::FileSystemContext(
          io_task_runner.get(), file_task_runner.get(),
          external_mount_points.get(),
          nullptr,  // special_storage_policy
          nullptr,  // quota_manager_proxy,
          std::vector<std::unique_ptr<storage::FileSystemBackend>>(),
          std::vector<storage::URLRequestAutoMountHandler>(),
          base::FilePath(),  // partition_path
          file_system_options));

  // Prepare content::DropData containing |file_system_url|.
  DropData::FileSystemFileInfo filesystem_file_info;
  filesystem_file_info.url = original_file.ToGURL();
  filesystem_file_info.size = 123;
  filesystem_file_info.filesystem_id = original_file.filesystem_id();
  DropData drop_data;
  drop_data.file_system_files.push_back(filesystem_file_info);

  // Verify that initially no access is be granted to the |kSensitiveOrigin|.
  EXPECT_FALSE(p->CanCommitURL(kRendererID, kSensitiveOrigin));

  // Verify that initially no access is granted to the |original_file|.
  EXPECT_FALSE(p->CanReadFileSystemFile(kRendererID, original_file));
  EXPECT_FALSE(p->CanWriteFileSystemFile(kRendererID, original_file));
  EXPECT_FALSE(p->CanCreateFileSystemFile(kRendererID, original_file));
  EXPECT_FALSE(p->CanCopyIntoFileSystemFile(kRendererID, original_file));
  EXPECT_FALSE(p->CanDeleteFileSystemFile(kRendererID, original_file));

  // Invoke the API under test to grant access to |drop_data|.
  PrepareDropDataForChildProcess(&drop_data, p, kRendererID,
                                 test_file_system_context.get());

  // Verify that |drop_data| is mostly unchanged.
  EXPECT_EQ(0u, drop_data.filenames.size());
  EXPECT_EQ(1u, drop_data.file_system_files.size());
  EXPECT_EQ(123, drop_data.file_system_files[0].size);
  // It is okay if |drop_data.file_system_files[0].url| and
  // |drop_data.file_system_files[0].filesystem_id| change (to aid in enforcing
  // proper access patterns that are verified below).

  // Verify that the URL didn't change *too* much.
  storage::FileSystemURL dropped_file =
      test_file_system_context->CrackURL(drop_data.file_system_files[0].url);
  EXPECT_TRUE(dropped_file.is_valid());
  EXPECT_EQ(original_file.origin(), dropped_file.origin());
  EXPECT_EQ(original_file.path().BaseName(), dropped_file.path().BaseName());

  // Verify that there is still no access to |kSensitiveOrigin|.
  EXPECT_FALSE(p->CanCommitURL(kRendererID, kSensitiveOrigin));

  // Verify that there is still no access to |original_file|.
  EXPECT_FALSE(p->CanReadFileSystemFile(kRendererID, original_file));
  EXPECT_FALSE(p->CanWriteFileSystemFile(kRendererID, original_file));
  EXPECT_FALSE(p->CanCreateFileSystemFile(kRendererID, original_file));
  EXPECT_FALSE(p->CanCopyIntoFileSystemFile(kRendererID, original_file));
  EXPECT_FALSE(p->CanDeleteFileSystemFile(kRendererID, original_file));

  // Verify that read access (and no other access) is granted for
  // |dropped_file|.
  EXPECT_TRUE(p->CanReadFileSystemFile(kRendererID, dropped_file));
  EXPECT_FALSE(p->CanWriteFileSystemFile(kRendererID, dropped_file));
  EXPECT_FALSE(p->CanCreateFileSystemFile(kRendererID, dropped_file));
  EXPECT_FALSE(p->CanCopyIntoFileSystemFile(kRendererID, dropped_file));
  EXPECT_FALSE(p->CanDeleteFileSystemFile(kRendererID, dropped_file));

  p->Remove(kRendererID);
}

TEST(BrowserFileSystemHelperTest, PrepareDropDataForChildProcess_LocalFiles) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  BrowserTaskEnvironment task_environment;
  TestBrowserContext browser_context;
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();
  p->Add(kRendererID, &browser_context);

  // Prepare content::DropData containing some local files.
  const base::FilePath kDraggedFile =
      temp_dir.GetPath().AppendASCII("dragged_file.txt");
  const base::FilePath kOtherFile =
      temp_dir.GetPath().AppendASCII("other_file.txt");
  DropData drop_data;
  drop_data.filenames.push_back(ui::FileInfo(kDraggedFile, base::FilePath()));

  // Verify that initially no access is granted to both |kDraggedFile| and
  // |kOtherFile|.
  EXPECT_FALSE(p->CanReadFile(kRendererID, kDraggedFile));
  EXPECT_FALSE(p->CanReadFile(kRendererID, kOtherFile));
  EXPECT_FALSE(
      p->CanCommitURL(kRendererID, net::FilePathToFileURL(kDraggedFile)));
  EXPECT_FALSE(p->CanCreateReadWriteFile(kRendererID, kDraggedFile));
  EXPECT_FALSE(p->CanCreateReadWriteFile(kRendererID, kOtherFile));
  EXPECT_FALSE(
      p->CanCommitURL(kRendererID, net::FilePathToFileURL(kOtherFile)));

  // Invoke the API under test to grant access to |drop_data|.
  PrepareDropDataForChildProcess(&drop_data, p, kRendererID, nullptr);

  // Verify that |drop_data| is unchanged.
  EXPECT_EQ(0u, drop_data.file_system_files.size());
  EXPECT_EQ(1u, drop_data.filenames.size());
  EXPECT_EQ(kDraggedFile, drop_data.filenames[0].path);

  // Verify that read access (and no other access) is granted for
  // |kDraggedFile|.
  EXPECT_TRUE(p->CanReadFile(kRendererID, kDraggedFile));
  EXPECT_FALSE(p->CanCreateReadWriteFile(kRendererID, kDraggedFile));
  EXPECT_TRUE(
      p->CanCommitURL(kRendererID, net::FilePathToFileURL(kDraggedFile)));

  // Verify that there is still no access for |kOtherFile|.
  EXPECT_FALSE(p->CanReadFile(kRendererID, kOtherFile));
  EXPECT_FALSE(p->CanCreateReadWriteFile(kRendererID, kOtherFile));
  EXPECT_FALSE(
      p->CanCommitURL(kRendererID, net::FilePathToFileURL(kOtherFile)));

  p->Remove(kRendererID);
}

}  // namespace browser_file_system_helper_unittest
}  // namespace content
