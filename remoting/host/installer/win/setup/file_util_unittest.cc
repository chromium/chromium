// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/installer/win/setup/file_util.h"

#include <windows.h>

#include <array>
#include <memory>
#include <string>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/win/scoped_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting::installer {

namespace {

inline constexpr wchar_t kTestHostBinary[] = L"remoting_host.exe";
inline constexpr wchar_t kTestCoreBinary[] = L"remoting_core.dll";
inline constexpr wchar_t kTestCoreOld0[] = L"remoting_core.dll.old";
inline constexpr wchar_t kTestCoreOld1[] = L"remoting_core.dll.1.old";

inline constexpr auto kTestPayloadFiles = std::to_array<const wchar_t*>({
    kTestHostBinary,
    kTestCoreBinary,
});

inline constexpr auto kSingleCorePayload = std::to_array<const wchar_t*>({
    kTestCoreBinary,
});

}  // namespace

class FileUtilTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    source_dir_ = temp_dir_.GetPath().Append(L"source");
    install_dir_ = temp_dir_.GetPath().Append(L"install");
    ASSERT_TRUE(base::CreateDirectory(source_dir_));

    SetCopyFileCallbackForTesting(
        base::BindRepeating(&FileUtilTest::OnCopyFile, base::Unretained(this)));
    SetDeleteFileCallbackForTesting(base::BindRepeating(
        &FileUtilTest::OnDeleteFile, base::Unretained(this)));
    SetMoveFileExCallbackForTesting(base::BindRepeating(
        &FileUtilTest::OnMoveFileEx, base::Unretained(this)));
  }

  void TearDown() override {
    SetCopyFileCallbackForTesting(CopyFileCallback());
    SetDeleteFileCallbackForTesting(DeleteFileCallback());
    SetMoveFileExCallbackForTesting(MoveFileExCallback());
  }

  // Simulates that `path` is in use by an active process:
  // - Direct CopyFile to `path` fails (returns false).
  // - DeleteFile on `path` fails (returns false).
  // - MoveFileExW on `path` succeeds (moves file on disk and transfers lock).
  void SimulateFileLocked(const base::FilePath& path) {
    locked_files_.insert(path);
  }

  void SimulateFileUnlocked(const base::FilePath& path) {
    locked_files_.erase(path);
  }

  int scheduled_reboot_delete_count() const { return reboot_delete_count_; }

 private:
  bool OnCopyFile(const base::FilePath& from, const base::FilePath& to) {
    if (locked_files_.contains(to)) {
      return false;
    }
    return base::CopyFile(from, to);
  }

  bool OnDeleteFile(const base::FilePath& path) {
    if (locked_files_.contains(path)) {
      return false;
    }
    return base::DeleteFile(path);
  }

  BOOL OnMoveFileEx(const wchar_t* existing,
                    const wchar_t* new_name,
                    DWORD flags) {
    if (flags & MOVEFILE_DELAY_UNTIL_REBOOT) {
      EXPECT_EQ(new_name, nullptr);
      ++reboot_delete_count_;
      return TRUE;
    }
    base::FilePath from(existing);
    base::FilePath to(new_name);
    if (locked_files_.contains(from)) {
      locked_files_.erase(from);
      locked_files_.insert(to);
    }
    return ::MoveFileExW(existing, new_name, flags);
  }

 protected:
  base::ScopedTempDir temp_dir_;
  base::FilePath source_dir_;
  base::FilePath install_dir_;
  base::flat_set<base::FilePath> locked_files_;
  int reboot_delete_count_ = 0;
};

TEST_F(FileUtilTest, DeployAndUninstallPayloadFiles_FreshInstallAndCleanup) {
  for (const wchar_t* name : kTestPayloadFiles) {
    ASSERT_TRUE(base::WriteFile(source_dir_.Append(name), "v1_content"));
  }

  EXPECT_TRUE(DeployPayloadFiles(source_dir_, install_dir_, kTestPayloadFiles));
  EXPECT_EQ(scheduled_reboot_delete_count(), 0);

  for (const wchar_t* name : kTestPayloadFiles) {
    std::string content;
    EXPECT_TRUE(base::ReadFileToString(install_dir_.Append(name), &content));
    EXPECT_EQ(content, "v1_content");
  }

  // Verify in-place unlocked update overwrites files without creating .old
  // files or scheduling reboot deletions.
  for (const wchar_t* name : kTestPayloadFiles) {
    ASSERT_TRUE(base::WriteFile(source_dir_.Append(name), "v2_content"));
  }
  EXPECT_TRUE(DeployPayloadFiles(source_dir_, install_dir_, kTestPayloadFiles));
  EXPECT_EQ(scheduled_reboot_delete_count(), 0);
  EXPECT_FALSE(base::PathExists(install_dir_.Append(kTestCoreOld0)));

  for (const wchar_t* name : kTestPayloadFiles) {
    std::string content;
    EXPECT_TRUE(base::ReadFileToString(install_dir_.Append(name), &content));
    EXPECT_EQ(content, "v2_content");
  }

  // Verify DeployPayloadFiles cleans up any pre-existing unlocked .old files.
  base::FilePath stale_old = install_dir_.Append(L"stale_file.exe.old");
  ASSERT_TRUE(base::WriteFile(stale_old, "stale"));
  EXPECT_TRUE(DeployPayloadFiles(source_dir_, install_dir_, kTestPayloadFiles));
  EXPECT_FALSE(base::PathExists(stale_old));

  EXPECT_TRUE(UninstallPayloadFiles(install_dir_, kTestPayloadFiles));
  EXPECT_FALSE(base::DirectoryExists(install_dir_));
}

