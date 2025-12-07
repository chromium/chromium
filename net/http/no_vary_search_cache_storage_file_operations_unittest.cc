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
#include <tuple>
#include <vector>

// Needed for the #if below it.
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>  // For SetFileAttributes
#endif                // BUILDFLAG(IS_WIN)

#include "base/containers/to_vector.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/insecure_random_generator.h"
#include "base/threading/simple_thread.h"
#include "base/time/time.h"
#include "base/types/optional_util.h"
#include "net/http/no_vary_search_cache_storage.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

using FileOperations = NoVarySearchCacheStorageFileOperations;

using enum base::File::Error;

using ::base::test::ErrorIs;
using ::base::test::HasValue;
using ::base::test::ValueIs;
using ::testing::Bool;
using ::testing::Combine;
using ::testing::Values;
using ::testing::ValuesIn;

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

std::string GetTempFileName() {
  return base::StrCat({NoVarySearchCacheStorage::kSnapshotFilename, "-new"});
}

class FileOperationsTestBase : public ::testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    operations_ = FileOperations::Create(GetNVSPath(), GetLegacyPath());
  }

 protected:
  FileOperations* operations() { return operations_.get(); }

  base::FilePath GetBasePath() const { return dir_.GetPath(); }

  base::FilePath GetLegacyPath() const {
    return GetBasePath().AppendASCII("Cache_Data");
  }

  base::FilePath GetLegacySubdirectoryPath() const {
    return GetLegacyPath().AppendASCII(
        NoVarySearchCacheStorageFileOperations::kLegacyNoVarySearchDirName);
  }

  base::FilePath GetNVSPath() const {
    return GetBasePath().AppendASCII("No_Vary_Search");
  }

 private:
  base::ScopedTempDir dir_;
  std::unique_ptr<FileOperations> operations_;
};

class FileOperationsInitTest : public FileOperationsTestBase {
};

TEST_F(FileOperationsInitTest, InitCreateNoVarySearchDir) {
  auto nvs_path = GetNVSPath();
  EXPECT_FALSE(base::PathExists(nvs_path));
  EXPECT_TRUE(operations()->Init());
  EXPECT_TRUE(base::DirectoryExists(nvs_path));
}

TEST_F(FileOperationsInitTest, InitFailsIfNoVarySearchDirIsFile) {
  auto nvs_path = GetNVSPath();
  ASSERT_TRUE(base::WriteFile(nvs_path, ""));
  EXPECT_FALSE(operations()->Init());
  EXPECT_TRUE(base::PathExists(nvs_path));
  EXPECT_FALSE(base::DirectoryExists(nvs_path));
}

class FileOperationsInitWithNVSDirTest : public FileOperationsInitTest {
 public:
  static constexpr std::string_view kSnapshot =
      NoVarySearchCacheStorage::kSnapshotFilename;
  static constexpr std::string_view kJournal =
      NoVarySearchCacheStorage::kJournalFilename;
  static constexpr std::string_view kExpected = "expected";
  static constexpr std::string_view kUnexpected = "unexpected";

  void SetUp() override {
    FileOperationsInitTest::SetUp();
    ASSERT_TRUE(base::CreateDirectory(GetNVSPath()));
  }

  bool Init() { return operations()->Init(); }

  void ExpectContents(const base::FilePath& path) {
    std::string contents;
    ASSERT_TRUE(base::ReadFileToString(path, &contents));
    EXPECT_EQ(contents, kExpected);
  }

  // Makes a directory at `path` that won't be deleted by base::DeleteFile().
  // This is done by putting a subdirectory inside it.
  void MakeUndeletable(const base::FilePath& path) {
    ASSERT_TRUE(base::CreateDirectory(path));
    ASSERT_TRUE(base::CreateDirectory(path.AppendASCII("subdirectory")));
  }
};

TEST_F(FileOperationsInitWithNVSDirTest,
       InitSucceedsWhenNoVarySearchDirExists) {
  EXPECT_TRUE(Init());
}

