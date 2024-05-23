// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/disk_cache/simple/simple_index_file.h"

#include <memory>

#include "base/check.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/hash/hash.h"
#include "base/location.h"
#include "base/pickle.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "net/base/cache_type.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/backend_cleanup_tracker.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "net/disk_cache/simple/simple_backend_impl.h"
#include "net/disk_cache/simple/simple_backend_version.h"
#include "net/disk_cache/simple/simple_entry_format.h"
#include "net/disk_cache/simple/simple_index.h"
#include "net/disk_cache/simple/simple_util.h"
#include "net/disk_cache/simple/simple_version_upgrade.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsOk;

using base::Time;
using disk_cache::SimpleIndexFile;
using disk_cache::SimpleIndex;

namespace disk_cache {

namespace {

uint32_t RoundSize(uint32_t in) {
  return (in + 0xFFu) & 0xFFFFFF00u;
}

}  // namespace

TEST(IndexMetadataTest, Basics) {
  SimpleIndexFile::IndexMetadata index_metadata;

  EXPECT_EQ(disk_cache::kSimpleIndexMagicNumber, index_metadata.magic_number_);
  EXPECT_EQ(disk_cache::kSimpleVersion, index_metadata.version_);
  EXPECT_EQ(0U, index_metadata.entry_count());
  EXPECT_EQ(0U, index_metadata.cache_size_);

  // Without setting a |reason_|, the index metadata isn't valid.
  index_metadata.reason_ = SimpleIndex::INDEX_WRITE_REASON_SHUTDOWN;

  EXPECT_TRUE(index_metadata.CheckIndexMetadata());
}

TEST(IndexMetadataTest, Serialize) {
  SimpleIndexFile::IndexMetadata index_metadata(
      SimpleIndex::INDEX_WRITE_REASON_SHUTDOWN, 123, 456);
  base::Pickle pickle;
  index_metadata.Serialize(&pickle);
  base::PickleIterator it(pickle);
  SimpleIndexFile::IndexMetadata new_index_metadata;
  new_index_metadata.Deserialize(&it);

  EXPECT_EQ(new_index_metadata.magic_number_, index_metadata.magic_number_);
  EXPECT_EQ(new_index_metadata.version_, index_metadata.version_);
  EXPECT_EQ(new_index_metadata.reason_, index_metadata.reason_);
  EXPECT_EQ(new_index_metadata.entry_count(), index_metadata.entry_count());
  EXPECT_EQ(new_index_metadata.cache_size_, index_metadata.cache_size_);

  EXPECT_TRUE(new_index_metadata.CheckIndexMetadata());
}

// This derived index metadata class allows us to serialize the older V6 format
// of the index metadata, thus allowing us to test deserializing the old format.
class V6IndexMetadataForTest : public SimpleIndexFile::IndexMetadata {
 public:
  // Do not default to |SimpleIndex::INDEX_WRITE_REASON_MAX|, because we want to
  // ensure we don't serialize that value and then deserialize it and have a
  // false positive result.
  V6IndexMetadataForTest(uint64_t entry_count, uint64_t cache_size)
      : SimpleIndexFile::IndexMetadata(SimpleIndex::INDEX_WRITE_REASON_SHUTDOWN,
                                       entry_count,
                                       cache_size) {
    version_ = 6;
  }

