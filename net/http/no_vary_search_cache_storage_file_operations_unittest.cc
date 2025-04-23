// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/no_vary_search_cache_storage_file_operations.h"

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#if BUILDFLAG(IS_WIN)
#include <windows.h>  // For SetFileAttributes
#endif                // BUILDFLAG(IS_WIN)

#include "base/containers/to_vector.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/insecure_random_generator.h"
#include "base/threading/simple_thread.h"
#include "base/time/time.h"
#include "base/types/optional_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

using FileOperations = NoVarySearchCacheStorageFileOperations;

using enum base::File::Error;

using ::base::test::ErrorIs;
using ::base::test::HasValue;
using ::base::test::ValueIs;

constexpr size_t kBigSize = 100 * 1024 * 1024;

// Returns some non-empty contents for a file that can be passed to AtomicSave()
// when the test doesn't care what the contents are.
std::array<base::span<const uint8_t>, 1> DummyContents() {
  static const std::vector<uint8_t> contents(1024u);
  return std::to_array({base::span<const uint8_t>(contents)});
}

// Returns some data that is unlikely to be matched by chance.
std::vector<uint8_t> InterestingData() {
  static const std::vector<uint8_t> data = [] {
    static constexpr size_t kBigDataSize = 111;
    std::vector<uint64_t> big_data;
    big_data.reserve(kBigDataSize);
    base::test::InsecureRandomGenerator gen;
    gen.ReseedForTesting(0xfedcba9876543210);
    for (size_t i = 0; i < kBigDataSize; ++i) {
      big_data.push_back(gen.RandUint64());
    }
    return base::ToVector(base::as_byte_span(big_data));
  }();
  return data;
}

constexpr bool CanMakeFileUnwritable() {
  return BUILDFLAG(IS_WIN) || BUILDFLAG(IS_POSIX);
}

void MakeFileUnwritable(const base::FilePath& path) {
#if BUILDFLAG(IS_POSIX)
  ASSERT_TRUE(
      base::SetPosixFilePermissions(path, base::FILE_PERMISSION_READ_BY_USER));
#elif BUILDFLAG(IS_WIN)
  // It's not safe to assume the current attributes.
  DWORD attrs = ::GetFileAttributes(path.value().c_str());
  ASSERT_NE(INVALID_FILE_ATTRIBUTES, attrs);
  ASSERT_TRUE(::SetFileAttributes(path.value().c_str(),
                                  attrs | FILE_ATTRIBUTE_READONLY));
#else
  NOTREACHED();
#endif  // BUILDFLAG(IS_POSIX)
}

class FileOperationsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    operations_ = FileOperations::Create(dir_.GetPath());
  }

  FileOperations* operations() { return operations_.get(); }

  // We don't use FileOperations for file manipulations needed by the tests,
  // because the implementation might have internally-consistent bugs.

  // Reads `filename` and returns its contents in a vector on success.
  std::optional<std::vector<uint8_t>> ReadFile(std::string_view filename) {
    return base::ReadFileToBytes(GetPath(filename));
  }

  // Expects that file `filename` contains `contents` and causes the test to
  // fail otherwise.
  void ExpectContents(std::string_view filename,
                      base::span<const uint8_t> contents) {
    ASSERT_OK_AND_ASSIGN(auto result, ReadFile(filename));
    EXPECT_EQ(result, contents);
  }

  // Writes `data` to `filename`. Returns true on success.
  bool WriteFile(std::string_view filename, base::span<const uint8_t> data) {
    return base::WriteFile(GetPath(filename), data);
  }

  std::optional<base::Time> GetLastModified(std::string_view filename) {
    base::File::Info info;
    if (base::GetFileInfo(GetPath(filename), &info)) {
      return info.last_modified;
    }
    return std::nullopt;
  }

  base::FilePath GetPath() const { return dir_.GetPath(); }

  base::FilePath GetPath(std::string_view filename) const {
    return GetPath().AppendASCII(filename);
  }

 private:
  base::ScopedTempDir dir_;
  std::unique_ptr<FileOperations> operations_;
};

