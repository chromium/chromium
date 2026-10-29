// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/installer/win/setup/file_work_items.h"

#include <windows.h>

#include <array>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "remoting/host/installer/win/setup/file_util.h"
#include "remoting/host/installer/win/setup/installer_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting::installer {

namespace {

inline constexpr wchar_t kTestBinary1[] = L"test_host.exe";
inline constexpr wchar_t kTestBinary2[] = L"test_core.dll";

inline constexpr auto kTestFiles = std::to_array<const wchar_t*>({
    kTestBinary1,
    kTestBinary2,
});

inline constexpr auto kSingleCorePayload = std::to_array<const wchar_t*>({
    kTestBinary2,
});

}  // namespace

class FileWorkItemsTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    source_dir_ = temp_dir_.GetPath().Append(L"source");
    install_dir_ = temp_dir_.GetPath().Append(L"install");

    ASSERT_TRUE(base::CreateDirectory(source_dir_));
    ASSERT_TRUE(base::WriteFile(source_dir_.Append(kTestBinary1), "exe_data"));
    ASSERT_TRUE(base::WriteFile(source_dir_.Append(kTestBinary2), "dll_data"));

    SetMoveFileExCallbackForTesting(base::BindRepeating(
        [](std::vector<base::FilePath>* scheduled, const wchar_t* existing,
           const wchar_t* new_name, DWORD flags) -> BOOL {
          if (flags & MOVEFILE_DELAY_UNTIL_REBOOT) {
            scheduled->emplace_back(existing);
            return TRUE;
          }
          return ::MoveFileExW(existing, new_name, flags);
        },
        &reboot_deleted_files_));
  }

  void TearDown() override {
    SetCopyFileCallbackForTesting(CopyFileCallback());
    SetDeleteFileCallbackForTesting(DeleteFileCallback());
    SetMoveFileExCallbackForTesting(MoveFileExCallback());
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath source_dir_;
  base::FilePath install_dir_;
  std::vector<base::FilePath> reboot_deleted_files_;
};

TEST_F(FileWorkItemsTest, DeployFilesWorkItem_SuccessAndRollback) {
  DeployFilesWorkItem item(source_dir_, install_dir_, kTestFiles);

  EXPECT_TRUE(item.Do());
  EXPECT_TRUE(base::PathExists(install_dir_.Append(kTestBinary1)));
  EXPECT_TRUE(base::PathExists(install_dir_.Append(kTestBinary2)));

  item.Rollback();
  EXPECT_FALSE(base::PathExists(install_dir_.Append(kTestBinary1)));
  EXPECT_FALSE(base::PathExists(install_dir_.Append(kTestBinary2)));
}

TEST_F(FileWorkItemsTest, DeployFilesWorkItem_MissingSourceFileFails) {
  ASSERT_TRUE(base::DeleteFile(source_dir_.Append(kTestBinary2)));

  DeployFilesWorkItem item(source_dir_, install_dir_, kTestFiles);
  EXPECT_FALSE(item.Do());
}

TEST_F(FileWorkItemsTest, DeployFilesWorkItem_CleansUpStaleOldFiles) {
  ASSERT_TRUE(base::CreateDirectory(install_dir_));
  base::FilePath stale_old = install_dir_.Append(L"stale_file.exe.old");
  ASSERT_TRUE(base::WriteFile(stale_old, "stale"));

  DeployFilesWorkItem item(source_dir_, install_dir_, kTestFiles);
  EXPECT_TRUE(item.Do());
  EXPECT_FALSE(base::PathExists(stale_old));
}

TEST_F(FileWorkItemsTest, DeployFilesWorkItem_StagedRenameRollback) {
  ASSERT_TRUE(base::CreateDirectory(install_dir_));
  base::FilePath existing_dll = install_dir_.Append(kTestBinary2);
  ASSERT_TRUE(base::WriteFile(existing_dll, "old_version_data"));

  DeployFilesWorkItem item(source_dir_, install_dir_, kTestFiles);

  EXPECT_TRUE(item.Do());
  std::string new_content;
  EXPECT_TRUE(base::ReadFileToString(existing_dll, &new_content));
  EXPECT_EQ(new_content, "dll_data");

  base::FilePath old_file = existing_dll.AddExtension(kOldFileExtension);
  EXPECT_TRUE(base::PathExists(old_file));

  item.Rollback();
  EXPECT_FALSE(base::PathExists(install_dir_.Append(kTestBinary1)));
  EXPECT_TRUE(base::PathExists(existing_dll));
  std::string restored_content;
  EXPECT_TRUE(base::ReadFileToString(existing_dll, &restored_content));
  EXPECT_EQ(restored_content, "old_version_data");
  EXPECT_FALSE(base::PathExists(old_file));
}

