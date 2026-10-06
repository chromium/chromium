// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/installer/win/setup/file_util.h"

#include <windows.h>

#include <array>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr_exclusion.h"
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

// Intercepts MoveFileExW(..., MOVEFILE_DELAY_UNTIL_REBOOT) calls in unit tests
// so tests do not mutate HKLM\...\PendingFileRenameOperations on elevated bots,
// and asserts that MoveFileExW is called with nullptr destination and
// MOVEFILE_DELAY_UNTIL_REBOOT flags.
class ScopedInterceptDeleteOnReboot {
 public:
  ScopedInterceptDeleteOnReboot() {
    SetMoveFileExCallbackForTesting(base::BindRepeating(
        &ScopedInterceptDeleteOnReboot::OnMoveFileEx, base::Unretained(this)));
  }

  ScopedInterceptDeleteOnReboot(const ScopedInterceptDeleteOnReboot&) = delete;
  ScopedInterceptDeleteOnReboot& operator=(
      const ScopedInterceptDeleteOnReboot&) = delete;

  ~ScopedInterceptDeleteOnReboot() {
    SetMoveFileExCallbackForTesting(MoveFileExCallback());
  }

  int scheduled_count() const { return scheduled_count_; }

 private:
  BOOL OnMoveFileEx(const wchar_t* existing,
                    const wchar_t* new_name,
                    DWORD flags) {
    EXPECT_EQ(new_name, nullptr);
    EXPECT_EQ(flags, static_cast<DWORD>(MOVEFILE_DELAY_UNTIL_REBOOT));
    ++scheduled_count_;
    return TRUE;
  }

  int scheduled_count_ = 0;
};

// Simulates the exact Windows kernel lock held by a running executable or
// loaded DLL:
// 1. A file handle opened with FILE_SHARE_READ | FILE_SHARE_DELETE (allowing
//    MoveFileExW rename while preventing CopyFileW overwrite).
// 2. An active memory-mapped view (preventing DeleteFileW from deleting the
//    file on disk).
class ScopedSimulatedLoadedBinary {
 public:
  explicit ScopedSimulatedLoadedBinary(const base::FilePath& path) {
    file_.Set(::CreateFileW(path.value().c_str(), GENERIC_READ,
                            FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file_.is_valid()) {
      PLOG(ERROR) << "Failed to open file for simulated lock: " << path.value();
      return;
    }
    mapping_.Set(::CreateFileMappingW(file_.Get(), nullptr, PAGE_READONLY, 0, 0,
                                      nullptr));
    if (!mapping_.is_valid()) {
      PLOG(ERROR) << "Failed to create file mapping for: " << path.value();
      return;
    }
    view_ = ::MapViewOfFile(mapping_.Get(), FILE_MAP_READ, 0, 0, 0);
    if (!view_) {
      PLOG(ERROR) << "Failed to map view of file for: " << path.value();
    }
  }

  ScopedSimulatedLoadedBinary(const ScopedSimulatedLoadedBinary&) = delete;
  ScopedSimulatedLoadedBinary& operator=(const ScopedSimulatedLoadedBinary&) =
      delete;

  ~ScopedSimulatedLoadedBinary() {
    if (view_) {
      ::UnmapViewOfFile(view_);
      view_ = nullptr;
    }
  }

  bool is_valid() const { return view_ != nullptr; }

 private:
  base::win::ScopedHandle file_;
  base::win::ScopedHandle mapping_;
  // `view_` is an OS memory-mapped section address returned by MapViewOfFile,
  // not a PartitionAlloc heap allocation.
  RAW_PTR_EXCLUSION void* view_ = nullptr;
};

}  // namespace

class FileUtilTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    source_dir_ = temp_dir_.GetPath().Append(L"source");
    install_dir_ = temp_dir_.GetPath().Append(L"install");
    ASSERT_TRUE(base::CreateDirectory(source_dir_));
  }

  ScopedInterceptDeleteOnReboot intercept_delete_on_reboot_;
  base::ScopedTempDir temp_dir_;
  base::FilePath source_dir_;
  base::FilePath install_dir_;
};