TEST_F(FileOperationsTest, LoadNonExistent) {
  EXPECT_THAT(operations()->Load("non-existent", kBigSize),
              ErrorIs(FILE_ERROR_NOT_FOUND));
}

TEST_F(FileOperationsTest, LoadExistent) {
  static constexpr char kFilename[] = "existent";
  const auto data = InterestingData();
  EXPECT_TRUE(WriteFile(kFilename, data));
  ASSERT_OK_AND_ASSIGN(auto result, operations()->Load(kFilename, kBigSize));
  EXPECT_EQ(result.contents, data);
}

TEST_F(FileOperationsTest, LoadModificationTime) {
  static constexpr char kFilename[] = "file.dat";
  EXPECT_TRUE(WriteFile(kFilename, InterestingData()));
  ASSERT_OK_AND_ASSIGN(auto last_modified, GetLastModified(kFilename));
  ASSERT_OK_AND_ASSIGN(auto result, operations()->Load(kFilename, kBigSize));
  EXPECT_EQ(result.last_modified, last_modified);
}

TEST_F(FileOperationsTest, LoadEmpty) {
  static constexpr char kFilename[] = "empty";
  EXPECT_TRUE(WriteFile(kFilename, base::span<const uint8_t>()));

  ASSERT_OK_AND_ASSIGN(auto result, operations()->Load(kFilename, kBigSize));
  EXPECT_TRUE(result.contents.empty());
}

TEST_F(FileOperationsTest, LoadTooBig) {
  static constexpr char kFilename[] = "toobig.dat";
  const auto data = InterestingData();
  EXPECT_TRUE(WriteFile(kFilename, data));

  EXPECT_THAT(operations()->Load(kFilename, data.size() - 1),
              ErrorIs(FILE_ERROR_NO_MEMORY));
}

TEST_F(FileOperationsTest, AtomicSaveEmpty) {
  static constexpr std::string_view kFilename = "empty";
  EXPECT_EQ(operations()->AtomicSave(kFilename, {}), base::ok());
  ASSERT_OK_AND_ASSIGN(auto contents, ReadFile(kFilename));
  EXPECT_TRUE(contents.empty());
}

TEST_F(FileOperationsTest, AtomicSaveEmptySegments) {
  static constexpr std::string_view kFilename = "segment.dat";
  const auto data = InterestingData();
  const auto [first, second] = base::span(data).split_at(data.size() / 2);
  const auto segments =
      std::to_array<base::span<const uint8_t>>({{}, first, {}, {}, second, {}});
  EXPECT_EQ(operations()->AtomicSave(kFilename, segments), base::ok());
  ExpectContents(kFilename, data);
}

TEST_F(FileOperationsTest, AtomicSaveOverwrites) {
  static constexpr std::string_view kFilename = "atomic.dat";
  EXPECT_TRUE(WriteFile(kFilename, std::vector<uint8_t>(64u)));
  const auto data = InterestingData();
  EXPECT_EQ(operations()->AtomicSave(kFilename, {data}), base::ok());
  ExpectContents(kFilename, data);
}

TEST_F(FileOperationsTest, AtomicSaveOverwritesUnwritable) {
  if (!CanMakeFileUnwritable()) {
    GTEST_SKIP() << "Don't know how to make a file unwritable on this platform";
  }
  static constexpr std::string_view kFilename = "nowrite.dat";
  EXPECT_TRUE(WriteFile(kFilename, {}));
  MakeFileUnwritable(GetPath(kFilename));
  const auto data = InterestingData();
  EXPECT_EQ(operations()->AtomicSave(kFilename, {data}), base::ok());
  ExpectContents(kFilename, data);
}