TEST_F(FileOperationsInitWithNVSDirTest,
       InitSucceedsWhenUnneededFilesUndeletable) {
  const std::string temp_filename = GetTempFileName();
  const auto filenames =
      std::to_array<std::string_view>({kSnapshot, kJournal, temp_filename});
  for (const base::FilePath& dir :
       {GetLegacyPath(), GetLegacySubdirectoryPath()}) {
    for (const std::string_view filename : filenames) {
      MakeUndeletable(dir.AppendASCII(filename));
    }
  }
  const auto nvs_file_paths =
      base::ToVector(std::to_array<std::string_view>({kSnapshot, kJournal}),
                     [&](std::string_view filename) {
                       return GetNVSPath().AppendASCII(filename);
                     });
  for (const base::FilePath& path : nvs_file_paths) {
    base::WriteFile(path, kExpected);
  }
  EXPECT_TRUE(Init());
  for (const base::FilePath& path : nvs_file_paths) {
    ExpectContents(path);
  }
}

TEST_F(FileOperationsInitWithNVSDirTest, LegacySubdirectoryDeleted) {
  base::CreateDirectory(GetLegacySubdirectoryPath());
  EXPECT_TRUE(Init());
  EXPECT_FALSE(base::PathExists(GetLegacySubdirectoryPath()));
}

enum class Dirs {
  kLegacy,
  kLegacySubdirectory,
  kDedicated,
};

class FileOperationsTempDeleteTest
    : public FileOperationsInitWithNVSDirTest,
      public ::testing::WithParamInterface<Dirs> {
 protected:
  void SetUp() override {
    FileOperationsInitWithNVSDirTest::SetUp();
    // All these tests request the dir to exist, so create it here.
    ASSERT_TRUE(base::CreateDirectory(GetDir()));
  }

  base::FilePath GetDir() const {
    switch (GetParam()) {
      case Dirs::kLegacy:
        return GetLegacyPath();
      case Dirs::kLegacySubdirectory:
        return GetLegacySubdirectoryPath();
      case Dirs::kDedicated:
        return GetNVSPath();
    }
  }

  base::FilePath GetTempFilePath() const {
    return GetDir().AppendASCII(GetTempFileName());
  }
};

INSTANTIATE_TEST_SUITE_P(FileOperations,
                         FileOperationsTempDeleteTest,
                         Values(Dirs::kLegacy,
                                Dirs::kLegacySubdirectory,
                                Dirs::kDedicated),
                         [](const testing::TestParamInfo<Dirs>& info) {
                           switch (info.param) {
                             case Dirs::kLegacy:
                               return "Legacy";
                             case Dirs::kLegacySubdirectory:
                               return "LegacySubdirectory";
                             case Dirs::kDedicated:
                               return "Dedicated";
                           }
                         });

TEST_P(FileOperationsTempDeleteTest, TempSnapshotDeleted) {
  const auto path = GetTempFilePath();
  ASSERT_TRUE(base::WriteFile(path, kUnexpected));
  EXPECT_TRUE(Init());
  EXPECT_FALSE(base::PathExists(path));
}

TEST_P(FileOperationsTempDeleteTest, TempSnapshotUndeletable) {
  const auto path = GetTempFilePath();
  MakeUndeletable(path);
  // This failure is considered harmless, so Init() returns true.
  EXPECT_TRUE(Init());
  // If we can't delete it it isn't deleted, but nothing bad happens.
  EXPECT_TRUE(base::PathExists(path));
}

enum class File {
  kSnapshot,
  kJournal,
};

std::string StringifyParam(File file) {
  return file == File::kSnapshot ? "Snapshot" : "Journal";
}

struct MoveCase {
  bool in_legacy_dir;
  bool in_legacy_subdir;
  bool in_dedicated;
  std::optional<std::string_view> expected_contents;
};

std::string StringifyParam(const MoveCase& move_case) {
  const auto& [dir, subdir, dedicated, contents] = move_case;
  return base::StringPrintf(
      "%c%c%c_%s", dir ? 'L' : '_', subdir ? 'S' : '_', dedicated ? 'D' : '_',
      contents ? std::string(contents.value()).c_str() : "none");
}

constexpr auto kMoveCases = std::to_array<MoveCase>({
    {false, false, false, std::nullopt},
    {false, false, true, "dedicated"},
    {false, true, false, "subdir"},
    {false, true, true, "dedicated"},
    {true, false, false, "legacy"},
    {true, false, true, "dedicated"},
    {true, true, false, "subdir"},
    {true, true, true, "dedicated"},
});

