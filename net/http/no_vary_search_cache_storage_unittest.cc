// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/no_vary_search_cache_storage.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/byte_conversions.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/bind_post_task.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/thread_annotations.h"
#include "net/base/features.h"
#include "net/http/no_vary_search_cache.h"
#include "net/http/no_vary_search_cache_storage_file_operations.h"
#include "net/http/no_vary_search_cache_storage_mock_file_operations.h"
#include "net/http/no_vary_search_cache_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

namespace nvs_test = no_vary_search_cache_test_utils;

using ::base::test::ErrorIs;
using ::base::test::RunClosure;
using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrictMock;

constexpr size_t kCacheMaxSize = 5u;

// Reference-counted thread-safe fake filesystem.
class FakeFilesystem final : public base::RefCountedThreadSafe<FakeFilesystem> {
 public:
  using LoadResult = NoVarySearchCacheStorageFileOperations::LoadResult;

  // Use base::MakeRefCounted<FakeFilesystem>() rather than calling this
  // directly.
  FakeFilesystem() = default;

  FakeFilesystem(const FakeFilesystem&) = delete;
  FakeFilesystem& operator=(const FakeFilesystem&) = delete;

  // Returns the contents and last modified time of `filename` if it exists. If
  // it does not exist, returns std::nullopt.
  std::optional<LoadResult> Load(std::string_view filename) {
    base::AutoLock auto_lock(lock_);

    if (auto it = files_.find(filename); it != files_.end()) {
      return it->second;
    }

    return std::nullopt;
  }

  // Creates or overwrites `filename` with `contents`.
  void Store(std::string_view filename, base::span<const uint8_t> contents) {
    base::AutoLock auto_lock(lock_);

    File& file = CreateOrOpen(filename);
    file.contents = base::ToVector(contents);
  }

  // Appends `contents` to `filename`, updating the last modified time.
  // `filename` is created if it doesn't exist.
  void Append(std::string_view filename, base::span<const uint8_t> contents) {
    // If we need to run `append_closure_` as a result of this Append(), we will
    // move it to `closure_to_run` so it can be called with the mutex unlocked.
    base::OnceClosure closure_to_run;
    {
      base::AutoLock auto_lock(lock_);

      File& file = CreateOrOpen(filename);
      auto& stored_contents = file.contents;
      stored_contents.insert(stored_contents.end(), contents.begin(),
                             contents.end());
      if (append_closure_) {
        closure_to_run = std::move(append_closure_);
      }
    }
    if (closure_to_run) {
      std::move(closure_to_run).Run();
    }
  }

  // Erases `filename` if it exists, otherwise does nothing.
  void Erase(std::string_view filename) {
    base::AutoLock auto_lock(lock_);

    if (auto it = files_.find(filename); it != files_.end()) {
      files_.erase(it);
    }
  }

  // Set a Closure to be run the next time an Append operation is performed. See
  // ScopedAppendWaiter, below, for a better API.
  void SetAppendClosure(base::OnceClosure closure) {
    base::AutoLock auto_lock(lock_);

    CHECK(closure.is_null() || append_closure_.is_null());
    append_closure_ = std::move(closure);
  }

 private:
  friend base::RefCountedThreadSafe<FakeFilesystem>;

  using File = LoadResult;

  ~FakeFilesystem() = default;

  File& CreateOrOpen(std::string_view filename)
      EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    auto [it, unused] = files_.try_emplace(std::string(filename));
    std::ignore = unused;
    File& file = it->second;
    file.last_modified = base::Time::Now();
    return file;
  }

  std::map<std::string, File, std::less<>> files_ GUARDED_BY(lock_);
  int expected_appends_ GUARDED_BY(lock_) = 0;
  base::OnceClosure append_closure_ GUARDED_BY(lock_);
  base::Lock lock_;
};

// A convenience class for waiting for an Append() filesystem
// operations to happen. Should be constructed before the operation that will
// trigger the Append.
class ScopedAppendWaiter final {
 public:
  // Should be constructed before taking the action to trigger the appends.
  [[nodiscard]] explicit ScopedAppendWaiter(FakeFilesystem* filesystem)
      : filesystem_(filesystem) {
    filesystem->SetAppendClosure(run_loop_.QuitClosure());
  }

  // Not copyable, movable or assignable.
  ScopedAppendWaiter(const ScopedAppendWaiter&) = delete;
  ScopedAppendWaiter& operator=(const ScopedAppendWaiter&) = delete;

  ~ScopedAppendWaiter() { filesystem_->SetAppendClosure(base::OnceClosure()); }

  // Wait() should be called after taking the action to trigger the appends.
  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;
  raw_ptr<FakeFilesystem> filesystem_;
};