TEST_F(FileOperationsTest, AtomicSaveHandlesUnwritableTempFile) {
  if (!CanMakeFileUnwritable()) {
    GTEST_SKIP() << "Don't know how to make a file unwritable on this platform";
  }
  static constexpr std::string_view kFilename = "target.dat";
  static constexpr std::string_view kTempFilename = "target-new.dat";
  EXPECT_TRUE(WriteFile(kTempFilename, {}));
  MakeFileUnwritable(GetPath(kTempFilename));
  const auto data = InterestingData();
  EXPECT_EQ(operations()->AtomicSave(kFilename, {data}), base::ok());
  ExpectContents(kFilename, data);
}

// Tests the case where another file opens the temporary file during an
// AtomicSave operation. This actually happens with virus checkers on Windows.
TEST_F(FileOperationsTest, AtomicSaveRenameRace) {
  static constexpr base::TimeDelta kMaxRacingTime = base::Seconds(1);
  static constexpr std::string_view kFilename = "file.dat";
  static constexpr std::string_view kTempFilename = "file-new.dat";
  static constexpr size_t kNumberOfWrites = 100u;

  // Unfortunately we need to start a thread to achieve the race condition.
  class RacingThread : public base::DelegateSimpleThread::Delegate {
   public:
    RacingThread(const base::FilePath& file_to_open, base::AtomicFlag& flag)
        : path_(file_to_open), flag_(flag) {}

    RacingThread(const RacingThread&) = delete;
    RacingThread& operator=(const RacingThread&) = delete;

    // Defines the code to be executed on the racing thread.
    void Run() override {
      auto start_time = base::TimeTicks::Now();
      while (!file_.IsValid()) {
        using enum base::File::Flags;
        file_.Initialize(path_, FLAG_OPEN | FLAG_READ);
        if (base::TimeTicks::Now() > start_time + kMaxRacingTime) {
          break;
        }
      }
      if (file_.IsValid()) {
        flag_->Set();
      }
    }

   private:
    const base::FilePath path_;
    const base::raw_ref<base::AtomicFlag> flag_;
    base::File file_;
  };

  base::AtomicFlag atomic_flag;
  RacingThread racing_thread(GetPath(kTempFilename), atomic_flag);
  base::DelegateSimpleThread thread(&racing_thread, "Racing thread");
  thread.StartAsync();

  // Make AtomicSave() do a large number of writes to make it more likely that
  // the other thread will manage to win the race.
  std::array<uint8_t, 1> one_write = {65};
  std::vector<base::span<const uint8_t>> many_writes(kNumberOfWrites,
                                                     one_write);

  auto start_time = base::TimeTicks::Now();
  while (base::TimeTicks::Now() <= start_time + kMaxRacingTime) {
    auto result = operations()->AtomicSave(kFilename, many_writes);
    if (atomic_flag.IsSet()) {
#if BUILDFLAG(IS_WIN)
      EXPECT_THAT(result, ErrorIs(base::File::FILE_ERROR_IN_USE));
#else
      EXPECT_THAT(result, HasValue());
#endif  // BUILDFLAG(IS_WIN)
      break;
    }
  }
  // The test passes by default if the racing thread fails to open the file
  // before it can be renamed. Unfortunately that means that if it starts to
  // fail, it may fail flakily.
  thread.Join();
}

TEST_F(FileOperationsTest, CreateWriterCreatesFile) {
  static constexpr std::string_view kFilename = "journal.dat";
  auto writer = operations()->CreateWriter(kFilename);
  EXPECT_THAT(writer, HasValue());
  ExpectContents(kFilename, {});
}

TEST_F(FileOperationsTest, CreateWriterReplacesFile) {
  static constexpr std::string_view kFilename = "journal.dat";
  EXPECT_TRUE(WriteFile(kFilename, std::vector<uint8_t>(256u)));
  auto writer = operations()->CreateWriter(kFilename);
  EXPECT_THAT(writer, HasValue());
  ExpectContents(kFilename, {});
}

