// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/in_memory_file_registry.h"

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory_coordinator/memory_limit.h"
#include "base/memory_coordinator/test_memory_consumer_registry.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/common/file_system/file_system_mount_option.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {

namespace {

using FindResult = InMemoryFileRegistry::FindResult;
using RegisterResult = InMemoryFileRegistry::RegisterResult;

constexpr char kContents[] = "in-memory contents";

// How many bytes of file contents the registry under test holds at the default
// memory limit.
constexpr size_t kMaxTotalBytes = 100;

constexpr char kFindResultHistogram[] =
    "Storage.InMemoryFileRegistry.FindResult";
constexpr char kRegisterResultHistogram[] =
    "Storage.InMemoryFileRegistry.RegisterResult";

base::Time TestTime() {
  return base::Time::UnixEpoch() + base::Days(1);
}

// Whether `filesystem_id` is still registered with IsolatedContext.
bool IsRegisteredWithIsolatedContext(const std::string& filesystem_id) {
  base::FilePath path;
  return IsolatedContext::GetInstance()->GetRegisteredPath(filesystem_id,
                                                           &path);
}

// Whether `<fsid>/<register_name>` of `file` resolves to its file system.
bool CracksToFile(const InMemoryFileRegistry::ScopedFile& file) {
  std::string id_or_name;
  FileSystemType type = kFileSystemTypeUnknown;
  std::string cracked_id;
  base::FilePath cracked_path;
  FileSystemMountOption mount_option;
  return IsolatedContext::GetInstance()->CrackVirtualPath(
             base::FilePath::FromASCII(file.filesystem_id())
                 .AppendASCII(file.register_name()),
             &id_or_name, &type, &cracked_id, &cracked_path, &mount_option) &&
         id_or_name == file.filesystem_id();
}

}  // namespace

class InMemoryFileRegistryTest : public testing::Test {
 protected:
  void SetUp() override {
    registry_ = InMemoryFileRegistry::CreateForTesting(kMaxTotalBytes);
    // The registry registers with the memory coordinator asynchronously.
    ASSERT_TRUE(base::test::RunUntil(
        [&] { return memory_consumer_registry_.size() == 1u; }));
  }

  void TearDown() override {
    registry_.reset();
    // The registry unregisters from the memory coordinator asynchronously.
    ASSERT_TRUE(base::test::RunUntil(
        [&] { return memory_consumer_registry_.size() == 0u; }));
  }

  InMemoryFileRegistry* registry() { return registry_.get(); }

  // Assigns `memory_limit` to the registry, like the memory coordinator does
  // when memory pressure changes.
  void SetMemoryLimit(base::MemoryLimit memory_limit) {
    memory_consumer_registry_.NotifyUpdateMemoryLimit(memory_limit);
    EXPECT_TRUE(base::test::RunUntil(
        [&] { return registry_->memory_limit() == memory_limit; }));
  }