TEST_F(FileWorkItemsTest, DeployFilesWorkItem_RollbackWhenStagedFilesLocked) {
  ASSERT_TRUE(base::CreateDirectory(install_dir_));
  base::FilePath existing_dll = install_dir_.Append(kTestBinary2);
  ASSERT_TRUE(base::WriteFile(existing_dll, "old_version_data"));

  DeployFilesWorkItem item(source_dir_, install_dir_, kTestFiles);
  ASSERT_TRUE(item.Do());
  reboot_deleted_files_.clear();

  // Simulate both newly created and staged files failing deletion during
  // rollback (e.g., locked by an AV scanner).
  SetDeleteFileCallbackForTesting(
      base::BindRepeating([](const base::FilePath&) { return false; }));

  item.Rollback();

  // Newly created `kTestBinary1` could not be deleted, so it must be renamed
  // out of its canonical path to `.old` and scheduled for reboot deletion.
  base::FilePath created_exe = install_dir_.Append(kTestBinary1);
  base::FilePath created_exe_old = created_exe.AddExtension(kOldFileExtension);
  EXPECT_FALSE(base::PathExists(created_exe));
  EXPECT_TRUE(base::PathExists(created_exe_old));

  // Staged `existing_dll` could not be deleted, so the locked new copy is
  // moved to `.1.old` (since `.old` holds the original binary), scheduled for
  // reboot deletion, and the original binary is restored from `.old`.
  base::FilePath staged_dll_temp_old =
      existing_dll.AddExtension(L"1").AddExtension(kOldFileExtension);
  EXPECT_TRUE(base::PathExists(staged_dll_temp_old));
  EXPECT_TRUE(base::PathExists(existing_dll));
  std::string restored_content;
  EXPECT_TRUE(base::ReadFileToString(existing_dll, &restored_content));
  EXPECT_EQ(restored_content, "old_version_data");

  ASSERT_EQ(reboot_deleted_files_.size(), 2u);
  EXPECT_EQ(reboot_deleted_files_[0], created_exe_old);
  EXPECT_EQ(reboot_deleted_files_[1], staged_dll_temp_old);
}

TEST_F(FileWorkItemsTest,
       DeployFilesWorkItem_RollbackWhenDeleteAndRenameBothFail) {
  DeployFilesWorkItem item(source_dir_, install_dir_, kTestFiles);
  ASSERT_TRUE(item.Do());
  reboot_deleted_files_.clear();

  // Simulate both DeleteFile and MoveFileEx rename failing during rollback.
  SetDeleteFileCallbackForTesting(
      base::BindRepeating([](const base::FilePath&) { return false; }));
  SetMoveFileExCallbackForTesting(base::BindRepeating(
      [](std::vector<base::FilePath>* scheduled, const wchar_t* existing,
         const wchar_t* new_name, DWORD flags) -> BOOL {
        if (flags & MOVEFILE_DELAY_UNTIL_REBOOT) {
          scheduled->emplace_back(existing);
          return TRUE;
        }
        return FALSE;
      },
      &reboot_deleted_files_));

  item.Rollback();

  // When MoveLockedFileToOldPath also fails, RollbackImpl falls back to
  // scheduling the canonical file paths for deletion on reboot.
  ASSERT_EQ(reboot_deleted_files_.size(), 2u);
  EXPECT_EQ(reboot_deleted_files_[0], install_dir_.Append(kTestBinary2));
  EXPECT_EQ(reboot_deleted_files_[1], install_dir_.Append(kTestBinary1));
}

TEST_F(FileWorkItemsTest,
       DeployFilesWorkItem_PartialCopyFailureRollsBackCompletedFiles) {
  ASSERT_TRUE(base::CreateDirectory(install_dir_));
  base::FilePath existing_exe = install_dir_.Append(kTestBinary1);
  ASSERT_TRUE(base::WriteFile(existing_exe, "original_exe_data"));

  // Allow copying the first file (`kTestBinary1`) to succeed, then fail on the
  // second file (`kTestBinary2`).
  int copy_count = 0;
  SetCopyFileCallbackForTesting(base::BindRepeating(
      [](int* count, const base::FilePath& from, const base::FilePath& to) {
        if (++(*count) == 1) {
          return base::CopyFile(from, to);
        }
        return false;
      },
      &copy_count));

  DeployFilesWorkItem item(source_dir_, install_dir_, kTestFiles);
  EXPECT_FALSE(item.Do());

  item.Rollback();

  // `kTestBinary1` must be restored to its original content, and `kTestBinary2`
  // must not exist.
  EXPECT_TRUE(base::PathExists(existing_exe));
  std::string restored_exe;
  EXPECT_TRUE(base::ReadFileToString(existing_exe, &restored_exe));
  EXPECT_EQ(restored_exe, "original_exe_data");
  EXPECT_FALSE(base::PathExists(install_dir_.Append(kTestBinary2)));
}