// TODO(https://crbug.com/421927600): Remove this text fixture and tests in
// December 2025 when removing MoveOldFilesIfNeeded() from the implementation.
class FileOperationsMovingTest
    : public FileOperationsInitWithNVSDirTest,
      public ::testing::WithParamInterface<std::tuple<File, MoveCase, bool>> {
 protected:
  File GetFile() const { return std::get<0>(GetParam()); }

  MoveCase GetMoveCase() { return std::get<1>(GetParam()); }

  bool CreateEmptyDirs() { return std::get<2>(GetParam()); }

  std::string_view GetFilename() const {
    return GetFile() == File::kSnapshot ? kSnapshot : kJournal;
  }
};

constexpr auto stringify_param = [](const auto& info) {
  auto [file, move_case, create_empty_dirs] = info.param;
  return StringifyParam(file) + StringifyParam(move_case) +
         (create_empty_dirs ? "_create" : "_nocreate");
};

INSTANTIATE_TEST_SUITE_P(FileOperations,
                         FileOperationsMovingTest,
                         Combine(Values(File::kSnapshot, File::kJournal),
                                 ValuesIn(kMoveCases),
                                 Bool()),
                         stringify_param);

TEST_P(FileOperationsMovingTest, Move) {
  const auto filename = GetFilename();
  const auto [in_legacy_dir, in_legacy_subdir, in_dedicated,
              expected_contents] = GetMoveCase();
  auto write_file_if_true = [&](bool write, const base::FilePath& path,
                                std::string_view contents) {
    if (write || CreateEmptyDirs()) {
      EXPECT_TRUE(base::CreateDirectory(path));
    }
    return write ? base::WriteFile(path.AppendASCII(filename), contents) : true;
  };
  ASSERT_TRUE(write_file_if_true(in_legacy_dir, GetLegacyPath(), "legacy"));
  ASSERT_TRUE(write_file_if_true(in_legacy_subdir, GetLegacySubdirectoryPath(),
                                 "subdir"));
  ASSERT_TRUE(write_file_if_true(in_dedicated, GetNVSPath(), "dedicated"));
  EXPECT_TRUE(Init());
  auto exists_in = [&](const base::FilePath& path) {
    return base::PathExists(path.AppendASCII(filename));
  };
  EXPECT_FALSE(exists_in(GetLegacyPath()));
  EXPECT_FALSE(exists_in(GetLegacySubdirectoryPath()));
  if (expected_contents) {
    std::string contents;
    ASSERT_TRUE(
        base::ReadFileToString(GetNVSPath().AppendASCII(filename), &contents));
    EXPECT_EQ(contents, expected_contents.value());
  } else {
    EXPECT_FALSE(exists_in(GetNVSPath()));
  }
}

// We need a way to make a path that can't be moved by base::ReplaceFile().
// There doesn't seem to be a common way to do this on POSIX and Windows, so use
// separate implementations.

class [[nodiscard]] ScopedUnmovablePathBase {
 public:
  ScopedUnmovablePathBase(const ScopedUnmovablePathBase&) = delete;
  ScopedUnmovablePathBase& operator=(const ScopedUnmovablePathBase&) = delete;

 protected:
  explicit ScopedUnmovablePathBase(const base::FilePath& path) : path_(path) {}

  ~ScopedUnmovablePathBase() { EXPECT_TRUE(base::DeleteFile(path_)); }

  const base::FilePath& path() { return path_; }

 private:
  const base::FilePath path_;
};

#if BUILDFLAG(IS_POSIX)

class [[nodiscard]] ScopedUnmovablePath : public ScopedUnmovablePathBase {
 public:
  static constexpr bool kSupported = true;

  explicit ScopedUnmovablePath(const base::FilePath& path)
      : ScopedUnmovablePathBase(path) {
    EXPECT_TRUE(base::CreateDirectory(path));
    // An unwritable directory is not movable in POSIX.
    MakeFileUnwritable(path);
  }

  ~ScopedUnmovablePath() {
    using enum base::FilePermissionBits;
    // This is permitted to fail if the directory has been deleted.
    base::SetPosixFilePermissions(path(), FILE_PERMISSION_READ_BY_USER |
                                              FILE_PERMISSION_WRITE_BY_USER |
                                              FILE_PERMISSION_EXECUTE_BY_USER);
  }
};

#elif BUILDFLAG(IS_WIN)

class [[nodiscard]] ScopedUnmovablePath : public ScopedUnmovablePathBase {
 public:
  using enum base::File::Flags;

  static constexpr bool kSupported = true;