  // Asks the registry to release the memory it holds beyond its memory limit.
  void ReleaseMemory() {
    memory_consumer_registry_.NotifyReleaseMemory();
    // The request reaches the registry through a task that is already posted
    // to this sequence, so it runs before the quit task.
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  // Adds a file to the isolated file system `filesystem_id`, the way Register()
  // adds each file to the file system it registers for it.
  InMemoryFileRegistry::ScopedFile AddFileToFileSystem(
      const std::string& filesystem_id) {
    return registry_->AddFile(IsolatedContext::ScopedFSHandle(filesystem_id),
                              "foo.txt", kContents, TestTime());
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::TestMemoryConsumerRegistry memory_consumer_registry_;
  std::unique_ptr<InMemoryFileRegistry> registry_;
};

TEST_F(InMemoryFileRegistryTest, DefaultScopedFileIsInvalid) {
  InMemoryFileRegistry::ScopedFile file;
  EXPECT_FALSE(file.is_valid());
  EXPECT_TRUE(file.filesystem_id().empty());
  EXPECT_TRUE(file.register_name().empty());
  EXPECT_EQ(0, file.length());
  EXPECT_TRUE(file.modification_time().is_null());
}

TEST_F(InMemoryFileRegistryTest, RegisterAndFind) {
  InMemoryFileRegistry::ScopedFile file =
      registry()->Register("foo.txt", kContents, TestTime());
  ASSERT_TRUE(file.is_valid());
  EXPECT_FALSE(file.filesystem_id().empty());
  EXPECT_EQ("foo.txt", file.register_name());
  EXPECT_EQ(static_cast<int64_t>(std::string(kContents).size()), file.length());
  EXPECT_EQ(TestTime(), file.modification_time());

  scoped_refptr<InMemoryFileRegistry::File> found =
      registry()->Find(file.filesystem_id());
  ASSERT_TRUE(found);
  EXPECT_EQ(kContents, found->contents());
  EXPECT_EQ(TestTime(), found->modification_time());
}

TEST_F(InMemoryFileRegistryTest, RegisterEmptyContents) {
  InMemoryFileRegistry::ScopedFile file =
      registry()->Register("empty", std::string(), TestTime());
  ASSERT_TRUE(file.is_valid());
  EXPECT_EQ(0, file.length());

  scoped_refptr<InMemoryFileRegistry::File> found =
      registry()->Find(file.filesystem_id());
  ASSERT_TRUE(found);
  EXPECT_TRUE(found->contents().empty());
}

TEST_F(InMemoryFileRegistryTest, FindUnknownIdReturnsNull) {
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(registry()->Find("unknown"));
  EXPECT_FALSE(registry()->Find(std::string()));
  histogram_tester.ExpectUniqueSample(kFindResultHistogram,
                                      FindResult::kNotFound, 2);
}

TEST_F(InMemoryFileRegistryTest, RegistersInMemoryIsolatedFileSystem) {
  InMemoryFileRegistry::ScopedFile file =
      registry()->Register("foo.txt", kContents, TestTime());
  ASSERT_TRUE(file.is_valid());
  EXPECT_TRUE(IsRegisteredWithIsolatedContext(file.filesystem_id()));

  // `<fsid>/<register_name>` resolves to an in-memory file system with an
  // empty cracked path, since the file has no path on disk.
  std::string id_or_name;
  FileSystemType type = kFileSystemTypeUnknown;
  std::string cracked_id;
  base::FilePath cracked_path(FILE_PATH_LITERAL("not-cleared"));
  FileSystemMountOption mount_option;
  ASSERT_TRUE(IsolatedContext::GetInstance()->CrackVirtualPath(
      base::FilePath::FromASCII(file.filesystem_id())
          .AppendASCII(file.register_name()),
      &id_or_name, &type, &cracked_id, &cracked_path, &mount_option));
  EXPECT_EQ(file.filesystem_id(), id_or_name);
  EXPECT_EQ(kFileSystemTypeInMemory, type);
  EXPECT_TRUE(cracked_path.empty());

  // Any other name in the file system does not resolve.
  EXPECT_FALSE(IsolatedContext::GetInstance()->CrackVirtualPath(
      base::FilePath::FromASCII(file.filesystem_id()).AppendASCII("other"),
      &id_or_name, &type, &cracked_id, &cracked_path, &mount_option));
}

TEST_F(InMemoryFileRegistryTest, DestroyingScopedFileUnregisters) {
  std::string filesystem_id;
  {
    InMemoryFileRegistry::ScopedFile file =
        registry()->Register("foo.txt", kContents, TestTime());
    ASSERT_TRUE(file.is_valid());
    filesystem_id = file.filesystem_id();
    ASSERT_TRUE(registry()->Find(filesystem_id));
    ASSERT_TRUE(IsRegisteredWithIsolatedContext(filesystem_id));
  }
  EXPECT_FALSE(registry()->Find(filesystem_id));
  EXPECT_FALSE(IsRegisteredWithIsolatedContext(filesystem_id));
}

TEST_F(InMemoryFileRegistryTest, FoundContentsOutliveRegistration) {
  scoped_refptr<InMemoryFileRegistry::File> found;
  {
    InMemoryFileRegistry::ScopedFile file =
        registry()->Register("foo.txt", kContents, TestTime());
    found = registry()->Find(file.filesystem_id());
    ASSERT_TRUE(found);
  }
  // Readers that already looked the file up keep their reference to the bytes.
  EXPECT_EQ(kContents, found->contents());
}

TEST_F(InMemoryFileRegistryTest, EachRegistrationGetsItsOwnFileSystem) {
  InMemoryFileRegistry::ScopedFile file1 =
      registry()->Register("same.txt", "one", TestTime());
  InMemoryFileRegistry::ScopedFile file2 =
      registry()->Register("same.txt", "two", TestTime());
  ASSERT_TRUE(file1.is_valid());
  ASSERT_TRUE(file2.is_valid());
  EXPECT_NE(file1.filesystem_id(), file2.filesystem_id());

  EXPECT_EQ("one", registry()->Find(file1.filesystem_id())->contents());
  EXPECT_EQ("two", registry()->Find(file2.filesystem_id())->contents());

  // Releasing one file leaves the other registered.
  const std::string filesystem_id1 = file1.filesystem_id();
  file1 = InMemoryFileRegistry::ScopedFile();
  EXPECT_FALSE(registry()->Find(filesystem_id1));
  EXPECT_TRUE(registry()->Find(file2.filesystem_id()));
}

TEST_F(InMemoryFileRegistryTest, MoveConstructTransfersOwnership) {
  InMemoryFileRegistry::ScopedFile original =
      registry()->Register("foo.txt", kContents, TestTime());
  const std::string filesystem_id = original.filesystem_id();

  InMemoryFileRegistry::ScopedFile moved(std::move(original));
  EXPECT_FALSE(original.is_valid());  // NOLINT(bugprone-use-after-move)
  EXPECT_EQ(0, original.length());    // NOLINT(bugprone-use-after-move)
  EXPECT_TRUE(original.modification_time()
                  .is_null());  // NOLINT(bugprone-use-after-move)

  ASSERT_TRUE(moved.is_valid());
  EXPECT_EQ(filesystem_id, moved.filesystem_id());
  EXPECT_EQ("foo.txt", moved.register_name());
  EXPECT_EQ(static_cast<int64_t>(std::string(kContents).size()),
            moved.length());
  EXPECT_EQ(TestTime(), moved.modification_time());

  // The file stays registered while the new owner is alive.
  EXPECT_TRUE(registry()->Find(filesystem_id));
  EXPECT_TRUE(IsRegisteredWithIsolatedContext(filesystem_id));

  moved = InMemoryFileRegistry::ScopedFile();
  EXPECT_FALSE(registry()->Find(filesystem_id));
  EXPECT_FALSE(IsRegisteredWithIsolatedContext(filesystem_id));
}

TEST_F(InMemoryFileRegistryTest, MoveAssignReleasesPreviousFile) {
  InMemoryFileRegistry::ScopedFile file1 =
      registry()->Register("one.txt", "one", TestTime());
  InMemoryFileRegistry::ScopedFile file2 =
      registry()->Register("two.txt", "two", TestTime());
  const std::string filesystem_id1 = file1.filesystem_id();
  const std::string filesystem_id2 = file2.filesystem_id();

  file1 = std::move(file2);
  EXPECT_FALSE(file2.is_valid());  // NOLINT(bugprone-use-after-move)

  // The file previously owned by `file1` is released.
  EXPECT_FALSE(registry()->Find(filesystem_id1));
  EXPECT_FALSE(IsRegisteredWithIsolatedContext(filesystem_id1));

  // `file1` now owns the second file.
  ASSERT_TRUE(file1.is_valid());
  EXPECT_EQ(filesystem_id2, file1.filesystem_id());
  EXPECT_EQ("two.txt", file1.register_name());
  EXPECT_TRUE(registry()->Find(filesystem_id2));
  EXPECT_TRUE(IsRegisteredWithIsolatedContext(filesystem_id2));
}

TEST_F(InMemoryFileRegistryTest, NameIsSanitizedToSingleComponent) {
  struct {
    const char* name;
    const char* expected;
  } kCases[] = {
      {"a/b/c.txt", "a_b_c.txt"},
      {"a\\b\\c.txt", "a_b_c.txt"},
      {"../evil.txt", ".._evil.txt"},
      {"/", "_"},
      {"", "file"},
      {".", "file"},
      {"..", "file"},
      {"...", "file"},
      {". .", "file"},
      {".. ", "file"},
      {".hidden", ".hidden"},
      {"archive.tar.gz", "archive.tar.gz"},
  };
  for (const auto& test_case : kCases) {
    SCOPED_TRACE(test_case.name);
    InMemoryFileRegistry::ScopedFile file =
        registry()->Register(test_case.name, kContents, TestTime());
    ASSERT_TRUE(file.is_valid());
    EXPECT_EQ(test_case.expected, file.register_name());
    EXPECT_TRUE(CracksToFile(file));
  }
}

TEST_F(InMemoryFileRegistryTest, RegisterFailsBeyondMemoryLimit) {
  base::HistogramTester histogram_tester;
  InMemoryFileRegistry::ScopedFile file1 = registry()->Register(
      "1", std::string(kMaxTotalBytes - 1, 'a'), TestTime());
  ASSERT_TRUE(file1.is_valid());

  // Two more bytes do not fit.
  EXPECT_FALSE(registry()->Register("2", "bb", TestTime()).is_valid());

  // One more byte exactly fills the registry.
  InMemoryFileRegistry::ScopedFile file3 =
      registry()->Register("3", "c", TestTime());
  EXPECT_TRUE(file3.is_valid());

  histogram_tester.ExpectBucketCount(kRegisterResultHistogram,
                                     RegisterResult::kSuccess, 2);
  histogram_tester.ExpectBucketCount(kRegisterResultHistogram,
                                     RegisterResult::kExceedsMemoryLimit, 1);
}

TEST_F(InMemoryFileRegistryTest, UnregisteringFreesMemory) {
  InMemoryFileRegistry::ScopedFile file =
      registry()->Register("1", std::string(kMaxTotalBytes, 'a'), TestTime());
  ASSERT_TRUE(file.is_valid());
  EXPECT_FALSE(registry()->Register("2", "b", TestTime()).is_valid());

  file = InMemoryFileRegistry::ScopedFile();
  EXPECT_TRUE(registry()->Register("2", "b", TestTime()).is_valid());
}

TEST_F(InMemoryFileRegistryTest, LowerMemoryLimitRejectsNewFiles) {
  InMemoryFileRegistry::ScopedFile file = registry()->Register(
      "1", std::string(kMaxTotalBytes / 2, 'a'), TestTime());
  ASSERT_TRUE(file.is_valid());

  // At half the memory limit the registry is full. The file it already holds
  // is kept until it is asked to release memory.
  SetMemoryLimit(base::MemoryLimit::ModeratePressureThreshold());
  EXPECT_FALSE(registry()->Register("2", "b", TestTime()).is_valid());
  EXPECT_TRUE(registry()->Find(file.filesystem_id()));

  // Once the memory pressure is over, there is room again.
  SetMemoryLimit(base::MemoryLimit::Default());
  EXPECT_TRUE(registry()->Register("2", "b", TestTime()).is_valid());
}

TEST_F(InMemoryFileRegistryTest, ReleaseMemoryEvictsOldestFiles) {
  constexpr size_t kSize = kMaxTotalBytes * 3 / 10;
  InMemoryFileRegistry::ScopedFile oldest =
      registry()->Register("oldest", std::string(kSize, 'a'), TestTime());
  InMemoryFileRegistry::ScopedFile middle =
      registry()->Register("middle", std::string(kSize, 'b'), TestTime());
  InMemoryFileRegistry::ScopedFile newest =
      registry()->Register("newest", std::string(kSize, 'c'), TestTime());
  ASSERT_TRUE(oldest.is_valid());
  ASSERT_TRUE(middle.is_valid());
  ASSERT_TRUE(newest.is_valid());
  scoped_refptr<InMemoryFileRegistry::File> in_flight =
      registry()->Find(oldest.filesystem_id());
  ASSERT_TRUE(in_flight);

  // Getting under half the memory limit takes evicting the two oldest files.
  SetMemoryLimit(base::MemoryLimit::ModeratePressureThreshold());
  ReleaseMemory();

  base::HistogramTester histogram_tester;
  EXPECT_FALSE(registry()->Find(oldest.filesystem_id()));
  EXPECT_FALSE(registry()->Find(middle.filesystem_id()));
  EXPECT_TRUE(registry()->Find(newest.filesystem_id()));
  histogram_tester.ExpectBucketCount(kFindResultHistogram, FindResult::kEvicted,
                                     2);
  histogram_tester.ExpectBucketCount(kFindResultHistogram, FindResult::kFound,
                                     1);

  // Evicted files stay registered until their handles are destroyed.
  EXPECT_TRUE(oldest.is_valid());
  EXPECT_TRUE(IsRegisteredWithIsolatedContext(oldest.filesystem_id()));

  // Readers that looked a file up before it was evicted keep their reference
  // to the bytes.
  EXPECT_EQ(std::string(kSize, 'a'), in_flight->contents());

  // Evicted files no longer count against the memory limit.
  EXPECT_TRUE(registry()
                  ->Register("new",
                             std::string(kMaxTotalBytes / 2 - kSize, 'd'),
                             TestTime())
                  .is_valid());
}

TEST_F(InMemoryFileRegistryTest, ReleaseMemoryKeepsFilesWithinLimit) {
  InMemoryFileRegistry::ScopedFile file =
      registry()->Register("foo.txt", kContents, TestTime());
  ASSERT_TRUE(file.is_valid());

  ReleaseMemory();
  EXPECT_TRUE(registry()->Find(file.filesystem_id()));
}

TEST_F(InMemoryFileRegistryTest, DestroyingEvictedScopedFileUnregisters) {
  std::string filesystem_id;
  {
    InMemoryFileRegistry::ScopedFile file =
        registry()->Register("foo.txt", kContents, TestTime());
    ASSERT_TRUE(file.is_valid());
    filesystem_id = file.filesystem_id();

    SetMemoryLimit(base::MemoryLimit::CriticalPressureThreshold());
    ReleaseMemory();
    ASSERT_FALSE(registry()->Find(filesystem_id));
  }
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(registry()->Find(filesystem_id));
  histogram_tester.ExpectUniqueSample(kFindResultHistogram,
                                      FindResult::kNotFound, 1);
  EXPECT_FALSE(IsRegisteredWithIsolatedContext(filesystem_id));
}

using InMemoryFileRegistryDeathTest = InMemoryFileRegistryTest;

// Register() never adds a file to a file system that already holds one, since
// IsolatedContext does not hand out the id of a registered file system.
TEST_F(InMemoryFileRegistryDeathTest, AddingFileToFileSystemInUseCrashes) {
  InMemoryFileRegistry::ScopedFile file =
      registry()->Register("foo.txt", kContents, TestTime());
  ASSERT_TRUE(file.is_valid());

  // Rather than keeping the existing file while counting the bytes of the new
  // one, the registry crashes.
  EXPECT_CHECK_DEATH(AddFileToFileSystem(file.filesystem_id()));
}

}  // namespace storage