TEST_F(FileWorkItemsTest, DeleteFilesWorkItem_DeletesInstalledFiles) {
  ASSERT_TRUE(base::CreateDirectory(install_dir_));
  ASSERT_TRUE(base::WriteFile(install_dir_.Append(kTestBinary1), "exe_data"));
  ASSERT_TRUE(base::WriteFile(install_dir_.Append(kTestBinary2), "dll_data"));

  DeleteFilesWorkItem item(install_dir_, kTestFiles);
  EXPECT_TRUE(item.Do());
  EXPECT_FALSE(base::PathExists(install_dir_.Append(kTestBinary1)));
  EXPECT_FALSE(base::PathExists(install_dir_.Append(kTestBinary2)));
  EXPECT_FALSE(base::DirectoryExists(install_dir_));
}

TEST_F(FileWorkItemsTest, DeleteFilesWorkItem_NonExistentDirectorySucceeds) {
  base::FilePath non_existent = temp_dir_.GetPath().Append(L"does_not_exist");
  DeleteFilesWorkItem item(non_existent, kTestFiles);
  EXPECT_TRUE(item.Do());
}

TEST_F(FileWorkItemsTest,
       DeleteFilesWorkItem_PreservesDirectoryWithExtraFiles) {
  DeployFilesWorkItem deploy_item(source_dir_, install_dir_,
                                  kSingleCorePayload);
  ASSERT_TRUE(deploy_item.Do());
  reboot_deleted_files_.clear();

  // Add an untracked file in `install_dir_`. DeleteFilesWorkItem should delete
  // the payload file and schedule `install_dir_` for reboot removal when
  // RemoveDirectoryW returns ERROR_DIR_NOT_EMPTY.
  base::FilePath extra_file = install_dir_.Append(L"extra_file.txt");
  ASSERT_TRUE(base::WriteFile(extra_file, "extra"));

  DeleteFilesWorkItem delete_item(install_dir_, kSingleCorePayload);
  EXPECT_TRUE(delete_item.Do());
  EXPECT_FALSE(base::PathExists(install_dir_.Append(kTestBinary2)));
  EXPECT_TRUE(base::PathExists(extra_file));
  ASSERT_EQ(reboot_deleted_files_.size(), 1u);
  EXPECT_EQ(reboot_deleted_files_[0], install_dir_);
}

TEST_F(FileWorkItemsTest, DeleteFilesWorkItem_LockedFileScheduledForReboot) {
  DeployFilesWorkItem deploy_item(source_dir_, install_dir_,
                                  kSingleCorePayload);
  ASSERT_TRUE(deploy_item.Do());
  reboot_deleted_files_.clear();

  base::FilePath installed_file = install_dir_.Append(kTestBinary2);
  base::FilePath old_file = installed_file.AddExtension(kOldFileExtension);

  SetDeleteFileCallbackForTesting(
      base::BindRepeating([](const base::FilePath&) { return false; }));

  DeleteFilesWorkItem delete_item(install_dir_, kSingleCorePayload);
  EXPECT_TRUE(delete_item.Do());
  // Both the renamed `.old` file and `install_dir_` should be scheduled for
  // reboot removal.
  ASSERT_EQ(reboot_deleted_files_.size(), 2u);
  EXPECT_EQ(reboot_deleted_files_[0], old_file);
  EXPECT_EQ(reboot_deleted_files_[1], install_dir_);
  EXPECT_FALSE(base::PathExists(installed_file));
  EXPECT_TRUE(base::PathExists(old_file));
}

TEST_F(FileWorkItemsTest,
       DeleteFilesWorkItem_ExclusiveLockedFileScheduledForReboot) {
  DeployFilesWorkItem deploy_item(source_dir_, install_dir_,
                                  kSingleCorePayload);
  ASSERT_TRUE(deploy_item.Do());
  reboot_deleted_files_.clear();

  base::FilePath installed_file = install_dir_.Append(kTestBinary2);

  // Lock the file exclusively without FILE_SHARE_DELETE so
  // MoveLockedFileToOldPath fails, triggering the fallback to
  // ScheduleDeleteOnReboot(file_path).
  base::File locked_file(installed_file,
                         base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(locked_file.IsValid());

  DeleteFilesWorkItem delete_item(install_dir_, kSingleCorePayload);
  EXPECT_FALSE(delete_item.Do());
  // Both the locked original file and `install_dir_` should be scheduled for
  // reboot removal.
  ASSERT_EQ(reboot_deleted_files_.size(), 2u);
  EXPECT_EQ(reboot_deleted_files_[0], installed_file);
  EXPECT_EQ(reboot_deleted_files_[1], install_dir_);
  EXPECT_TRUE(base::PathExists(installed_file));

  locked_file.Close();
}

}  // namespace remoting::installer