  // Copied and pasted from the V6 implementation of
  // |SimpleIndexFile::IndexMetadata()| (removing DCHECKs).
  void Serialize(base::Pickle* pickle) const override {
    pickle->WriteUInt64(magic_number_);
    pickle->WriteUInt32(version_);
    pickle->WriteUInt64(entry_count_);
    pickle->WriteUInt64(cache_size_);
  }
};

TEST(IndexMetadataTest, ReadV6Format) {
  V6IndexMetadataForTest v6_index_metadata(123, 456);
  EXPECT_EQ(6U, v6_index_metadata.version_);
  base::Pickle pickle;
  v6_index_metadata.Serialize(&pickle);
  base::PickleIterator it(pickle);
  SimpleIndexFile::IndexMetadata new_index_metadata;
  new_index_metadata.Deserialize(&it);

  EXPECT_EQ(new_index_metadata.magic_number_, v6_index_metadata.magic_number_);
  EXPECT_EQ(new_index_metadata.version_, v6_index_metadata.version_);

  EXPECT_EQ(new_index_metadata.reason_, SimpleIndex::INDEX_WRITE_REASON_MAX);
  EXPECT_EQ(new_index_metadata.entry_count(), v6_index_metadata.entry_count());
  EXPECT_EQ(new_index_metadata.cache_size_, v6_index_metadata.cache_size_);

  EXPECT_TRUE(new_index_metadata.CheckIndexMetadata());
}

// This derived index metadata class allows us to serialize the older V7 format
// of the index metadata, thus allowing us to test deserializing the old format.
class V7IndexMetadataForTest : public SimpleIndexFile::IndexMetadata {
 public:
  V7IndexMetadataForTest(uint64_t entry_count, uint64_t cache_size)
      : SimpleIndexFile::IndexMetadata(SimpleIndex::INDEX_WRITE_REASON_SHUTDOWN,
                                       entry_count,
                                       cache_size) {
    version_ = 7;
  }
};

class V8IndexMetadataForTest : public SimpleIndexFile::IndexMetadata {
 public:
  V8IndexMetadataForTest(uint64_t entry_count, uint64_t cache_size)
      : SimpleIndexFile::IndexMetadata(SimpleIndex::INDEX_WRITE_REASON_SHUTDOWN,
                                       entry_count,
                                       cache_size) {
    version_ = 8;
  }
};

// This friend derived class is able to reexport its ancestors private methods
// as public, for use in tests.
class WrappedSimpleIndexFile : public SimpleIndexFile {
 public:
  using SimpleIndexFile::Deserialize;
  using SimpleIndexFile::Serialize;
  using SimpleIndexFile::SerializeFinalData;

  explicit WrappedSimpleIndexFile(const base::FilePath& index_file_directory)
      : SimpleIndexFile(base::SingleThreadTaskRunner::GetCurrentDefault(),
                        base::MakeRefCounted<TrivialFileOperationsFactory>(),
                        net::DISK_CACHE,
                        index_file_directory) {}
  ~WrappedSimpleIndexFile() override = default;

  const base::FilePath& GetIndexFilePath() const {
    return index_file_;
  }

  const base::FilePath& GetTempIndexFilePath() const {
    return temp_index_file_;
  }

  bool CreateIndexFileDirectory() const {
    return base::CreateDirectory(index_file_.DirName());
  }

  static bool LegacyIsIndexFileStale(base::Time cache_last_modified,
                                     const base::FilePath& index_file_path) {
    TrivialFileOperations ops;
    return SimpleIndexFile::LegacyIsIndexFileStale(&ops, cache_last_modified,
                                                   index_file_path);
  }
};

class SimpleIndexFileTest : public net::TestWithTaskEnvironment {
 public:
  bool CompareTwoEntryMetadata(const EntryMetadata& a, const EntryMetadata& b) {
    return a.last_used_time_seconds_since_epoch_ ==
               b.last_used_time_seconds_since_epoch_ &&
           a.entry_size_256b_chunks_ == b.entry_size_256b_chunks_ &&
           a.in_memory_data_ == b.in_memory_data_;
  }