// An implementation of a FileOperations::Writer backed by a FakeFilesystem.
class FakeWriter final : public NoVarySearchCacheStorageFileOperations::Writer {
 public:
  FakeWriter(scoped_refptr<FakeFilesystem> filesystem,
             std::string_view filename)
      : filesystem_(std::move(filesystem)), filename_(filename) {}

  bool Write(base::span<const uint8_t> data) override {
    filesystem_->Append(filename_, data);
    return true;
  }

 private:
  scoped_refptr<FakeFilesystem> filesystem_;
  std::string filename_;
};

// An implementation of FileOperations backed by a FakeFilesystem.
class FakeFileOperations final : public NoVarySearchCacheStorageFileOperations {
 public:
  explicit FakeFileOperations(scoped_refptr<FakeFilesystem> filesystem)
      : filesystem_(std::move(filesystem)) {}

  bool Init() override {
    init_called_ = true;
    return true;
  }

  base::expected<LoadResult, base::File::Error> Load(std::string_view filename,
                                                     size_t max_size) override {
    EXPECT_TRUE(init_called_);
    auto maybe_result = filesystem_->Load(filename);
    if (!maybe_result) {
      return base::unexpected(base::File::FILE_ERROR_NOT_FOUND);
    }
    auto result = std::move(maybe_result.value());
    if (result.contents.size() > max_size) {
      return base::unexpected(base::File::FILE_ERROR_NO_MEMORY);
    }
    return result;
  }

  base::expected<void, base::File::Error> AtomicSave(
      std::string_view filename,
      base::span<const base::span<const uint8_t>> segments) override {
    EXPECT_TRUE(init_called_);
    const size_t total_size =
        std::accumulate(segments.begin(), segments.end(), size_t{0u},
                        [](size_t total, base::span<const uint8_t> inner_span) {
                          return total + inner_span.size();
                        });
    std::vector<uint8_t> contents;
    contents.reserve(total_size);
    for (auto segment : segments) {
      contents.insert(contents.end(), segment.begin(), segment.end());
    }
    CHECK_EQ(contents.size(), total_size);

    filesystem_->Store(filename, contents);

    return base::ok();
  }

  base::expected<std::unique_ptr<Writer>, base::File::Error> CreateWriter(
      std::string_view filename) override {
    EXPECT_TRUE(init_called_);
    filesystem_->Store(filename, {});
    return std::make_unique<FakeWriter>(filesystem_, filename);
  }

 private:
  bool init_called_ = false;
  scoped_refptr<FakeFilesystem> filesystem_;
};

std::string QueryWithIParameter(size_t i) {
  return "i=" + base::NumberToString(i);
}