  // An open file is not movable on Windows.
  explicit ScopedUnmovablePath(const base::FilePath& path)
      : ScopedUnmovablePathBase(path),
        handle_(path,
                FLAG_CREATE_ALWAYS | FLAG_WRITE | FLAG_WIN_EXCLUSIVE_WRITE) {}

  // This closes the handle, enabling the file to be deleted by the base class
  // destructor.
  ~ScopedUnmovablePath() = default;

 private:
  base::File handle_;
};

#else  // !BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_WIN)

// Currently this isn't supported on Fuchsia.
class [[nodiscard]] ScopedUnmovablePath : public ScopedUnmovablePathBase {
 public:
  static constexpr bool kSupported = false;

  explicit ScopedUnmovablePath(const base::FilePath& path)
      : ScopedUnmovablePathBase(path) {
    NOTREACHED();
  }

  ~ScopedUnmovablePath() = default;
};

#endif  // BUILDFLAG(IS_POSIX)

TEST_P(FileOperationsMovingTest, NoErrorOnUnmovableFile) {
  if constexpr (!ScopedUnmovablePath::kSupported) {
    GTEST_SKIP() << "Don't know how to make a path unmovable on this platform";
  }

  const auto filename = GetFilename();
  const auto [in_legacy_dir, in_legacy_subdir, in_dedicated, _] = GetMoveCase();
  const auto emplace_if_true =
      [&](bool emplace, const base::FilePath& in_directory,
          std::optional<ScopedUnmovablePath>& maybe_unmovable) {
        if (emplace) {
          EXPECT_TRUE(base::CreateDirectory(in_directory));
          maybe_unmovable.emplace(in_directory.AppendASCII(filename));
        }
      };
  std::optional<ScopedUnmovablePath> maybe_in_legacy_dir;
  emplace_if_true(in_legacy_dir, GetLegacyPath(), maybe_in_legacy_dir);
  std::optional<ScopedUnmovablePath> maybe_in_legacy_subdir;
  emplace_if_true(in_legacy_subdir, GetLegacySubdirectoryPath(),
                  maybe_in_legacy_subdir);
  const base::FilePath dedicated_filepath = GetNVSPath().AppendASCII(filename);
  if (in_dedicated) {
    EXPECT_TRUE(base::CreateDirectory(GetNVSPath()));
    base::WriteFile(dedicated_filepath, kExpected);
  }
  // This failure is considered harmless, so Init() returns true.
  EXPECT_TRUE(Init());
  if (in_dedicated) {
    ExpectContents(dedicated_filepath);
  } else {
    EXPECT_FALSE(base::PathExists(dedicated_filepath));
  }
}

class FileOperationsPostInitTest : public FileOperationsTestBase {
 public:
  void SetUp() override {
    FileOperationsTestBase::SetUp();
    ASSERT_TRUE(operations()->Init());
  }

 protected:
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

  base::FilePath GetPath() const { return GetNVSPath(); }

  base::FilePath GetPath(std::string_view filename) const {
    return GetPath().AppendASCII(filename);
  }
};

TEST_F(FileOperationsPostInitTest, LoadNonExistent) {
  EXPECT_THAT(operations()->Load("non-existent", kBigSize),
              ErrorIs(FILE_ERROR_NOT_FOUND));
}

TEST_F(FileOperationsPostInitTest, LoadExistent) {
  static constexpr char kFilename[] = "existent";
  const auto data = InterestingData();
  EXPECT_TRUE(WriteFile(kFilename, data));
  ASSERT_OK_AND_ASSIGN(auto result, operations()->Load(kFilename, kBigSize));
  EXPECT_EQ(result.contents, data);
}

TEST_F(FileOperationsPostInitTest, LoadModificationTime) {
  static constexpr char kFilename[] = "file.dat";
  EXPECT_TRUE(WriteFile(kFilename, InterestingData()));
  ASSERT_OK_AND_ASSIGN(auto last_modified, GetLastModified(kFilename));
  ASSERT_OK_AND_ASSIGN(auto result, operations()->Load(kFilename, kBigSize));
  EXPECT_EQ(result.last_modified, last_modified);
}

TEST_F(FileOperationsPostInitTest, LoadEmpty) {
  static constexpr char kFilename[] = "empty";
  EXPECT_TRUE(WriteFile(kFilename, base::span<const uint8_t>()));

  ASSERT_OK_AND_ASSIGN(auto result, operations()->Load(kFilename, kBigSize));
  EXPECT_TRUE(result.contents.empty());
}