  bool CompareTwoAppCacheEntryMetadata(const EntryMetadata& a,
                                       const EntryMetadata& b) {
    return a.trailer_prefetch_size_ == b.trailer_prefetch_size_ &&
           a.entry_size_256b_chunks_ == b.entry_size_256b_chunks_ &&
           a.in_memory_data_ == b.in_memory_data_;
  }
};

TEST_F(SimpleIndexFileTest, Serialize) {
  SimpleIndex::EntrySet entries;
  static const uint64_t kHashes[] = {11, 22, 33};
  static const size_t kNumHashes = std::size(kHashes);
  EntryMetadata metadata_entries[kNumHashes];

  SimpleIndexFile::IndexMetadata index_metadata(
      SimpleIndex::INDEX_WRITE_REASON_SHUTDOWN,
      static_cast<uint64_t>(kNumHashes), 456);
  for (size_t i = 0; i < kNumHashes; ++i) {
    uint64_t hash = kHashes[i];
    // TODO(eroman): Should restructure the test so no casting here (and same
    //               elsewhere where a hash is cast to an entry size).
    metadata_entries[i] = EntryMetadata(Time(), static_cast<uint32_t>(hash));
    metadata_entries[i].SetInMemoryData(static_cast<uint8_t>(i));
    SimpleIndex::InsertInEntrySet(hash, metadata_entries[i], &entries);
  }

  std::unique_ptr<base::Pickle> pickle = WrappedSimpleIndexFile::Serialize(
      net::DISK_CACHE, index_metadata, entries);
  EXPECT_TRUE(pickle.get() != nullptr);
  base::Time now = base::Time::Now();
  WrappedSimpleIndexFile::SerializeFinalData(now, pickle.get());
  base::Time when_index_last_saw_cache;
  SimpleIndexLoadResult deserialize_result;
  WrappedSimpleIndexFile::Deserialize(
      net::DISK_CACHE, pickle->data_as_char(), pickle->size(),
      &when_index_last_saw_cache, &deserialize_result);
  EXPECT_TRUE(deserialize_result.did_load);
  EXPECT_EQ(now, when_index_last_saw_cache);
  const SimpleIndex::EntrySet& new_entries = deserialize_result.entries;
  EXPECT_EQ(entries.size(), new_entries.size());

  for (size_t i = 0; i < kNumHashes; ++i) {
    auto it = new_entries.find(kHashes[i]);
    EXPECT_TRUE(new_entries.end() != it);
    EXPECT_TRUE(CompareTwoEntryMetadata(it->second, metadata_entries[i]));
  }
}

TEST_F(SimpleIndexFileTest, SerializeAppCache) {
  SimpleIndex::EntrySet entries;
  static const uint64_t kHashes[] = {11, 22, 33};
  static const size_t kNumHashes = std::size(kHashes);
  static const int32_t kTrailerPrefetches[] = {123, -1, 987};
  EntryMetadata metadata_entries[kNumHashes];

  SimpleIndexFile::IndexMetadata index_metadata(
      SimpleIndex::INDEX_WRITE_REASON_SHUTDOWN,
      static_cast<uint64_t>(kNumHashes), 456);
  for (size_t i = 0; i < kNumHashes; ++i) {
    uint64_t hash = kHashes[i];
    metadata_entries[i] =
        EntryMetadata(kTrailerPrefetches[i], static_cast<uint32_t>(hash));
    metadata_entries[i].SetInMemoryData(static_cast<uint8_t>(i));
    SimpleIndex::InsertInEntrySet(hash, metadata_entries[i], &entries);
  }

  std::unique_ptr<base::Pickle> pickle = WrappedSimpleIndexFile::Serialize(
      net::APP_CACHE, index_metadata, entries);
  EXPECT_TRUE(pickle.get() != nullptr);
  base::Time now = base::Time::Now();
  WrappedSimpleIndexFile::SerializeFinalData(now, pickle.get());
  base::Time when_index_last_saw_cache;
  SimpleIndexLoadResult deserialize_result;
  WrappedSimpleIndexFile::Deserialize(
      net::APP_CACHE, pickle->data_as_char(), pickle->size(),
      &when_index_last_saw_cache, &deserialize_result);
  EXPECT_TRUE(deserialize_result.did_load);
  EXPECT_EQ(now, when_index_last_saw_cache);
  const SimpleIndex::EntrySet& new_entries = deserialize_result.entries;
  EXPECT_EQ(entries.size(), new_entries.size());

  for (size_t i = 0; i < kNumHashes; ++i) {
    auto it = new_entries.find(kHashes[i]);
    EXPECT_TRUE(new_entries.end() != it);
    EXPECT_TRUE(
        CompareTwoAppCacheEntryMetadata(it->second, metadata_entries[i]));
  }
}

TEST_F(SimpleIndexFileTest, ReadV7Format) {
  static const uint64_t kHashes[] = {11, 22, 33};
  static const uint32_t kSizes[] = {394, 594, 495940};
  static_assert(std::size(kHashes) == std::size(kSizes),
                "Need same number of hashes and sizes");
  static const size_t kNumHashes = std::size(kHashes);

  V7IndexMetadataForTest v7_metadata(kNumHashes, 100 * 1024 * 1024);

  // We don't have a convenient way of serializing the actual entries in the
  // V7 format, but we can cheat a bit by using the implementation details: if
  // we set the 8 lower bits of size as the memory data, and upper bits
  // as the size, the new serialization will produce what we want.
  SimpleIndex::EntrySet entries;
  for (size_t i = 0; i < kNumHashes; ++i) {
    EntryMetadata entry(Time(), kSizes[i] & 0xFFFFFF00u);
    entry.SetInMemoryData(static_cast<uint8_t>(kSizes[i] & 0xFFu));
    SimpleIndex::InsertInEntrySet(kHashes[i], entry, &entries);
  }
  std::unique_ptr<base::Pickle> pickle =
      WrappedSimpleIndexFile::Serialize(net::DISK_CACHE, v7_metadata, entries);
  ASSERT_TRUE(pickle.get() != nullptr);
  base::Time now = base::Time::Now();
  WrappedSimpleIndexFile::SerializeFinalData(now, pickle.get());

  // Now read it back. We should get the sizes rounded, and 0 for mem entries.
  base::Time when_index_last_saw_cache;
  SimpleIndexLoadResult deserialize_result;
  WrappedSimpleIndexFile::Deserialize(
      net::DISK_CACHE, pickle->data_as_char(), pickle->size(),
      &when_index_last_saw_cache, &deserialize_result);
  EXPECT_TRUE(deserialize_result.did_load);
  EXPECT_EQ(now, when_index_last_saw_cache);
  const SimpleIndex::EntrySet& new_entries = deserialize_result.entries;
  ASSERT_EQ(entries.size(), new_entries.size());
  for (size_t i = 0; i < kNumHashes; ++i) {
    auto it = new_entries.find(kHashes[i]);
    ASSERT_TRUE(new_entries.end() != it);
    EXPECT_EQ(RoundSize(kSizes[i]), it->second.GetEntrySize());
    EXPECT_EQ(0u, it->second.GetInMemoryData());
  }
}

TEST_F(SimpleIndexFileTest, ReadV8Format) {
  static const uint64_t kHashes[] = {11, 22, 33};
  static const uint32_t kSizes[] = {394, 594, 495940};
  static_assert(std::size(kHashes) == std::size(kSizes),
                "Need same number of hashes and sizes");
  static const size_t kNumHashes = std::size(kHashes);

  // V8 to V9 should not make any modifications for non-APP_CACHE modes.
  // Verify that the data is preserved through the migration.
  V8IndexMetadataForTest v8_metadata(kNumHashes, 100 * 1024 * 1024);

  EntryMetadata metadata_entries[kNumHashes];
  SimpleIndex::EntrySet entries;
  for (size_t i = 0; i < kNumHashes; ++i) {
    metadata_entries[i] =
        EntryMetadata(base::Time::Now(), static_cast<uint32_t>(kHashes[i]));
    metadata_entries[i].SetInMemoryData(static_cast<uint8_t>(i));
    SimpleIndex::InsertInEntrySet(kHashes[i], metadata_entries[i], &entries);
  }
  std::unique_ptr<base::Pickle> pickle =
      WrappedSimpleIndexFile::Serialize(net::DISK_CACHE, v8_metadata, entries);
  ASSERT_TRUE(pickle.get() != nullptr);
  base::Time now = base::Time::Now();
  WrappedSimpleIndexFile::SerializeFinalData(now, pickle.get());

  base::Time when_index_last_saw_cache;
  SimpleIndexLoadResult deserialize_result;
  WrappedSimpleIndexFile::Deserialize(
      net::DISK_CACHE, pickle->data_as_char(), pickle->size(),
      &when_index_last_saw_cache, &deserialize_result);
  EXPECT_TRUE(deserialize_result.did_load);
  EXPECT_EQ(now, when_index_last_saw_cache);
  const SimpleIndex::EntrySet& new_entries = deserialize_result.entries;
  ASSERT_EQ(entries.size(), new_entries.size());
  for (size_t i = 0; i < kNumHashes; ++i) {
    auto it = new_entries.find(kHashes[i]);
    ASSERT_TRUE(new_entries.end() != it);
    EXPECT_TRUE(CompareTwoEntryMetadata(it->second, metadata_entries[i]));
  }
}

TEST_F(SimpleIndexFileTest, ReadV8FormatAppCache) {
  static const uint64_t kHashes[] = {11, 22, 33};
  static const uint32_t kSizes[] = {394, 594, 495940};
  static_assert(std::size(kHashes) == std::size(kSizes),
                "Need same number of hashes and sizes");
  static const size_t kNumHashes = std::size(kHashes);

  // To simulate an upgrade from v8 to v9 write out the v8 schema
  // using DISK_CACHE mode.  The read it back in in APP_CACHE mode.
  // The entry access time data should be zeroed to reset it as the
  // new trailer prefetch size.
  V8IndexMetadataForTest v8_metadata(kNumHashes, 100 * 1024 * 1024);

  EntryMetadata metadata_entries[kNumHashes];
  SimpleIndex::EntrySet entries;
  for (size_t i = 0; i < kNumHashes; ++i) {
    metadata_entries[i] =
        EntryMetadata(base::Time::Now(), static_cast<uint32_t>(kHashes[i]));
    metadata_entries[i].SetInMemoryData(static_cast<uint8_t>(i));
    SimpleIndex::InsertInEntrySet(kHashes[i], metadata_entries[i], &entries);
  }
  std::unique_ptr<base::Pickle> pickle =
      WrappedSimpleIndexFile::Serialize(net::DISK_CACHE, v8_metadata, entries);
  ASSERT_TRUE(pickle.get() != nullptr);
  base::Time now = base::Time::Now();
  WrappedSimpleIndexFile::SerializeFinalData(now, pickle.get());

  // Deserialize using APP_CACHE mode.  This should zero out the
  // trailer_prefetch_size_ instead of using the time bits written
  // out previously.
  base::Time when_index_last_saw_cache;
  SimpleIndexLoadResult deserialize_result;
  WrappedSimpleIndexFile::Deserialize(
      net::APP_CACHE, pickle->data_as_char(), pickle->size(),
      &when_index_last_saw_cache, &deserialize_result);
  EXPECT_TRUE(deserialize_result.did_load);
  EXPECT_EQ(now, when_index_last_saw_cache);
  const SimpleIndex::EntrySet& new_entries = deserialize_result.entries;
  ASSERT_EQ(entries.size(), new_entries.size());
  for (size_t i = 0; i < kNumHashes; ++i) {
    auto it = new_entries.find(kHashes[i]);
    ASSERT_TRUE(new_entries.end() != it);
    // The trailer prefetch size should be zeroed.
    EXPECT_NE(metadata_entries[i].trailer_prefetch_size_,
              it->second.trailer_prefetch_size_);
    EXPECT_EQ(0, it->second.trailer_prefetch_size_);
    // Other data should be unaffected.
    EXPECT_EQ(metadata_entries[i].entry_size_256b_chunks_,
              it->second.entry_size_256b_chunks_);
    EXPECT_EQ(metadata_entries[i].in_memory_data_, it->second.in_memory_data_);
  }
}

TEST_F(SimpleIndexFileTest, LegacyIsIndexFileStale) {
  base::ScopedTempDir cache_dir;
  ASSERT_TRUE(cache_dir.CreateUniqueTempDir());
  base::File::Info file_info;
  base::Time cache_mtime;
  const base::FilePath cache_path = cache_dir.GetPath();

  ASSERT_TRUE(base::GetFileInfo(cache_path, &file_info));
  cache_mtime = file_info.last_modified;
  WrappedSimpleIndexFile simple_index_file(cache_path);
  ASSERT_TRUE(simple_index_file.CreateIndexFileDirectory());
  const base::FilePath& index_path = simple_index_file.GetIndexFilePath();
  EXPECT_TRUE(
      WrappedSimpleIndexFile::LegacyIsIndexFileStale(cache_mtime, index_path));
  const std::string kDummyData = "nothing to be seen here";
  EXPECT_TRUE(base::WriteFile(index_path, kDummyData));
  ASSERT_TRUE(base::GetFileInfo(cache_path, &file_info));
  cache_mtime = file_info.last_modified;
  EXPECT_FALSE(
      WrappedSimpleIndexFile::LegacyIsIndexFileStale(cache_mtime, index_path));

  const base::Time past_time = base::Time::Now() - base::Seconds(10);
  EXPECT_TRUE(base::TouchFile(index_path, past_time, past_time));
  EXPECT_TRUE(base::TouchFile(cache_path, past_time, past_time));
  ASSERT_TRUE(base::GetFileInfo(cache_path, &file_info));
  cache_mtime = file_info.last_modified;
  EXPECT_FALSE(
      WrappedSimpleIndexFile::LegacyIsIndexFileStale(cache_mtime, index_path));
  const base::Time even_older = past_time - base::Seconds(10);
  EXPECT_TRUE(base::TouchFile(index_path, even_older, even_older));
  EXPECT_TRUE(
      WrappedSimpleIndexFile::LegacyIsIndexFileStale(cache_mtime, index_path));
}

TEST_F(SimpleIndexFileTest, WriteThenLoadIndex) {
  base::ScopedTempDir cache_dir;
  ASSERT_TRUE(cache_dir.CreateUniqueTempDir());

  SimpleIndex::EntrySet entries;
  static const uint64_t kHashes[] = {11, 22, 33};
  static const size_t kNumHashes = std::size(kHashes);
  EntryMetadata metadata_entries[kNumHashes];
  for (size_t i = 0; i < kNumHashes; ++i) {
    uint64_t hash = kHashes[i];
    metadata_entries[i] = EntryMetadata(Time(), static_cast<uint32_t>(hash));
    SimpleIndex::InsertInEntrySet(hash, metadata_entries[i], &entries);
  }

  const uint64_t kCacheSize = 456U;
  net::TestClosure closure;
  {
    WrappedSimpleIndexFile simple_index_file(cache_dir.GetPath());
    simple_index_file.WriteToDisk(net::DISK_CACHE,
                                  SimpleIndex::INDEX_WRITE_REASON_SHUTDOWN,
                                  entries, kCacheSize, closure.closure());
    closure.WaitForResult();
    EXPECT_TRUE(base::PathExists(simple_index_file.GetIndexFilePath()));
  }

  WrappedSimpleIndexFile simple_index_file(cache_dir.GetPath());
  base::File::Info file_info;
  ASSERT_TRUE(base::GetFileInfo(cache_dir.GetPath(), &file_info));
  base::Time fake_cache_mtime = file_info.last_modified;
  SimpleIndexLoadResult load_index_result;
  simple_index_file.LoadIndexEntries(fake_cache_mtime, closure.closure(),
                                     &load_index_result);
  closure.WaitForResult();

  EXPECT_TRUE(base::PathExists(simple_index_file.GetIndexFilePath()));
  EXPECT_TRUE(load_index_result.did_load);
  EXPECT_FALSE(load_index_result.flush_required);

  EXPECT_EQ(kNumHashes, load_index_result.entries.size());
  for (uint64_t hash : kHashes)
    EXPECT_EQ(1U, load_index_result.entries.count(hash));
}

TEST_F(SimpleIndexFileTest, LoadCorruptIndex) {
  base::ScopedTempDir cache_dir;
  ASSERT_TRUE(cache_dir.CreateUniqueTempDir());

  WrappedSimpleIndexFile simple_index_file(cache_dir.GetPath());
  ASSERT_TRUE(simple_index_file.CreateIndexFileDirectory());
  const base::FilePath& index_path = simple_index_file.GetIndexFilePath();
  const std::string kDummyData = "nothing to be seen here";
  EXPECT_TRUE(base::WriteFile(index_path, kDummyData));
  base::File::Info file_info;
  ASSERT_TRUE(
      base::GetFileInfo(simple_index_file.GetIndexFilePath(), &file_info));
  base::Time fake_cache_mtime = file_info.last_modified;
  EXPECT_FALSE(WrappedSimpleIndexFile::LegacyIsIndexFileStale(fake_cache_mtime,
                                                              index_path));
  SimpleIndexLoadResult load_index_result;
  net::TestClosure closure;
  simple_index_file.LoadIndexEntries(fake_cache_mtime, closure.closure(),
                                     &load_index_result);
  closure.WaitForResult();

  EXPECT_FALSE(base::PathExists(index_path));
  EXPECT_TRUE(load_index_result.did_load);
  EXPECT_TRUE(load_index_result.flush_required);
}

TEST_F(SimpleIndexFileTest, LoadCorruptIndex2) {
  // Variant where the index looks like a pickle, but not one with right
  // header size --- that used to hit a DCHECK on debug builds.
  base::ScopedTempDir cache_dir;
  ASSERT_TRUE(cache_dir.CreateUniqueTempDir());

  WrappedSimpleIndexFile simple_index_file(cache_dir.GetPath());
  ASSERT_TRUE(simple_index_file.CreateIndexFileDirectory());
  const base::FilePath& index_path = simple_index_file.GetIndexFilePath();
  base::Pickle bad_payload;
  bad_payload.WriteString("nothing to be seen here");

  EXPECT_TRUE(base::WriteFile(index_path, bad_payload));
  base::File::Info file_info;
  ASSERT_TRUE(
      base::GetFileInfo(simple_index_file.GetIndexFilePath(), &file_info));
  base::Time fake_cache_mtime = file_info.last_modified;
  EXPECT_FALSE(WrappedSimpleIndexFile::LegacyIsIndexFileStale(fake_cache_mtime,
                                                              index_path));
  SimpleIndexLoadResult load_index_result;
  net::TestClosure closure;
  simple_index_file.LoadIndexEntries(fake_cache_mtime, closure.closure(),
                                     &load_index_result);
  closure.WaitForResult();

  EXPECT_FALSE(base::PathExists(index_path));
  EXPECT_TRUE(load_index_result.did_load);
  EXPECT_TRUE(load_index_result.flush_required);
}

// Tests that after an upgrade the backend has the index file put in place.
TEST_F(SimpleIndexFileTest, SimpleCacheUpgrade) {
  base::ScopedTempDir cache_dir;
  ASSERT_TRUE(cache_dir.CreateUniqueTempDir());
  const base::FilePath cache_path = cache_dir.GetPath();

  // Write an old fake index file.
  base::File file(cache_path.AppendASCII("index"),
                  base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  ASSERT_TRUE(file.IsValid());
  disk_cache::FakeIndexData file_contents;
  file_contents.initial_magic_number = disk_cache::kSimpleInitialMagicNumber;
  file_contents.version = 5;
  int bytes_written = file.Write(0, reinterpret_cast<char*>(&file_contents),
                                 sizeof(file_contents));
  ASSERT_EQ((int)sizeof(file_contents), bytes_written);
  file.Close();

  // Write the index file. The format is incorrect, but for transitioning from
  // v5 it does not matter.
  const std::string index_file_contents("incorrectly serialized data");
  const base::FilePath old_index_file =
      cache_path.AppendASCII("the-real-index");
  ASSERT_TRUE(base::WriteFile(old_index_file, index_file_contents));

  TrivialFileOperations file_operations;
  // Upgrade the cache.
  ASSERT_EQ(disk_cache::UpgradeSimpleCacheOnDisk(&file_operations, cache_path),
            SimpleCacheConsistencyResult::kOK);

  // Create the backend and initiate index flush by destroying the backend.
  scoped_refptr<disk_cache::BackendCleanupTracker> cleanup_tracker =
      disk_cache::BackendCleanupTracker::TryCreate(cache_path,
                                                   base::OnceClosure());
  ASSERT_TRUE(cleanup_tracker != nullptr);

  net::TestClosure post_cleanup;
  cleanup_tracker->AddPostCleanupCallback(post_cleanup.closure());

  auto simple_cache = std::make_unique<disk_cache::SimpleBackendImpl>(
      /*file_operations_factory=*/nullptr, cache_path, cleanup_tracker,
      /*file_tracker=*/nullptr, 0, net::DISK_CACHE,
      /*net_log=*/nullptr);
  net::TestCompletionCallback cb;
  simple_cache->Init(cb.callback());
  EXPECT_THAT(cb.WaitForResult(), IsOk());
  simple_cache->index()->ExecuteWhenReady(cb.callback());
  int rv = cb.WaitForResult();
  EXPECT_THAT(rv, IsOk());
  simple_cache.reset();
  cleanup_tracker = nullptr;

  // The backend flushes the index on destruction; it will run the post-cleanup
  // callback set on the cleanup_tracker once that finishes.
  post_cleanup.WaitForResult();

  // Verify that the index file exists.
  const base::FilePath& index_file_path =
      cache_path.AppendASCII("index-dir").AppendASCII("the-real-index");
  EXPECT_TRUE(base::PathExists(index_file_path));

  // Verify that the version of the index file is correct.
  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(index_file_path, &contents));
  base::Time when_index_last_saw_cache;
  SimpleIndexLoadResult deserialize_result;
  WrappedSimpleIndexFile::Deserialize(
      net::DISK_CACHE, contents.data(), contents.size(),
      &when_index_last_saw_cache, &deserialize_result);
  EXPECT_TRUE(deserialize_result.did_load);
}

TEST_F(SimpleIndexFileTest, OverwritesStaleTempFile) {
  base::ScopedTempDir cache_dir;
  ASSERT_TRUE(cache_dir.CreateUniqueTempDir());
  const base::FilePath cache_path = cache_dir.GetPath();
  WrappedSimpleIndexFile simple_index_file(cache_path);
  ASSERT_TRUE(simple_index_file.CreateIndexFileDirectory());

  // Create an temporary index file.
  const base::FilePath& temp_index_path =
      simple_index_file.GetTempIndexFilePath();
  const std::string kDummyData = "nothing to be seen here";
  EXPECT_TRUE(base::WriteFile(temp_index_path, kDummyData));
  ASSERT_TRUE(base::PathExists(simple_index_file.GetTempIndexFilePath()));

  // Write the index file.
  SimpleIndex::EntrySet entries;
  SimpleIndex::InsertInEntrySet(11, EntryMetadata(Time(), 11u), &entries);
  net::TestClosure closure;
  simple_index_file.WriteToDisk(net::DISK_CACHE,
                                SimpleIndex::INDEX_WRITE_REASON_SHUTDOWN,
                                entries, 120U, closure.closure());
  closure.WaitForResult();

  // Check that the temporary file was deleted and the index file was created.
  EXPECT_FALSE(base::PathExists(simple_index_file.GetTempIndexFilePath()));
  EXPECT_TRUE(base::PathExists(simple_index_file.GetIndexFilePath()));
}

}  // namespace disk_cache