// Common functionality for all NoVarySearchCacheStorage test fixtures.
class NoVarySearchCacheStorageTestBase : public ::testing::Test {
 public:
  NoVarySearchCacheStorageTestBase() {
    // Always test with cache partitioning enabled, as it is more interesting.
    scoped_feature_list_.InitAndEnableFeature(
        features::kSplitCacheByNetworkIsolationKey);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// A fixture for tests that use a FakeFilesystem.
class NoVarySearchCacheStorageFakeFilesystemTest
    : public NoVarySearchCacheStorageTestBase {
 protected:
  NoVarySearchCacheStorageFakeFilesystemTest()
      : filesystem_(base::MakeRefCounted<FakeFilesystem>()) {
    Reset();
  }

  // Reset() simulates restarting the browser. Everything is discarded except
  // the file system contents.
  void Reset() {
    storage_.emplace();
    cache_ = nullptr;
    base::RunLoop run_loop;
    // This use of base::Unretained() is safe because destroying `storage_` will
    // prevent the callback being called.
    storage_->Load(
        std::make_unique<FakeFileOperations>(filesystem_), kCacheMaxSize,
        base::BindOnce(&NoVarySearchCacheStorageFakeFilesystemTest::OnLoaded,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Resets with an empty filesystem. Useful when multiple cases are tested in a
  // loop, and each iteration needs to mutate the filesystem independently.
  void HardReset() {
    filesystem_ = base::MakeRefCounted<FakeFilesystem>();
    Reset();
  }

  // Inserts `query` with `no_vary_search_value` into the cache, waiting for the
  // journal append to complete.
  void Insert(std::string_view query, std::string_view no_vary_search_value) {
    ScopedAppendWaiter waiter(filesystem_.get());
    nvs_test::Insert(cache(), query, no_vary_search_value);
    waiter.Wait();
  }

  // Checks if `query` exists in the cache.
  bool Exists(std::string_view query) {
    return nvs_test::Exists(cache(), query);
  }

  // Erases `query` from the cache, waiting for the journal append to complete.
  bool Erase(std::string_view query) {
    ScopedAppendWaiter waiter(filesystem_.get());
    const bool erased = nvs_test::Erase(cache(), query);
    waiter.Wait();
    return erased;
  }

  // Gets the current size of the file `filename` in the fake filesystem. It is
  // expected to exist.
  size_t GetSize(std::string_view filename) {
    auto maybe_load_result = filesystem()->Load(filename);
    if (!maybe_load_result.has_value()) {
      ADD_FAILURE() << "'" << filename << "' not found";
      return size_t{0};
    }
    return maybe_load_result->contents.size();
  }

  struct Sizes {
    size_t snapshot = 0u;
    size_t journal = 0u;

    bool operator==(const Sizes&) const = default;
  };

  // Returns the current sizes of both files in the fake filesystem.
  Sizes GetSizes() {
    return {GetSize(NoVarySearchCacheStorage::kSnapshotFilename),
            GetSize(NoVarySearchCacheStorage::kJournalFilename)};
  }

  NoVarySearchCache& cache() { return *cache_; }
  NoVarySearchCacheStorage& storage() { return *storage_; }

  FakeFilesystem* filesystem() { return filesystem_.get(); }

 private:
  void OnLoaded(base::OnceClosure quit_closure,
                NoVarySearchCacheStorage::LoadResult result) {
    if (result.has_value()) {
      cache_ = std::move(result.value());
      EXPECT_TRUE(cache_);
    }
    std::move(quit_closure).Run();
  }

  std::unique_ptr<NoVarySearchCache> cache_;
  std::optional<NoVarySearchCacheStorage> storage_;
  scoped_refptr<FakeFilesystem> filesystem_;
};

// Fixture for high-level tests that don't inspect the file system.
using NoVarySearchCacheStorageHighLevelTest =
    NoVarySearchCacheStorageFakeFilesystemTest;

TEST_F(NoVarySearchCacheStorageHighLevelTest, StartWithNoCache) {
  EXPECT_EQ(cache().size(), 0u);
}

TEST_F(NoVarySearchCacheStorageHighLevelTest, StartWithSavedEmptyCache) {
  Reset();
  EXPECT_EQ(cache().size(), 0u);
}

TEST_F(NoVarySearchCacheStorageHighLevelTest, InsertionRestored) {
  Insert("a=b", "key-order");
  Reset();
  EXPECT_TRUE(Exists("a=b"));
}

TEST_F(NoVarySearchCacheStorageHighLevelTest, InsertionRestoredTwice) {
  Insert("a=b", "key-order");
  // The first reset causes the journal to be loaded and written back to the
  // snapshot dump.
  Reset();
  // The second reset then loads that snapshot dump.
  Reset();
  EXPECT_TRUE(Exists("a=b"));
}

TEST_F(NoVarySearchCacheStorageHighLevelTest, InsertThenErase) {
  Insert("a=b", "key-order");
  EXPECT_TRUE(Erase("a=b"));
  Reset();
  EXPECT_FALSE(Exists("a=b"));
  EXPECT_EQ(cache().size(), 0u);
}

TEST_F(NoVarySearchCacheStorageHighLevelTest, InsertResetThenErase) {
  Insert("a=b", "key-order");
  Reset();
  EXPECT_TRUE(Erase("a=b"));
  Reset();
  EXPECT_FALSE(Exists("a=b"));
  EXPECT_EQ(cache().size(), 0u);
}

TEST_F(NoVarySearchCacheStorageHighLevelTest, EvictionOrderIsPreserved) {
  for (size_t i = 0; i < kCacheMaxSize; ++i) {
    Insert(QueryWithIParameter(i), "key-order");
  }
  Reset();
  for (size_t i = 0; i < kCacheMaxSize; ++i) {
    Insert(QueryWithIParameter(i + kCacheMaxSize), "key-order");
    EXPECT_EQ(cache().size(), kCacheMaxSize);
    EXPECT_FALSE(Exists(QueryWithIParameter(i)));
  }
}

TEST_F(NoVarySearchCacheStorageHighLevelTest, NoVarySearchValueIsPreserved) {
  Insert("a=1&b=2", "key-order");
  Insert("c=2&d=3", "params=(\"d\")");
  Insert("e=4&f=3", "params, except=(\"e\")");
  Insert("g=5&h=6&i=7", "params=(\"g\"), key-order");
  Reset();
  EXPECT_EQ(cache().size(), 4u);
  EXPECT_TRUE(Exists("b=2&a=1"));
  EXPECT_TRUE(Exists("c=2"));
  EXPECT_TRUE(Exists("e=4&f=9"));
  EXPECT_TRUE(Exists("i=7&h=6"));
}

TEST_F(NoVarySearchCacheStorageHighLevelTest, InsertMoreThanMaxSize) {
  // Insert enough entries that some will have to be evicted while replaying the
  // journal, but not so many as to trigger a new snapshot.
  for (size_t i = 0; i < kCacheMaxSize * 2; ++i) {
    Insert(QueryWithIParameter(i), "key-order");
  }
  Reset();
  EXPECT_EQ(cache().size(), kCacheMaxSize);
  for (size_t i = kCacheMaxSize; i < kCacheMaxSize * 2; ++i) {
    EXPECT_TRUE(Exists(QueryWithIParameter(i)));
  }
}

// The journal should not be replayed if snapshot.baf is missing.
TEST_F(NoVarySearchCacheStorageFakeFilesystemTest, ErasedSnapshot) {
  Insert("a=1", "key-order");
  filesystem()->Erase(NoVarySearchCacheStorage::kSnapshotFilename);
  Reset();
  EXPECT_EQ(cache().size(), 0u);
}

// Test framework for tests that verify behaviour when snapshot.baf is
// corrupted.
class NoVarySearchCacheStorageCorruptSnapshotTest
    : public NoVarySearchCacheStorageFakeFilesystemTest {
 public:
  NoVarySearchCacheStorageCorruptSnapshotTest() = default;

  // Perform common initialization.
  void SetUp() override {
    auto maybe =
        filesystem()->Load(NoVarySearchCacheStorage::kSnapshotFilename);
    ASSERT_TRUE(maybe);
    auto [contents, last_modified] = std::move(maybe.value());
    std::ignore = last_modified;
    contents_ = std::move(contents);
  }

  void Overwrite(base::span<const uint8_t> contents) {
    filesystem()->Store(NoVarySearchCacheStorage::kSnapshotFilename, contents);
  }

  std::vector<uint8_t>& contents() { return contents_; }

 private:
  std::vector<uint8_t> contents_;
};

// The journal should not be replayed if snapshot.baf is truncated.
TEST_F(NoVarySearchCacheStorageCorruptSnapshotTest, Truncated) {
  for (size_t i = 0; i < contents().size(); ++i) {
    SCOPED_TRACE(i);
    Overwrite(base::span(contents()).first(i));
    Insert("a=1", "key-order");
    Reset();
    EXPECT_EQ(cache().size(), 0u);
  }
}

// The journal should not be replayed if snapshot.baf has trailing garbage.
TEST_F(NoVarySearchCacheStorageCorruptSnapshotTest, TrailingGarbage) {
  contents().insert(contents().end(), {0xff, 0xff, 0xff, 0xff});
  Overwrite(contents());
  Insert("a=1", "key-order");
  Reset();
  EXPECT_EQ(cache().size(), 0u);
}

// The journal should not be replayed if snapshot.baf has a bad magic number.
TEST_F(NoVarySearchCacheStorageCorruptSnapshotTest, BadMagic) {
  contents()[0] = ~contents()[0];
  Overwrite(contents());
  Insert("a=1", "key-order");
  Reset();
  EXPECT_EQ(cache().size(), 0u);
}

// If journal.baj is missing, everything else still works.
TEST_F(NoVarySearchCacheStorageFakeFilesystemTest, ErasedJournal) {
  Insert("a=1", "key-order");
  Reset();
  Insert("b=7", "key-order");
  filesystem()->Erase(NoVarySearchCacheStorage::kJournalFilename);
  Reset();
  EXPECT_EQ(cache().size(), 1u);
  EXPECT_TRUE(Exists("a=1"));
}

class NoVarySearchCacheStorageCorruptJournalTest
    : public NoVarySearchCacheStorageFakeFilesystemTest {
 public:
  NoVarySearchCacheStorageCorruptJournalTest() = default;

  void SetUp() override { InitializeCache(); }

  void InitializeCache() {
    HardReset();
    Insert("b=2", "key-order");
    // Move the new entry to snapshot.baf. This will enable us to verify that
    // snapshot.baf is being loaded despite journal corruption.
    Reset();
    // Add an entry to the journal.
    Insert("a=1", "key-order");
    contents_ = Load();
    ASSERT_FALSE(contents_.empty());
  }

  std::vector<uint8_t> Load() {
    auto maybe = filesystem()->Load(NoVarySearchCacheStorage::kJournalFilename);
    if (!maybe) {
      return {};
    }
    auto [contents, last_modified] = std::move(maybe.value());
    std::ignore = last_modified;
    return contents;
  }

  void Overwrite(base::span<const uint8_t> contents) {
    filesystem()->Store(NoVarySearchCacheStorage::kJournalFilename, contents);
  }

  std::vector<uint8_t>& contents() { return contents_; }

 private:
  std::vector<uint8_t> contents_;
};

// When a journal with 1 entry is truncated, the entry is not used.
TEST_F(NoVarySearchCacheStorageCorruptJournalTest, Truncated) {
  for (size_t i = 0; i < contents().size(); ++i) {
    InitializeCache();
    Overwrite(base::span(contents()).first(i));
    Reset();
    EXPECT_EQ(cache().size(), 1u);
    EXPECT_TRUE(Exists("b=2"));
  }
}

// Trailing garbage in the journal is safely ignored.
TEST_F(NoVarySearchCacheStorageCorruptJournalTest, TrailingGarbage) {
  for (size_t i = 0; i < 16u; ++i) {
    InitializeCache();
    auto copy = contents();
    copy.reserve(copy.size() + i);
    for (size_t byte = 0; byte < i; ++byte) {
      copy.push_back(static_cast<uint8_t>(0xff - byte));
    }
    Overwrite(copy);
    Reset();
    EXPECT_EQ(cache().size(), 2u);
    EXPECT_TRUE(Exists("b=2"));
    EXPECT_TRUE(Exists("a=1"));
  }
}

TEST_F(NoVarySearchCacheStorageCorruptJournalTest, BadMagic) {
  contents()[0] = ~contents()[0];
  Overwrite(contents());
  Reset();
  EXPECT_EQ(cache().size(), 1u);
  EXPECT_TRUE(Exists("b=2"));
}

TEST_F(NoVarySearchCacheStorageCorruptJournalTest, BadLengthField) {
  // The length field is the four bytes immediately after the magic number.
  base::span(contents())
      .subspan<4u, 4u>()
      .copy_from(base::U32ToLittleEndian(0xffffffff));
  Overwrite(contents());
  Reset();
  EXPECT_EQ(cache().size(), 1u);
  EXPECT_TRUE(Exists("b=2"));
}

// Responsibility for checking that a zero-length journal entry is detected as
// bad is delegated to base::Pickle, so explicitly check it works.
TEST_F(NoVarySearchCacheStorageCorruptJournalTest, ZeroLengthField) {
  base::span(contents())
      .subspan<4u, 4u>()
      .copy_from(base::U32ToLittleEndian(0u));
  Overwrite(contents());
  Reset();
  EXPECT_EQ(cache().size(), 1u);
  EXPECT_TRUE(Exists("b=2"));
}

// The explicit length field makes it hard to hit many cases of deserialization
// failure. This tests forces deserialization failure by intentionally writing
// short values into the length field.
TEST_F(NoVarySearchCacheStorageCorruptJournalTest, ShortLengthField) {
  auto length_field = base::span(contents()).subspan<4u, 4u>();
  const uint32_t original_length = base::U32FromLittleEndian(length_field);
  for (uint32_t i = 1; i < original_length; ++i) {
    length_field.copy_from(base::U32ToLittleEndian(i));
    Overwrite(contents());
    Reset();
    EXPECT_EQ(cache().size(), 1u);
    EXPECT_TRUE(Exists("b=2"));
  }
}

// We can't hit the case where the JournalEntry type is not one of the known
// types by truncation, so this test explicitly overwrites it.
TEST_F(NoVarySearchCacheStorageCorruptJournalTest, BadJournalEntryType) {
  // The first 16 bytes of the journal:
  // 0-3: Magic number
  // 4-7: Length field for the first entry
  // 8-11: base::Pickle's header for the first entry
  // 12-15: JournalEntryType for the first entry
  auto journal_entry_type_field = base::span(contents()).subspan<12u, 4u>();
  journal_entry_type_field.copy_from(base::U32ToLittleEndian(0xffffffff));
  Overwrite(contents());
  Reset();
  EXPECT_EQ(cache().size(), 1u);
  EXPECT_TRUE(Exists("b=2"));
}

// In the case where the last entry in the journal is truncated, previous
// entries are still used.
TEST_F(NoVarySearchCacheStorageCorruptJournalTest, TruncatedSecondEntry) {
  const size_t initial_size = contents().size();
  Insert("c=3", "key-order");
  auto new_contents = Load();
  ASSERT_LT(initial_size + 2u, new_contents.size());
  for (size_t i = initial_size + 1; i < new_contents.size(); ++i) {
    InitializeCache();
    Insert("c=3", "key-order");
    Overwrite(base::span(new_contents).first(i));
    Reset();
    EXPECT_EQ(cache().size(), 2u);
    EXPECT_TRUE(Exists("b=2"));
    EXPECT_TRUE(Exists("a=1"));
  }
}

TEST_F(NoVarySearchCacheStorageCorruptJournalTest, TruncatedErase) {
  const size_t initial_size = contents().size();
  Erase("a=1");
  auto new_contents = Load();
  ASSERT_LT(initial_size + 2u, new_contents.size());
  for (size_t i = initial_size + 1; i < new_contents.size(); ++i) {
    InitializeCache();
    Insert("c=3", "key-order");
    Overwrite(base::span(new_contents).first(i));
    Reset();
    EXPECT_EQ(cache().size(), 2u);
    EXPECT_TRUE(Exists("b=2"));
    EXPECT_TRUE(Exists("a=1"));
  }
}

// Like the ShortLengthField test, but for a journalled erase.
TEST_F(NoVarySearchCacheStorageCorruptJournalTest, ShortLengthFieldForErase) {
  const size_t initial_size = contents().size();
  Erase("a=1");
  auto new_contents = Load();
  auto length_field =
      base::span(new_contents).subspan(initial_size + 4u).first<4u>();
  const uint32_t original_length = base::U32FromLittleEndian(length_field);
  for (uint32_t i = 1; i < original_length; ++i) {
    length_field.copy_from(base::U32ToLittleEndian(i));
    Overwrite(new_contents);
    Reset();
    EXPECT_EQ(cache().size(), 2u);
    EXPECT_TRUE(Exists("b=2"));
    EXPECT_TRUE(Exists("a=1"));
  }
}

// Any entries after a corrupt entry will be ignored.
TEST_F(NoVarySearchCacheStorageCorruptJournalTest, CorruptedSecondEntry) {
  const size_t initial_size = contents().size();
  Insert("c=3", "key-order");
  const size_t size_after_one_insertion = Load().size();
  ASSERT_LT(initial_size + 8u, size_after_one_insertion);
  Insert("d=z", "key-order");
  auto final_contents = Load();
  ASSERT_LT(size_after_one_insertion + 4u, final_contents.size());
  // Corrupt the Pickle header of the second journal entry, leaving the length
  // field intact so that the following entry could in theory be read.
  base::span(final_contents)
      .subspan(initial_size + 4u, 4u)
      .copy_from(base::I32ToLittleEndian(0xbadcafe));
  Overwrite(final_contents);
  Reset();
  EXPECT_EQ(cache().size(), 2u);
  EXPECT_TRUE(Exists("b=2"));
  EXPECT_TRUE(Exists("a=1"));
}

// If journal.baj is older than snapshot.baf, it is ignored.
TEST_F(NoVarySearchCacheStorageFakeFilesystemTest, JournalTooOld) {
  Insert("a=1", "key-order");
  // Update the timestamp on snapshot.baf by rewriting it.
  task_environment_.FastForwardBy(base::Seconds(1));
  auto maybe = filesystem()->Load(NoVarySearchCacheStorage::kSnapshotFilename);
  ASSERT_TRUE(maybe);
  auto [contents, last_modified] = std::move(maybe.value());
  std::ignore = last_modified;
  filesystem()->Store(NoVarySearchCacheStorage::kSnapshotFilename, contents);

  Reset();
  EXPECT_EQ(cache().size(), 0u);
}

TEST_F(NoVarySearchCacheStorageFakeFilesystemTest, TakeSnapshot) {
  Insert("a=1", "key-order");

  const Sizes sizes_before = GetSizes();

  static constexpr size_t kMagicNumberSize =
      sizeof(NoVarySearchCacheStorage::kJournalMagicNumber);

  EXPECT_GT(sizes_before.journal, kMagicNumberSize);

  ScopedAppendWaiter waiter(filesystem());
  storage().TakeSnapshot();
  waiter.Wait();

  const Sizes sizes_after_snapshot = GetSizes();

  EXPECT_GT(sizes_after_snapshot.snapshot, sizes_before.snapshot);

  // The journal should now be empty except for the magic number.
  EXPECT_EQ(sizes_after_snapshot.journal, kMagicNumberSize);

  Reset();

  EXPECT_EQ(cache().size(), 1u);
  EXPECT_TRUE(Exists("a=1"));

  const Sizes sizes_after_reset = GetSizes();

  EXPECT_EQ(sizes_after_snapshot, sizes_after_reset);
}

TEST_F(NoVarySearchCacheStorageFakeFilesystemTest, AutoSnapshot) {
  // Use a very large query to reduce the number of iterations needed for the
  // journal to reach the size to trigger a snapshot.
  const std::string query = "longparam=" + std::string(4000u, 'x');
  static constexpr size_t kAutoSnapshotSize =
      NoVarySearchCacheStorage::kMinimumAutoSnapshotSize;
  static constexpr size_t kMagicSize =
      sizeof(NoVarySearchCacheStorage::kJournalMagicNumber);
  const size_t max_iterations =
      (kAutoSnapshotSize + query.size() - 1) / query.size();
  Sizes previous_sizes = GetSizes();
  size_t journal_entry_size = 0u;
  for (size_t iteration = 0; iteration < max_iterations; ++iteration) {
    // The duplicate writes still result in new journal entries to update the
    // "update_time" on the entry.
    Insert(query, "key-order");
    Sizes sizes = GetSizes();
    if (sizes.journal > kAutoSnapshotSize) {
      // A snapshot should have been triggered, but hasn't been stored yet. We
      // need to proceed carefully to avoid race conditions.
      ScopedAppendWaiter waiter(filesystem());
      // Any Append() happening on the background thread after this point will
      // be counted by `waiter`.
      const size_t journal_size =
          GetSize(NoVarySearchCacheStorage::kJournalFilename);
      if (journal_size > kAutoSnapshotSize) {
        // The old journal file still existed after `waiter` was created, so the
        // Append() of the magic number to the new journal file had not happened
        // at that time, therefore we can safely wait without deadlocking. This
        // will timeout if the snapshot fails to be triggered due to a bug.
        waiter.Wait();
      } else if (journal_size == 0) {
        // We happened to see the journal file immediately after creation before
        // the magic number could be written to it. We can safely wait for the
        // Append() to happen.
        waiter.Wait();
      }
      sizes = GetSizes();
      EXPECT_EQ(sizes.journal, kMagicSize);
    }
    if (sizes.journal ==
        sizeof(NoVarySearchCacheStorage::kJournalMagicNumber)) {
      EXPECT_LT(previous_sizes.snapshot, sizes.snapshot);
      EXPECT_GT(previous_sizes.journal + journal_entry_size, kAutoSnapshotSize);
      break;
    }
    EXPECT_EQ(previous_sizes.snapshot, sizes.snapshot);
    EXPECT_LT(previous_sizes.journal, sizes.journal);
    if (journal_entry_size == 0) {
      journal_entry_size = sizes.journal - previous_sizes.journal;
    }
    previous_sizes = sizes;
  }
  // Check the loop iterated more than once.
  EXPECT_GT(journal_entry_size, 0u);
}

class NoVarySearchCacheStorageMockFilesystemTest
    : public NoVarySearchCacheStorageTestBase {
 public:
  NoVarySearchCacheStorageMockFilesystemTest()
      : operations_(std::make_unique<StrictMock<MockFileOperations>>()) {}

  // Start the loading process using the MockFileOperations that have been
  // created.
  void TriggerLoad() {
    // This use of base::Unretained is safe because the callback won't be called
    // after `storage_` has been destroyed.
    storage_.Load(std::move(operations_), kCacheMaxSize, future_.GetCallback());
  }

  // Used to register expectations before calling TriggerLoad.
  StrictMock<MockFileOperations>& operations() { return *operations_; }

  NoVarySearchCacheStorage::LoadResult TakeLoadResult() {
    return future_.Take();
  }

  bool IsJournalling() const { return storage_.IsJournallingForTesting(); }

  NoVarySearchCacheStorage& storage() { return storage_; }

 private:
  std::unique_ptr<StrictMock<MockFileOperations>> operations_;
  base::test::TestFuture<NoVarySearchCacheStorage::LoadResult> future_;

  NoVarySearchCacheStorage storage_;
};

TEST_F(NoVarySearchCacheStorageMockFilesystemTest, InitFails) {
  EXPECT_CALL(operations(), Init).WillOnce(Return(false));

  TriggerLoad();

  EXPECT_THAT(TakeLoadResult(),
              ErrorIs(NoVarySearchCacheStorage::LoadFailed::kCannotJournal));
}

TEST_F(NoVarySearchCacheStorageMockFilesystemTest, InitIsCalledFirst) {
  {
    InSequence s;
    EXPECT_CALL(operations(), Init).WillOnce(Return(true));
    EXPECT_CALL(operations(), Load)
        .WillOnce(Return(base::unexpected(base::File::FILE_ERROR_NOT_FOUND)));

    // Cause the load to fail just to keep this test simple.
    EXPECT_CALL(operations(), AtomicSave)
        .WillOnce(Return(base::unexpected(base::File::FILE_ERROR_NO_SPACE)));
  }

  TriggerLoad();

  EXPECT_THAT(TakeLoadResult(),
              ErrorIs(NoVarySearchCacheStorage::LoadFailed::kCannotJournal));
}

class NoVarySearchCacheStorageMockFilesystemInitCalledTest
    : public NoVarySearchCacheStorageMockFilesystemTest {
 public:
  NoVarySearchCacheStorageMockFilesystemInitCalledTest() {
    EXPECT_CALL(operations(), Init).WillOnce(Return(true));
  }
};

TEST_F(NoVarySearchCacheStorageMockFilesystemInitCalledTest,
       WriteSnapshotFails) {
  EXPECT_CALL(operations(), Load)
      .WillOnce(Return(base::unexpected(base::File::FILE_ERROR_NOT_FOUND)));
  EXPECT_CALL(operations(), AtomicSave)
      .WillOnce(Return(base::unexpected(base::File::FILE_ERROR_NO_SPACE)));

  TriggerLoad();

  EXPECT_THAT(TakeLoadResult(),
              ErrorIs(NoVarySearchCacheStorage::LoadFailed::kCannotJournal));
}

TEST_F(NoVarySearchCacheStorageMockFilesystemInitCalledTest,
       CreateJournalFails) {
  EXPECT_CALL(operations(), Load)
      .WillOnce(Return(base::unexpected(base::File::FILE_ERROR_NOT_FOUND)));
  EXPECT_CALL(operations(), AtomicSave).WillOnce(Return(base::ok()));
  EXPECT_CALL(operations(), CreateWriter)
      .WillOnce(Return(base::unexpected(base::File::FILE_ERROR_ACCESS_DENIED)));

  TriggerLoad();

  EXPECT_THAT(TakeLoadResult(),
              ErrorIs(NoVarySearchCacheStorage::LoadFailed::kCannotJournal));
}

TEST_F(NoVarySearchCacheStorageMockFilesystemInitCalledTest,
       StartJournalFails) {
  auto writer = std::make_unique<StrictMock<MockWriter>>();
  EXPECT_CALL(*writer, Write).WillOnce(Return(false));

  EXPECT_CALL(operations(), Load)
      .WillOnce(Return(base::unexpected(base::File::FILE_ERROR_NOT_FOUND)));
  EXPECT_CALL(operations(), AtomicSave).WillOnce(Return(base::ok()));
  EXPECT_CALL(operations(), CreateWriter).WillOnce(Return(std::move(writer)));

  TriggerLoad();

  EXPECT_THAT(TakeLoadResult(),
              ErrorIs(NoVarySearchCacheStorage::LoadFailed::kCannotJournal));
}

TEST_F(NoVarySearchCacheStorageMockFilesystemInitCalledTest,
       JournallingFailsAfterStart) {
  auto writer = std::make_unique<StrictMock<MockWriter>>();

  // If more than two writes are attempted the test will fail.
  EXPECT_CALL(*writer, Write).WillOnce(Return(true)).WillOnce(Return(false));

  EXPECT_CALL(operations(), Load)
      .WillOnce(Return(base::unexpected(base::File::FILE_ERROR_NOT_FOUND)));
  EXPECT_CALL(operations(), AtomicSave).WillOnce(Return(base::ok()));
  EXPECT_CALL(operations(), CreateWriter).WillOnce(Return(std::move(writer)));

  TriggerLoad();

  auto result = TakeLoadResult();
  ASSERT_TRUE(result.has_value());
  std::unique_ptr<NoVarySearchCache> cache = std::move(result.value());
  // Writing this insertion to the journal will fail.
  nvs_test::Insert(*cache, "a=1", "key-order");

  // The task to append this to the journal will be posted, then ignored on the
  // background thread. No write to the journal file will be attempted.
  nvs_test::Insert(*cache, "b=2", "key-order");

  EXPECT_TRUE(base::test::RunUntil([&] { return !IsJournalling(); }));

  // No attempt will be made to journal this.
  nvs_test::Insert(*cache, "c=3", "key-order");

  // Everything still exists in the in-memory cache.
  EXPECT_EQ(cache->size(), 3u);
}

TEST_F(NoVarySearchCacheStorageMockFilesystemInitCalledTest,
       TakeSnapshotFails) {
  auto writer = std::make_unique<StrictMock<MockWriter>>();
  base::RunLoop run_loop;

  EXPECT_CALL(*writer, Write).WillOnce(Return(true));

  EXPECT_CALL(operations(), Load)
      .WillOnce(Return(base::unexpected(base::File::FILE_ERROR_NOT_FOUND)));
  EXPECT_CALL(operations(), AtomicSave)
      .WillOnce(Return(base::ok()))
      .WillOnce(
          DoAll(RunClosure(run_loop.QuitClosure()),
                Return(base::unexpected(base::File::FILE_ERROR_NO_SPACE))));
  EXPECT_CALL(operations(), CreateWriter).WillOnce(Return(std::move(writer)));

  TriggerLoad();

  auto result = TakeLoadResult();
  ASSERT_TRUE(result.has_value());

  storage().TakeSnapshot();

  // This second call to TakeSnapshot() will be ignored on the background thread
  // because the previous one failed.
  storage().TakeSnapshot();

  run_loop.Run();

  EXPECT_TRUE(base::test::RunUntil([&] { return !IsJournalling(); }));

  // This third call will be ignored on this thread.
  storage().TakeSnapshot();
}

}  // namespace

}  // namespace net