TEST_F(FileOperationsPostInitTest, LoadTooBig) {
  static constexpr char kFilename[] = "toobig.dat";
  const auto data = InterestingData();
  EXPECT_TRUE(WriteFile(kFilename, data));

  EXPECT_THAT(operations()->Load(kFilename, data.size() - 1),
              ErrorIs(FILE_ERROR_NO_MEMORY));
}

TEST_F(FileOperationsPostInitTest, AtomicSaveEmpty) {
  static constexpr std::string_view kFilename = "empty";
  EXPECT_EQ(operations()->AtomicSave(kFilename, {}), base::ok());
  ASSERT_OK_AND_ASSIGN(auto contents, ReadFile(kFilename));
  EXPECT_TRUE(contents.empty());
}

TEST_F(FileOperationsPostInitTest, AtomicSaveEmptySegments) {
  static constexpr std::string_view kFilename = "segment.dat";
  const auto data = InterestingData();
  const auto [first, second] = base::span(data).split_at(data.size() / 2);
  const auto segments =
      std::to_array<base::span<const uint8_t>>({{}, first, {}, {}, second, {}});
  EXPECT_EQ(operations()->AtomicSave(kFilename, segments), base::ok());
  ExpectContents(kFilename, data);
}

TEST_F(FileOperationsPostInitTest, AtomicSaveOverwrites) {
  static constexpr std::string_view kFilename = "atomic.dat";
  EXPECT_TRUE(WriteFile(kFilename, std::vector<uint8_t>(64u)));
  const auto data = InterestingData();
  EXPECT_EQ(operations()->AtomicSave(kFilename, {data}), base::ok());
  ExpectContents(kFilename, data);
}

TEST_F(FileOperationsPostInitTest, AtomicSaveOverwritesUnwritable) {
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

TEST_F(FileOperationsPostInitTest, AtomicSaveHandlesUnwritableTempFile) {
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
TEST_F(FileOperationsPostInitTest, AtomicSaveRenameRace) {
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

TEST_F(FileOperationsPostInitTest, CreateWriterCreatesFile) {
  static constexpr std::string_view kFilename = "journal.dat";
  auto writer = operations()->CreateWriter(kFilename);
  EXPECT_THAT(writer, HasValue());
  ExpectContents(kFilename, {});
}

TEST_F(FileOperationsPostInitTest, CreateWriterReplacesFile) {
  static constexpr std::string_view kFilename = "journal.dat";
  EXPECT_TRUE(WriteFile(kFilename, std::vector<uint8_t>(256u)));
  auto writer = operations()->CreateWriter(kFilename);
  EXPECT_THAT(writer, HasValue());
  ExpectContents(kFilename, {});
}

TEST_F(FileOperationsPostInitTest, CreateWriterReplacesUnwritableFile) {
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

TEST_F(FileOperationsPostInitTest, WriterWriteWrites) {
  static constexpr std::string_view kFilename = "journal.dat";
  ASSERT_OK_AND_ASSIGN(auto writer, operations()->CreateWriter(kFilename));
  const auto data = InterestingData();
  writer->Write(data);
  ExpectContents(kFilename, data);
}

TEST_F(FileOperationsPostInitTest, WriterWriteAppends) {
  static constexpr std::string_view kFilename = "journal.dat";
  ASSERT_OK_AND_ASSIGN(auto writer, operations()->CreateWriter(kFilename));
  const auto data = InterestingData();
  auto [first, second] = base::span(data).split_at(data.size() / 2);
  writer->Write(first);
  writer->Write(second);
  ExpectContents(kFilename, data);
}

TEST_F(FileOperationsPostInitTest, CreateWriterUpdatesLastModifiedTime) {
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

TEST_F(FileOperationsPostInitTest, VeryLongFilename) {
  // No current system supports 1MB filenames.
  const std::string filename(1024u * 1024u, 'X');
  EXPECT_THAT(operations()->Load(filename, kBigSize),
              ErrorIs(FILE_ERROR_FAILED));
  EXPECT_THAT(operations()->AtomicSave(filename, DummyContents()),
              ErrorIs(FILE_ERROR_FAILED));
  EXPECT_THAT(operations()->CreateWriter(filename), ErrorIs(FILE_ERROR_FAILED));
}

class FileOperationsBadFilenameTest
    : public FileOperationsPostInitTest,
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