TEST_F(FileUtilTest, DeployPayloadFiles_MissingSourceFileFails) {
  // Only write the first file, leaving kTestCoreBinary missing in source_dir_.
  ASSERT_TRUE(
      base::WriteFile(source_dir_.Append(kTestHostBinary), "v1_content"));

  EXPECT_FALSE(
      DeployPayloadFiles(source_dir_, install_dir_, kTestPayloadFiles));
}

TEST_F(FileUtilTest, InstallFileWithStagedRename_MissingSourceFileFails) {
  ASSERT_TRUE(base::CreateDirectory(install_dir_));
  base::FilePath missing_src = source_dir_.Append(kTestCoreBinary);
  base::FilePath dst_file = install_dir_.Append(kTestCoreBinary);

  EXPECT_FALSE(InstallFileWithStagedRename(missing_src, dst_file));
  EXPECT_FALSE(base::PathExists(dst_file));
}

TEST_F(FileUtilTest,
       InstallFileWithStagedRename_ExclusiveLockWithoutDeleteShareFails) {
  ASSERT_TRUE(base::CreateDirectory(install_dir_));

  base::FilePath src_file = source_dir_.Append(kTestCoreBinary);
  base::FilePath dst_file = install_dir_.Append(kTestCoreBinary);
  base::FilePath old_file_0 = install_dir_.Append(kTestCoreOld0);

  ASSERT_TRUE(base::WriteFile(src_file, "v2_binary"));
  ASSERT_TRUE(base::WriteFile(dst_file, "v1_binary"));

  // Open `dst_file` with 0 sharing flags (no FILE_SHARE_DELETE). Both CopyFile
  // and MoveFileExW must fail immediately and return false without scheduling
  // reboot deletions.
  base::win::ScopedHandle exclusive_lock(
      ::CreateFileW(dst_file.value().c_str(), GENERIC_READ, /*dwShareMode=*/0,
                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
  ASSERT_TRUE(exclusive_lock.is_valid());

  EXPECT_FALSE(InstallFileWithStagedRename(src_file, dst_file));
  EXPECT_EQ(scheduled_reboot_delete_count(), 0);
  EXPECT_FALSE(base::PathExists(old_file_0));
}

TEST_F(FileUtilTest,
       InstallFileWithStagedRename_LockedFileAndConsecutiveUpdates) {
  ASSERT_TRUE(base::CreateDirectory(install_dir_));

  base::FilePath src_file = source_dir_.Append(kTestCoreBinary);
  base::FilePath dst_file = install_dir_.Append(kTestCoreBinary);
  base::FilePath old_file_0 = install_dir_.Append(kTestCoreOld0);
  base::FilePath old_file_1 = install_dir_.Append(kTestCoreOld1);

  ASSERT_TRUE(base::WriteFile(src_file, "v2_binary"));
  ASSERT_TRUE(base::WriteFile(dst_file, "v1_binary"));

  // Simulate v1 being locked by an active process.
  SimulateFileLocked(dst_file);

  // First update: v1 -> remoting_core.dll.old, v2 -> remoting_core.dll.
  EXPECT_TRUE(InstallFileWithStagedRename(src_file, dst_file));
  EXPECT_EQ(scheduled_reboot_delete_count(), 1);

  std::string dst_content;
  EXPECT_TRUE(base::ReadFileToString(dst_file, &dst_content));
  EXPECT_EQ(dst_content, "v2_binary");
  EXPECT_TRUE(base::PathExists(old_file_0));

  // Now lock v2 while v1 (remoting_core.dll.old) is STILL locked, and perform
  // a second update (v3).
  SimulateFileLocked(dst_file);
  ASSERT_TRUE(base::WriteFile(src_file, "v3_binary"));

  EXPECT_TRUE(InstallFileWithStagedRename(src_file, dst_file));
  EXPECT_EQ(scheduled_reboot_delete_count(), 2);

  EXPECT_TRUE(base::ReadFileToString(dst_file, &dst_content));
  EXPECT_EQ(dst_content, "v3_binary");
  EXPECT_TRUE(base::PathExists(old_file_0));
  EXPECT_TRUE(base::PathExists(old_file_1));

  // Release only v1's lock and verify CleanupOldFiles deletes old_file_0 while
  // keeping old_file_1 (still locked by v2).
  SimulateFileUnlocked(old_file_0);
  CleanupOldFiles(install_dir_);
  EXPECT_FALSE(base::PathExists(old_file_0));
  EXPECT_TRUE(base::PathExists(old_file_1));

  // Release v2's lock and verify CleanupOldFiles deletes old_file_1.
  SimulateFileUnlocked(old_file_1);
  CleanupOldFiles(install_dir_);
  EXPECT_FALSE(base::PathExists(old_file_1));
}

TEST_F(FileUtilTest, UninstallPayloadFiles_NonExistentDirectorySucceeds) {
  base::FilePath non_existent = temp_dir_.GetPath().Append(L"does_not_exist");
  EXPECT_TRUE(UninstallPayloadFiles(non_existent, kTestPayloadFiles));
}

TEST_F(FileUtilTest, UninstallPayloadFiles_PreservesDirectoryWithExtraFiles) {
  ASSERT_TRUE(
      base::WriteFile(source_dir_.Append(kTestCoreBinary), "v1_binary"));
  ASSERT_TRUE(
      DeployPayloadFiles(source_dir_, install_dir_, kSingleCorePayload));

  // Add an untracked file in `install_dir_`. UninstallPayloadFiles should
  // delete the payload file and schedule `install_dir_` for reboot removal when
  // RemoveDirectoryW returns ERROR_DIR_NOT_EMPTY.
  base::FilePath extra_file = install_dir_.Append(L"extra_file.txt");
  ASSERT_TRUE(base::WriteFile(extra_file, "extra"));

  EXPECT_TRUE(UninstallPayloadFiles(install_dir_, kSingleCorePayload));
  EXPECT_FALSE(base::PathExists(install_dir_.Append(kTestCoreBinary)));
  EXPECT_TRUE(base::PathExists(extra_file));
  EXPECT_EQ(scheduled_reboot_delete_count(), 1);
}

TEST_F(FileUtilTest, UninstallPayloadFiles_LockedFileScheduledForReboot) {
  ASSERT_TRUE(
      base::WriteFile(source_dir_.Append(kTestCoreBinary), "v1_binary"));
  ASSERT_TRUE(
      DeployPayloadFiles(source_dir_, install_dir_, kSingleCorePayload));

  base::FilePath installed_file = install_dir_.Append(kTestCoreBinary);
  base::FilePath old_file = install_dir_.Append(kTestCoreOld0);

  SimulateFileLocked(installed_file);

  EXPECT_TRUE(UninstallPayloadFiles(install_dir_, kSingleCorePayload));
  // Both the renamed `.old` file and `install_dir` should be scheduled for
  // reboot removal.
  EXPECT_EQ(scheduled_reboot_delete_count(), 2);
  EXPECT_FALSE(base::PathExists(installed_file));
  EXPECT_TRUE(base::PathExists(old_file));

  SimulateFileUnlocked(old_file);
}

TEST_F(FileUtilTest,
       UninstallPayloadFiles_ExclusiveLockedFileScheduledForReboot) {
  ASSERT_TRUE(
      base::WriteFile(source_dir_.Append(kTestCoreBinary), "v1_binary"));
  ASSERT_TRUE(
      DeployPayloadFiles(source_dir_, install_dir_, kSingleCorePayload));

  base::FilePath installed_file = install_dir_.Append(kTestCoreBinary);

  // Lock the file exclusively without FILE_SHARE_DELETE so
  // MoveLockedFileToOldPath fails, triggering the fallback to
  // ScheduleDeleteOnReboot(file_path).
  base::File locked_file(installed_file,
                         base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(locked_file.IsValid());

  EXPECT_FALSE(UninstallPayloadFiles(install_dir_, kSingleCorePayload));
  // Both the locked original file and install_dir should be scheduled for
  // reboot removal.
  EXPECT_EQ(scheduled_reboot_delete_count(), 2);
  EXPECT_TRUE(base::PathExists(installed_file));

  locked_file.Close();
}

}  // namespace remoting::installer
