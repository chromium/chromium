// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/host/persistent_cache_sandboxed_file_factory.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

namespace {

// RAII class to save and restore the current directory.
class ScopedCurrentDirectory {
 public:
  explicit ScopedCurrentDirectory(const base::FilePath& new_current_dir) {
    CHECK(base::GetCurrentDirectory(&original_cwd_));
    CHECK(base::SetCurrentDirectory(new_current_dir));
  }

  ~ScopedCurrentDirectory() { CHECK(base::SetCurrentDirectory(original_cwd_)); }

 private:
  base::FilePath original_cwd_;
};

const base::FilePath::StringType kCacheId = FILE_PATH_LITERAL("123");
const std::string kProduct = "Chrome/1.0.0.0";

std::vector<base::FilePath> GetDirsInDir(const base::FilePath& dir) {
  std::vector<base::FilePath> dirs;
  base::FileEnumerator e(dir, false, base::FileEnumerator::DIRECTORIES);
  for (base::FilePath name = e.Next(); !name.empty(); name = e.Next()) {
    dirs.push_back(name);
  }
  return dirs;
}

}  // namespace

class PersistentCacheSandboxedFileFactoryTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  const base::FilePath& cache_root_path() const { return temp_dir_.GetPath(); }

 protected:
  void FlushMainThreadTasks() {
    auto quit = task_environment_.QuitClosure();

    task_environment_.GetMainThreadTaskRunner()->PostTask(FROM_HERE, quit);
    task_environment_.RunUntilQuit();
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
};

// Test that PersistentCacheSandboxedFileFactory auto delete old cache files
// after a version change.
TEST_F(PersistentCacheSandboxedFileFactoryTest, DeletesStaleFiles) {
  const std::string kOldProduct = kProduct;
  const std::string kNewProduct = "Chrome/2.0.0.0";
  const base::FilePath cache_dir = cache_root_path().Append(kCacheId);

  // Create old, stale cache files.
  auto factory_old = base::MakeRefCounted<PersistentCacheSandboxedFileFactory>(
      cache_root_path(), task_environment_.GetMainThreadTaskRunner());
  auto files_old = factory_old->CreateFiles(kCacheId, kOldProduct);
  ASSERT_TRUE(files_old);
  files_old.reset();  // Close the files.

  // There should be one version directory for the old product.
  ASSERT_EQ(1u, GetDirsInDir(cache_dir).size());
  const base::FilePath old_version_dir = GetDirsInDir(cache_dir)[0];
  EXPECT_TRUE(base::PathExists(old_version_dir));

  // Create a file for a different client ID that should not be deleted.
  const base::FilePath::StringType kOtherCacheId = FILE_PATH_LITERAL("456");
  const base::FilePath other_cache_dir =
      cache_root_path().Append(kOtherCacheId);
  auto factory_other =
      base::MakeRefCounted<PersistentCacheSandboxedFileFactory>(
          cache_root_path(), task_environment_.GetMainThreadTaskRunner());
  auto files_other = factory_other->CreateFiles(kOtherCacheId, kOldProduct);
  ASSERT_TRUE(files_other);
  files_other.reset();  // Close the file.
  ASSERT_EQ(1u, GetDirsInDir(other_cache_dir).size());

  // Wait for any async tasks to complete.
  FlushMainThreadTasks();

  // The old version directory for the first client should still exist.
  EXPECT_TRUE(base::PathExists(old_version_dir));

  // Now, create a factory for the new version. This should trigger the async
  // deletion of the stale files for kCacheId.
  auto factory_new = base::MakeRefCounted<PersistentCacheSandboxedFileFactory>(
      cache_root_path(), task_environment_.GetMainThreadTaskRunner());
  auto files_new = factory_new->CreateFiles(kCacheId, kNewProduct);
  ASSERT_TRUE(files_new);

  // Wait for the async deletion task to complete.
  FlushMainThreadTasks();

  // Verify the stale version for kCacheId is gone.
  EXPECT_FALSE(base::PathExists(old_version_dir));
  EXPECT_EQ(1u, GetDirsInDir(cache_dir).size());

  // Verify the other client's directory is untouched.
  EXPECT_EQ(1u, GetDirsInDir(other_cache_dir).size());

  // Verify the root cache directory still contains two client directories.
  EXPECT_EQ(2u, GetDirsInDir(cache_root_path()).size());
}

TEST_F(PersistentCacheSandboxedFileFactoryTest, ClearFiles) {
  const base::FilePath cache_dir = cache_root_path().Append(kCacheId);

  // Create cache files.
  auto factory = base::MakeRefCounted<PersistentCacheSandboxedFileFactory>(
      cache_root_path(), task_environment_.GetMainThreadTaskRunner());
  auto files = factory->CreateFiles(kCacheId, kProduct);
  ASSERT_TRUE(files);
  files.reset();  // Close the files.

  // Verify a version directory exists.
  ASSERT_EQ(1u, GetDirsInDir(cache_dir).size());

  // Clear the files.
  EXPECT_TRUE(factory->ClearFiles(kCacheId, kProduct));

  // Verify the version directory is gone.
  EXPECT_EQ(0u, GetDirsInDir(cache_dir).size());
}

TEST_F(PersistentCacheSandboxedFileFactoryTest, ClearFilesAsync) {
  const base::FilePath cache_dir = cache_root_path().Append(kCacheId);

  // Create cache files.
  auto factory = base::MakeRefCounted<PersistentCacheSandboxedFileFactory>(
      cache_root_path(), task_environment_.GetMainThreadTaskRunner());
  auto files = factory->CreateFiles(kCacheId, kProduct);
  ASSERT_TRUE(files);
  files.reset();  // Close the files.

  // Verify a version directory exists.
  ASSERT_EQ(1u, GetDirsInDir(cache_dir).size());

  // Clear the files asynchronously.
  bool callback_result = false;
  factory->ClearFilesAsync(
      kCacheId, kProduct,
      base::BindOnce([](bool* result, bool success) { *result = success; },
                     &callback_result)
          .Then(task_environment_.QuitClosure()));

  // Wait for the async deletion task to complete.
  task_environment_.RunUntilQuit();

  EXPECT_TRUE(callback_result);

  // Verify the version directory is gone.
  EXPECT_EQ(0u, GetDirsInDir(cache_dir).size());
}

// Test that the factory correctly uses the current working directory when an
// empty root path is provided.
TEST_F(PersistentCacheSandboxedFileFactoryTest,
       CreateFiles_EmptyRootPath_UsesCurrentDirectory) {
  ScopedCurrentDirectory scoped_cwd(cache_root_path());

  auto factory = base::MakeRefCounted<PersistentCacheSandboxedFileFactory>(
      /*cache_root_dir=*/base::FilePath(),
      task_environment_.GetMainThreadTaskRunner());

  auto files = factory->CreateFiles(kCacheId, kProduct);
  ASSERT_TRUE(files);
  EXPECT_TRUE(files->db_file.IsValid());
  EXPECT_TRUE(files->journal_file.IsValid());

  // Close the files so we can check the paths.
  files->db_file.Close();
  files->journal_file.Close();

  // The factory should have used the current directory.
  EXPECT_TRUE(base::PathExists(cache_root_path().Append(kCacheId)));
}

}  // namespace gpu