TEST_F(FileUtilTest, DeployAndUninstallPayloadFiles_FreshInstallAndCleanup) {
  for (const wchar_t* name : kTestPayloadFiles) {
    ASSERT_TRUE(base::WriteFile(source_dir_.Append(name), "v1_content"));
  }

  EXPECT_TRUE(DeployPayloadFiles(source_dir_, install_dir_, kTestPayloadFiles));
  EXPECT_EQ(intercept_delete_on_reboot_.scheduled_count(), 0);

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
  EXPECT_EQ(intercept_delete_on_reboot_.scheduled_count(), 0);
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
  EXPECT_EQ(intercept_delete_on_reboot_.scheduled_count(), 0);
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

  auto locked_v1 = std::make_unique<ScopedSimulatedLoadedBinary>(dst_file);
  ASSERT_TRUE(locked_v1->is_valid());

  // First update: v1 -> remoting_core.dll.old, v2 -> remoting_core.dll.
  EXPECT_TRUE(InstallFileWithStagedRename(src_file, dst_file));
  EXPECT_EQ(intercept_delete_on_reboot_.scheduled_count(), 1);

  std::string dst_content;
  EXPECT_TRUE(base::ReadFileToString(dst_file, &dst_content));
  EXPECT_EQ(dst_content, "v2_binary");
  EXPECT_TRUE(base::PathExists(old_file_0));

  // Now lock v2 while v1 (remoting_core.dll.old) is STILL locked, and perform
  // a second update (v3).
  auto locked_v2 = std::make_unique<ScopedSimulatedLoadedBinary>(dst_file);
  ASSERT_TRUE(locked_v2->is_valid());
  ASSERT_TRUE(base::WriteFile(src_file, "v3_binary"));

  EXPECT_TRUE(InstallFileWithStagedRename(src_file, dst_file));
  EXPECT_EQ(intercept_delete_on_reboot_.scheduled_count(), 2);

  EXPECT_TRUE(base::ReadFileToString(dst_file, &dst_content));
  EXPECT_EQ(dst_content, "v3_binary");
  EXPECT_TRUE(base::PathExists(old_file_0));
  EXPECT_TRUE(base::PathExists(old_file_1));

  // Release only v1's lock and verify CleanupOldFiles deletes old_file_0 while
  // keeping old_file_1 (still locked by v2).
  locked_v1.reset();
  CleanupOldFiles(install_dir_);
  EXPECT_FALSE(base::PathExists(old_file_0));
  EXPECT_TRUE(base::PathExists(old_file_1));

  // Release v2's lock and verify CleanupOldFiles deletes old_file_1.
  locked_v2.reset();
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
  EXPECT_EQ(intercept_delete_on_reboot_.scheduled_count(), 1);
}

TEST_F(FileUtilTest, UninstallPayloadFiles_LockedFileScheduledForReboot) {
  ASSERT_TRUE(
      base::WriteFile(source_dir_.Append(kTestCoreBinary), "v1_binary"));
  ASSERT_TRUE(
      DeployPayloadFiles(source_dir_, install_dir_, kSingleCorePayload));

  base::FilePath installed_file = install_dir_.Append(kTestCoreBinary);
  base::FilePath old_file = install_dir_.Append(kTestCoreOld0);

  auto locked_file =
      std::make_unique<ScopedSimulatedLoadedBinary>(installed_file);
  ASSERT_TRUE(locked_file->is_valid());

  EXPECT_TRUE(UninstallPayloadFiles(install_dir_, kSingleCorePayload));
  // Both the renamed `.old` file and `install_dir` should be scheduled for
  // reboot removal.
  EXPECT_EQ(intercept_delete_on_reboot_.scheduled_count(), 2);
  EXPECT_FALSE(base::PathExists(installed_file));
  EXPECT_TRUE(base::PathExists(old_file));

  locked_file.reset();
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
  EXPECT_EQ(intercept_delete_on_reboot_.scheduled_count(), 2);
  EXPECT_TRUE(base::PathExists(installed_file));

  locked_file.Close();
}

}  // namespace remoting::installer