TEST_F(FileOperationsTest, CreateWriterReplacesUnwritableFile) {
  if (!CanMakeFileUnwritable()) {
    GTEST_SKIP() << "Don't know how to make a file unwritable on this platform";
  }
  static constexpr std::string_view kFilename = "journal.dat";
  EXPECT_TRUE(WriteFile(kFilename, std::vector<uint8_t>(256u)));
  MakeFileUnwritable(GetPath(kFilename));
  auto writer = operations()->CreateWriter(kFilename);
  EXPECT_THAT(writer, HasValue());
  ExpectContents(kFilename, {});
}

TEST_F(FileOperationsTest, WriterWriteWrites) {
  static constexpr std::string_view kFilename = "journal.dat";
  ASSERT_OK_AND_ASSIGN(auto writer, operations()->CreateWriter(kFilename));
  const auto data = InterestingData();
  writer->Write(data);
  ExpectContents(kFilename, data);
}

TEST_F(FileOperationsTest, WriterWriteAppends) {
  static constexpr std::string_view kFilename = "journal.dat";
  ASSERT_OK_AND_ASSIGN(auto writer, operations()->CreateWriter(kFilename));
  const auto data = InterestingData();
  auto [first, second] = base::span(data).split_at(data.size() / 2);
  writer->Write(first);
  writer->Write(second);
  ExpectContents(kFilename, data);
}

TEST_F(FileOperationsTest, CreateWriterUpdatesLastModifiedTime) {
  static constexpr std::string_view kFilename = "journal.dat";
  static constexpr base::TimeDelta kSleepTime = base::Milliseconds(1);
  EXPECT_TRUE(WriteFile(kFilename, {}));
  // The timestamp resolution is different on different operating systems, so
  // just repeatedly call CreateWriter() until the time increases.
  ASSERT_OK_AND_ASSIGN(auto original_last_modified, GetLastModified(kFilename));
  base::Time new_last_modified;
  do {
    base::PlatformThread::Sleep(kSleepTime);
    ASSERT_THAT(operations()->CreateWriter(kFilename), HasValue());
    ASSERT_TRUE(
        base::OptionalUnwrapTo(GetLastModified(kFilename), new_last_modified));
  } while (new_last_modified == original_last_modified);
  // This may fail if there is a backwards leap second or someone changes the
  // system clock.
  EXPECT_GT(new_last_modified, original_last_modified);
}

TEST_F(FileOperationsTest, VeryLongFilename) {
  // No current system supports 1MB filenames.
  const std::string filename(1024u * 1024u, 'X');
  EXPECT_THAT(operations()->Load(filename, kBigSize),
              ErrorIs(FILE_ERROR_FAILED));
  EXPECT_THAT(operations()->AtomicSave(filename, DummyContents()),
              ErrorIs(FILE_ERROR_FAILED));
  EXPECT_THAT(operations()->CreateWriter(filename), ErrorIs(FILE_ERROR_FAILED));
}

class FileOperationsBadFilenameTest
    : public FileOperationsTest,
      public ::testing::WithParamInterface<std::string_view> {
 protected:
  std::string_view Filename() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(FileOperations,
                         FileOperationsBadFilenameTest,
                         ::testing::Values("foo/bar", ".", "..", "\x80"));

TEST_P(FileOperationsBadFilenameTest, Load) {
  EXPECT_THAT(operations()->Load(Filename(), kBigSize),
              ErrorIs(FILE_ERROR_SECURITY));
}

TEST_P(FileOperationsBadFilenameTest, AtomicSave) {
  EXPECT_THAT(operations()->AtomicSave(Filename(), DummyContents()),
              ErrorIs(FILE_ERROR_SECURITY));
}

TEST_P(FileOperationsBadFilenameTest, CreateWriter) {
  EXPECT_THAT(operations()->CreateWriter(Filename()),
              ErrorIs(FILE_ERROR_SECURITY));
}

}  // namespace

}  // namespace net
