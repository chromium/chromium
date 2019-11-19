// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/simple_index.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/pickle.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "net/base/cache_type.h"
#include "net/disk_cache/backend_cleanup_tracker.h"
#include "net/disk_cache/simple/simple_index_delegate.h"
#include "net/disk_cache/simple/simple_index_file.h"
#include "net/disk_cache/simple/simple_test_util.h"
#include "net/disk_cache/simple/simple_util.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace disk_cache {
namespace {

const base::Time kTestLastUsedTime =
    base::Time::UnixEpoch() + base::TimeDelta::FromDays(20);
const uint32_t kTestEntrySize = 789;
const uint8_t kTestEntryMemoryData = 123;

uint32_t RoundSize(uint32_t in) {
  return (in + 0xFFu) & 0xFFFFFF00u;
}

}  // namespace

class EntryMetadataTest : public testing::Test {
 public:
  EntryMetadata NewEntryMetadataWithValues() {
    EntryMetadata entry(kTestLastUsedTime, kTestEntrySize);
    entry.SetInMemoryData(kTestEntryMemoryData);
    return entry;
  }

  void CheckEntryMetadataValues(const EntryMetadata& entry_metadata) {
    EXPECT_LT(kTestLastUsedTime - base::TimeDelta::FromSeconds(2),
              entry_metadata.GetLastUsedTime());
    EXPECT_GT(kTestLastUsedTime + base::TimeDelta::FromSeconds(2),
              entry_metadata.GetLastUsedTime());
    EXPECT_EQ(RoundSize(kTestEntrySize), entry_metadata.GetEntrySize());
    EXPECT_EQ(kTestEntryMemoryData, entry_metadata.GetInMemoryData());
  }
};

class MockSimpleIndexFile : public SimpleIndexFile,
                            public base::SupportsWeakPtr<MockSimpleIndexFile> {
 public:
  explicit MockSimpleIndexFile(net::CacheType cache_type)
      : SimpleIndexFile(nullptr, nullptr, cache_type, base::FilePath()) {}

  void LoadIndexEntries(base::Time cache_last_modified,
                        const base::Closure& callback,
                        SimpleIndexLoadResult* out_load_result) override {
    load_callback_ = callback;
    load_result_ = out_load_result;
    ++load_index_entries_calls_;
  }

  void WriteToDisk(net::CacheType cache_type,
                   SimpleIndex::IndexWriteToDiskReason reason,
                   const SimpleIndex::EntrySet& entry_set,
                   uint64_t cache_size,
                   const base::TimeTicks& start,
                   bool app_on_background,
                   const base::Closure& callback) override {
    disk_writes_++;
    disk_write_entry_set_ = entry_set;
  }

  void GetAndResetDiskWriteEntrySet(SimpleIndex::EntrySet* entry_set) {
    entry_set->swap(disk_write_entry_set_);
  }

  const base::Closure& load_callback() const { return load_callback_; }
  SimpleIndexLoadResult* load_result() const { return load_result_; }
  int load_index_entries_calls() const { return load_index_entries_calls_; }
  int disk_writes() const { return disk_writes_; }

 private:
  base::Closure load_callback_;
  SimpleIndexLoadResult* load_result_ = nullptr;
  int load_index_entries_calls_ = 0;
  int disk_writes_ = 0;
  SimpleIndex::EntrySet disk_write_entry_set_;
};

class SimpleIndexTest : public net::TestWithTaskEnvironment,
                        public SimpleIndexDelegate {
 protected:
  SimpleIndexTest() : hashes_(base::BindRepeating(&HashesInitializer)) {}

  static uint64_t HashesInitializer(size_t hash_index) {
    return disk_cache::simple_util::GetEntryHashKey(
        base::StringPrintf("key%d", static_cast<int>(hash_index)));
  }

  void SetUp() override {
    std::unique_ptr<MockSimpleIndexFile> index_file(
        new MockSimpleIndexFile(CacheType()));
    index_file_ = index_file->AsWeakPtr();
    index_.reset(new SimpleIndex(/* io_thread = */ nullptr,
                                 /* cleanup_tracker = */ nullptr, this,
                                 CacheType(), std::move(index_file)));

    index_->Initialize(base::Time());
  }

  void WaitForTimeChange() {
    const base::Time initial_time = base::Time::Now();
    do {
      base::PlatformThread::YieldCurrentThread();
    } while (base::Time::Now() -
             initial_time < base::TimeDelta::FromSeconds(1));
  }

  // From SimpleIndexDelegate:
  void DoomEntries(std::vector<uint64_t>* entry_hashes,
                   net::CompletionOnceCallback callback) override {
    for (const uint64_t& entry_hash : *entry_hashes)
      index_->Remove(entry_hash);
    last_doom_entry_hashes_ = *entry_hashes;
    ++doom_entries_calls_;
  }

  // Redirect to allow single "friend" declaration in base class.
  bool GetEntryForTesting(uint64_t key, EntryMetadata* metadata) {
    auto it = index_->entries_set_.find(key);
    if (index_->entries_set_.end() == it)
      return false;
    *metadata = it->second;
    return true;
  }

  void InsertIntoIndexFileReturn(uint64_t hash_key,
                                 base::Time last_used_time,
                                 int entry_size) {
    index_file_->load_result()->entries.insert(std::make_pair(
        hash_key, EntryMetadata(last_used_time,
                                base::checked_cast<uint32_t>(entry_size))));
  }

  void ReturnIndexFile() {
    index_file_->load_result()->did_load = true;
    index_file_->load_callback().Run();
  }

  // Non-const for timer manipulation.
  SimpleIndex* index() { return index_.get(); }
  const MockSimpleIndexFile* index_file() const { return index_file_.get(); }

  const std::vector<uint64_t>& last_doom_entry_hashes() const {
    return last_doom_entry_hashes_;
  }
  int doom_entries_calls() const { return doom_entries_calls_; }

  virtual net::CacheType CacheType() const { return net::DISK_CACHE; }

  const simple_util::ImmutableArray<uint64_t, 16> hashes_;
  std::unique_ptr<SimpleIndex> index_;
  base::WeakPtr<MockSimpleIndexFile> index_file_;

  std::unique_ptr<base::FieldTrialList> field_trial_list_;
  base::test::ScopedFeatureList scoped_feature_list_;

  std::vector<uint64_t> last_doom_entry_hashes_;
  int doom_entries_calls_ = 0;
};

class SimpleIndexAppCacheTest : public SimpleIndexTest {
 protected:
  net::CacheType CacheType() const override { return net::APP_CACHE; }
};

TEST_F(EntryMetadataTest, Basics) {
  EntryMetadata entry_metadata;
  EXPECT_EQ(base::Time(), entry_metadata.GetLastUsedTime());
  EXPECT_EQ(0u, entry_metadata.GetEntrySize());
  EXPECT_EQ(0u, entry_metadata.GetInMemoryData());

  entry_metadata = NewEntryMetadataWithValues();
  CheckEntryMetadataValues(entry_metadata);

  const base::Time new_time = base::Time::Now();
  entry_metadata.SetLastUsedTime(new_time);

  EXPECT_LT(new_time - base::TimeDelta::FromSeconds(2),
            entry_metadata.GetLastUsedTime());
  EXPECT_GT(new_time + base::TimeDelta::FromSeconds(2),
            entry_metadata.GetLastUsedTime());
}

// Tests that setting an unusually small/large last used time results in
// truncation (rather than crashing).
TEST_F(EntryMetadataTest, SaturatedLastUsedTime) {
  EntryMetadata entry_metadata;

  // Set a time that is too large to be represented internally as 32-bit unix
  // timestamp. Will saturate to a large timestamp (in year 2106).
  entry_metadata.SetLastUsedTime(base::Time::Max());
  EXPECT_EQ(INT64_C(15939440895000000),
            entry_metadata.GetLastUsedTime().ToInternalValue());

  // Set a time that is too small to be represented by a unix timestamp (before
  // 1970).
  entry_metadata.SetLastUsedTime(
      base::Time::FromInternalValue(7u));  // This is a date in 1601.
  EXPECT_EQ(base::Time::UnixEpoch() + base::TimeDelta::FromSeconds(1),
            entry_metadata.GetLastUsedTime());
}

TEST_F(EntryMetadataTest, Serialize) {
  EntryMetadata entry_metadata = NewEntryMetadataWithValues();

  base::Pickle pickle;
  entry_metadata.Serialize(net::DISK_CACHE, &pickle);

  base::PickleIterator it(pickle);
  EntryMetadata new_entry_metadata;
  new_entry_metadata.Deserialize(net::DISK_CACHE, &it, true, true);
  CheckEntryMetadataValues(new_entry_metadata);

  // Test reading of old format --- the modern serialization of above entry
  // corresponds, in older format, to an entry with size =
  //   RoundSize(kTestEntrySize) | kTestEntryMemoryData, which then gets
  // rounded again when stored by EntryMetadata.
  base::PickleIterator it2(pickle);
  EntryMetadata new_entry_metadata2;
  new_entry_metadata2.Deserialize(net::DISK_CACHE, &it2, false, false);
  EXPECT_EQ(RoundSize(RoundSize(kTestEntrySize) | kTestEntryMemoryData),
            new_entry_metadata2.GetEntrySize());
  EXPECT_EQ(0, new_entry_metadata2.GetInMemoryData());
}

TEST_F(SimpleIndexTest, IndexSizeCorrectOnMerge) {
  const unsigned int kSizeResolution = 256u;
  index()->SetMaxSize(100 * kSizeResolution);
  index()->Insert(hashes_.at<2>());
  index()->UpdateEntrySize(hashes_.at<2>(), 2u * kSizeResolution);
  index()->Insert(hashes_.at<3>());
  index()->UpdateEntrySize(hashes_.at<3>(), 3u * kSizeResolution);
  index()->Insert(hashes_.at<4>());
  index()->UpdateEntrySize(hashes_.at<4>(), 4u * kSizeResolution);
  EXPECT_EQ(9u * kSizeResolution, index()->cache_size_);
  {
    std::unique_ptr<SimpleIndexLoadResult> result(new SimpleIndexLoadResult());
    result->did_load = true;
    index()->MergeInitializingSet(std::move(result));
  }
  EXPECT_EQ(9u * kSizeResolution, index()->cache_size_);
  {
    std::unique_ptr<SimpleIndexLoadResult> result(new SimpleIndexLoadResult());
    result->did_load = true;
    const uint64_t new_hash_key = hashes_.at<11>();
    result->entries.insert(std::make_pair(
        new_hash_key, EntryMetadata(base::Time::Now(), 11u * kSizeResolution)));
    const uint64_t redundant_hash_key = hashes_.at<4>();
    result->entries.insert(
        std::make_pair(redundant_hash_key,
                       EntryMetadata(base::Time::Now(), 4u * kSizeResolution)));
    index()->MergeInitializingSet(std::move(result));
  }
  EXPECT_EQ((2u + 3u + 4u + 11u) * kSizeResolution, index()->cache_size_);
}

// State of index changes as expected with an insert and a remove.
TEST_F(SimpleIndexTest, BasicInsertRemove) {
  // Confirm blank state.
  EntryMetadata metadata;
  EXPECT_EQ(base::Time(), metadata.GetLastUsedTime());
  EXPECT_EQ(0U, metadata.GetEntrySize());

  // Confirm state after insert.
  index()->Insert(hashes_.at<1>());
  ASSERT_TRUE(GetEntryForTesting(hashes_.at<1>(), &metadata));
  base::Time now(base::Time::Now());
  EXPECT_LT(now - base::TimeDelta::FromMinutes(1), metadata.GetLastUsedTime());
  EXPECT_GT(now + base::TimeDelta::FromMinutes(1), metadata.GetLastUsedTime());
  EXPECT_EQ(0U, metadata.GetEntrySize());

  // Confirm state after remove.
  metadata = EntryMetadata();
  index()->Remove(hashes_.at<1>());
  EXPECT_FALSE(GetEntryForTesting(hashes_.at<1>(), &metadata));
  EXPECT_EQ(base::Time(), metadata.GetLastUsedTime());
  EXPECT_EQ(0U, metadata.GetEntrySize());
}

TEST_F(SimpleIndexTest, Has) {
  // Confirm the base index has dispatched the request for index entries.
  EXPECT_TRUE(index_file_.get());
  EXPECT_EQ(1, index_file_->load_index_entries_calls());

  // Confirm "Has()" always returns true before the callback is called.
  const uint64_t kHash1 = hashes_.at<1>();
  EXPECT_TRUE(index()->Has(kHash1));
  index()->Insert(kHash1);
  EXPECT_TRUE(index()->Has(kHash1));
  index()->Remove(kHash1);
  // TODO(morlovich): Maybe return false on explicitly removed entries?
  EXPECT_TRUE(index()->Has(kHash1));

  ReturnIndexFile();

  // Confirm "Has() returns conditionally now.
  EXPECT_FALSE(index()->Has(kHash1));
  index()->Insert(kHash1);
  EXPECT_TRUE(index()->Has(kHash1));
  index()->Remove(kHash1);
}

TEST_F(SimpleIndexTest, UseIfExists) {
  // Confirm the base index has dispatched the request for index entries.
  EXPECT_TRUE(index_file_.get());
  EXPECT_EQ(1, index_file_->load_index_entries_calls());

  // Confirm "UseIfExists()" always returns true before the callback is called
  // and updates mod time if the entry was really there.
  const uint64_t kHash1 = hashes_.at<1>();
  EntryMetadata metadata1, metadata2;
  EXPECT_TRUE(index()->UseIfExists(kHash1));
  EXPECT_FALSE(GetEntryForTesting(kHash1, &metadata1));
  index()->Insert(kHash1);
  EXPECT_TRUE(index()->UseIfExists(kHash1));
  EXPECT_TRUE(GetEntryForTesting(kHash1, &metadata1));
  WaitForTimeChange();
  EXPECT_TRUE(GetEntryForTesting(kHash1, &metadata2));
  EXPECT_EQ(metadata1.GetLastUsedTime(), metadata2.GetLastUsedTime());
  EXPECT_TRUE(index()->UseIfExists(kHash1));
  EXPECT_TRUE(GetEntryForTesting(kHash1, &metadata2));
  EXPECT_LT(metadata1.GetLastUsedTime(), metadata2.GetLastUsedTime());
  index()->Remove(kHash1);
  EXPECT_TRUE(index()->UseIfExists(kHash1));

  ReturnIndexFile();

  // Confirm "UseIfExists() returns conditionally now
  EXPECT_FALSE(index()->UseIfExists(kHash1));
  EXPECT_FALSE(GetEntryForTesting(kHash1, &metadata1));
  index()->Insert(kHash1);
  EXPECT_TRUE(index()->UseIfExists(kHash1));
  EXPECT_TRUE(GetEntryForTesting(kHash1, &metadata1));
  WaitForTimeChange();
  EXPECT_TRUE(GetEntryForTesting(kHash1, &metadata2));
  EXPECT_EQ(metadata1.GetLastUsedTime(), metadata2.GetLastUsedTime());
  EXPECT_TRUE(index()->UseIfExists(kHash1));
  EXPECT_TRUE(GetEntryForTesting(kHash1, &metadata2));
  EXPECT_LT(metadata1.GetLastUsedTime(), metadata2.GetLastUsedTime());
  index()->Remove(kHash1);
  EXPECT_FALSE(index()->UseIfExists(kHash1));
}

TEST_F(SimpleIndexTest, UpdateEntrySize) {
  base::Time now(base::Time::Now());

  index()->SetMaxSize(1000);

  const uint64_t kHash1 = hashes_.at<1>();
  InsertIntoIndexFileReturn(kHash1, now - base::TimeDelta::FromDays(2), 475);
  ReturnIndexFile();

  EntryMetadata metadata;
  EXPECT_TRUE(GetEntryForTesting(kHash1, &metadata));
  EXPECT_LT(
      now - base::TimeDelta::FromDays(2) - base::TimeDelta::FromSeconds(1),
      metadata.GetLastUsedTime());
  EXPECT_GT(
      now - base::TimeDelta::FromDays(2) + base::TimeDelta::FromSeconds(1),
      metadata.GetLastUsedTime());
  EXPECT_EQ(RoundSize(475u), metadata.GetEntrySize());

  index()->UpdateEntrySize(kHash1, 600u);
  EXPECT_TRUE(GetEntryForTesting(kHash1, &metadata));
  EXPECT_EQ(RoundSize(600u), metadata.GetEntrySize());
  EXPECT_EQ(1, index()->GetEntryCount());
}

TEST_F(SimpleIndexTest, GetEntryCount) {
  EXPECT_EQ(0, index()->GetEntryCount());
  index()->Insert(hashes_.at<1>());
  EXPECT_EQ(1, index()->GetEntryCount());
  index()->Insert(hashes_.at<2>());
  EXPECT_EQ(2, index()->GetEntryCount());
  index()->Insert(hashes_.at<3>());
  EXPECT_EQ(3, index()->GetEntryCount());
  index()->Insert(hashes_.at<3>());
  EXPECT_EQ(3, index()->GetEntryCount());
  index()->Remove(hashes_.at<2>());
  EXPECT_EQ(2, index()->GetEntryCount());
  index()->Insert(hashes_.at<4>());
  EXPECT_EQ(3, index()->GetEntryCount());
  index()->Remove(hashes_.at<3>());
  EXPECT_EQ(2, index()->GetEntryCount());
  index()->Remove(hashes_.at<3>());
  EXPECT_EQ(2, index()->GetEntryCount());
  index()->Remove(hashes_.at<1>());
  EXPECT_EQ(1, index()->GetEntryCount());
  index()->Remove(hashes_.at<4>());
  EXPECT_EQ(0, index()->GetEntryCount());
}

// Confirm that we get the results we expect from a simple init.
TEST_F(SimpleIndexTest, BasicInit) {
  base::Time now(base::Time::Now());

  InsertIntoIndexFileReturn(hashes_.at<1>(),
                            now - base::TimeDelta::FromDays(2),
                            10u);
  InsertIntoIndexFileReturn(hashes_.at<2>(), now - base::TimeDelta::FromDays(3),
                            1000u);

  ReturnIndexFile();

  EntryMetadata metadata;
  EXPECT_TRUE(GetEntryForTesting(hashes_.at<1>(), &metadata));
  EXPECT_EQ(metadata.GetLastUsedTime(),
            index()->GetLastUsedTime(hashes_.at<1>()));
  EXPECT_LT(
      now - base::TimeDelta::FromDays(2) - base::TimeDelta::FromSeconds(1),
      metadata.GetLastUsedTime());
  EXPECT_GT(
      now - base::TimeDelta::FromDays(2) + base::TimeDelta::FromSeconds(1),
      metadata.GetLastUsedTime());
  EXPECT_EQ(RoundSize(10u), metadata.GetEntrySize());
  EXPECT_TRUE(GetEntryForTesting(hashes_.at<2>(), &metadata));
  EXPECT_EQ(metadata.GetLastUsedTime(),
            index()->GetLastUsedTime(hashes_.at<2>()));
  EXPECT_LT(
      now - base::TimeDelta::FromDays(3) - base::TimeDelta::FromSeconds(1),
      metadata.GetLastUsedTime());
  EXPECT_GT(
      now - base::TimeDelta::FromDays(3) + base::TimeDelta::FromSeconds(1),
      metadata.GetLastUsedTime());
  EXPECT_EQ(RoundSize(1000u), metadata.GetEntrySize());
  EXPECT_EQ(base::Time(), index()->GetLastUsedTime(hashes_.at<3>()));
}

// Remove something that's going to come in from the loaded index.
TEST_F(SimpleIndexTest, RemoveBeforeInit) {
  const uint64_t kHash1 = hashes_.at<1>();
  index()->Remove(kHash1);

  InsertIntoIndexFileReturn(kHash1,
                            base::Time::Now() - base::TimeDelta::FromDays(2),
                            10u);
  ReturnIndexFile();

  EXPECT_FALSE(index()->Has(kHash1));
}

// Insert something that's going to come in from the loaded index; correct
// result?
TEST_F(SimpleIndexTest, InsertBeforeInit) {
  const uint64_t kHash1 = hashes_.at<1>();
  index()->Insert(kHash1);

  InsertIntoIndexFileReturn(kHash1,
                            base::Time::Now() - base::TimeDelta::FromDays(2),
                            10u);
  ReturnIndexFile();

  EntryMetadata metadata;
  EXPECT_TRUE(GetEntryForTesting(kHash1, &metadata));
  base::Time now(base::Time::Now());
  EXPECT_LT(now - base::TimeDelta::FromMinutes(1), metadata.GetLastUsedTime());
  EXPECT_GT(now + base::TimeDelta::FromMinutes(1), metadata.GetLastUsedTime());
  EXPECT_EQ(0U, metadata.GetEntrySize());
}

// Insert and Remove something that's going to come in from the loaded index.
TEST_F(SimpleIndexTest, InsertRemoveBeforeInit) {
  const uint64_t kHash1 = hashes_.at<1>();
  index()->Insert(kHash1);
  index()->Remove(kHash1);

  InsertIntoIndexFileReturn(kHash1,
                            base::Time::Now() - base::TimeDelta::FromDays(2),
                            10u);
  ReturnIndexFile();

  EXPECT_FALSE(index()->Has(kHash1));
}

// Insert and Remove something that's going to come in from the loaded index.
TEST_F(SimpleIndexTest, RemoveInsertBeforeInit) {
  const uint64_t kHash1 = hashes_.at<1>();
  index()->Remove(kHash1);
  index()->Insert(kHash1);

  InsertIntoIndexFileReturn(kHash1,
                            base::Time::Now() - base::TimeDelta::FromDays(2),
                            10u);
  ReturnIndexFile();

  EntryMetadata metadata;
  EXPECT_TRUE(GetEntryForTesting(kHash1, &metadata));
  base::Time now(base::Time::Now());
  EXPECT_LT(now - base::TimeDelta::FromMinutes(1), metadata.GetLastUsedTime());
  EXPECT_GT(now + base::TimeDelta::FromMinutes(1), metadata.GetLastUsedTime());
  EXPECT_EQ(0U, metadata.GetEntrySize());
}

// Do all above tests at once + a non-conflict to test for cross-key
// interactions.
TEST_F(SimpleIndexTest, AllInitConflicts) {
  base::Time now(base::Time::Now());

  index()->Remove(hashes_.at<1>());
  InsertIntoIndexFileReturn(hashes_.at<1>(),
                            now - base::TimeDelta::FromDays(2),
                            10u);
  index()->Insert(hashes_.at<2>());
  InsertIntoIndexFileReturn(hashes_.at<2>(),
                            now - base::TimeDelta::FromDays(3),
                            100u);
  index()->Insert(hashes_.at<3>());
  index()->Remove(hashes_.at<3>());
  InsertIntoIndexFileReturn(hashes_.at<3>(),
                            now - base::TimeDelta::FromDays(4),
                            1000u);
  index()->Remove(hashes_.at<4>());
  index()->Insert(hashes_.at<4>());
  InsertIntoIndexFileReturn(hashes_.at<4>(),
                            now - base::TimeDelta::FromDays(5),
                            10000u);
  InsertIntoIndexFileReturn(hashes_.at<5>(),
                            now - base::TimeDelta::FromDays(6),
                            100000u);

  ReturnIndexFile();

  EXPECT_FALSE(index()->Has(hashes_.at<1>()));

  EntryMetadata metadata;
  EXPECT_TRUE(GetEntryForTesting(hashes_.at<2>(), &metadata));
  EXPECT_LT(now - base::TimeDelta::FromMinutes(1), metadata.GetLastUsedTime());
  EXPECT_GT(now + base::TimeDelta::FromMinutes(1), metadata.GetLastUsedTime());
  EXPECT_EQ(0U, metadata.GetEntrySize());

  EXPECT_FALSE(index()->Has(hashes_.at<3>()));

  EXPECT_TRUE(GetEntryForTesting(hashes_.at<4>(), &metadata));
  EXPECT_LT(now - base::TimeDelta::FromMinutes(1), metadata.GetLastUsedTime());
  EXPECT_GT(now + base::TimeDelta::FromMinutes(1), metadata.GetLastUsedTime());
  EXPECT_EQ(0U, metadata.GetEntrySize());

  EXPECT_TRUE(GetEntryForTesting(hashes_.at<5>(), &metadata));

  EXPECT_GT(
      now - base::TimeDelta::FromDays(6) + base::TimeDelta::FromSeconds(1),
      metadata.GetLastUsedTime());
  EXPECT_LT(
      now - base::TimeDelta::FromDays(6) - base::TimeDelta::FromSeconds(1),
      metadata.GetLastUsedTime());

  EXPECT_EQ(RoundSize(100000u), metadata.GetEntrySize());
}

TEST_F(SimpleIndexTest, BasicEviction) {
  base::Time now(base::Time::Now());
  index()->SetMaxSize(1000);
  InsertIntoIndexFileReturn(hashes_.at<1>(),
                            now - base::TimeDelta::FromDays(2),
                            475u);
  index()->Insert(hashes_.at<2>());
  index()->UpdateEntrySize(hashes_.at<2>(), 475u);
  ReturnIndexFile();

  WaitForTimeChange();

  index()->Insert(hashes_.at<3>());
  // Confirm index is as expected: No eviction, everything there.
  EXPECT_EQ(3, index()->GetEntryCount());
  EXPECT_EQ(0, doom_entries_calls());
  EXPECT_TRUE(index()->Has(hashes_.at<1>()));
  EXPECT_TRUE(index()->Has(hashes_.at<2>()));
  EXPECT_TRUE(index()->Has(hashes_.at<3>()));

  // Trigger an eviction, and make sure the right things are tossed.
  // TODO(morlovich): This is dependent on the innards of the implementation
  // as to at exactly what point we trigger eviction. Not sure how to fix
  // that.
  index()->UpdateEntrySize(hashes_.at<3>(), 475u);
  EXPECT_EQ(1, doom_entries_calls());
  EXPECT_EQ(1, index()->GetEntryCount());
  EXPECT_FALSE(index()->Has(hashes_.at<1>()));
  EXPECT_FALSE(index()->Has(hashes_.at<2>()));
  EXPECT_TRUE(index()->Has(hashes_.at<3>()));
  ASSERT_EQ(2u, last_doom_entry_hashes().size());
}

TEST_F(SimpleIndexTest, EvictByLRU) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(kSimpleCacheEvictionWithSize);

  base::Time now(base::Time::Now());
  index()->SetMaxSize(50000);
  InsertIntoIndexFileReturn(hashes_.at<1>(), now - base::TimeDelta::FromDays(2),
                            475u);
  InsertIntoIndexFileReturn(hashes_.at<2>(), now - base::TimeDelta::FromDays(1),
                            40000u);
  ReturnIndexFile();
  WaitForTimeChange();

  index()->Insert(hashes_.at<3>());
  // Confirm index is as expected: No eviction, everything there.
  EXPECT_EQ(3, index()->GetEntryCount());
  EXPECT_EQ(0, doom_entries_calls());
  EXPECT_TRUE(index()->Has(hashes_.at<1>()));
  EXPECT_TRUE(index()->Has(hashes_.at<2>()));
  EXPECT_TRUE(index()->Has(hashes_.at<3>()));

  // Trigger an eviction, and make sure the right things are tossed.
  // TODO(morlovich): This is dependent on the innards of the implementation
  // as to at exactly what point we trigger eviction. Not sure how to fix
  // that.
  index()->UpdateEntrySize(hashes_.at<3>(), 40000u);
  EXPECT_EQ(1, doom_entries_calls());
  EXPECT_EQ(1, index()->GetEntryCount());
  EXPECT_FALSE(index()->Has(hashes_.at<1>()));
  EXPECT_FALSE(index()->Has(hashes_.at<2>()));
  EXPECT_TRUE(index()->Has(hashes_.at<3>()));
  ASSERT_EQ(2u, last_doom_entry_hashes().size());
}

TEST_F(SimpleIndexTest, EvictBySize) {
  base::Time now(base::Time::Now());
  index()->SetMaxSize(50000);
  InsertIntoIndexFileReturn(hashes_.at<1>(), now - base::TimeDelta::FromDays(2),
                            475u);
  InsertIntoIndexFileReturn(hashes_.at<2>(), now - base::TimeDelta::FromDays(1),
                            40000u);
  ReturnIndexFile();
  WaitForTimeChange();

  index()->Insert(hashes_.at<3>());
  // Confirm index is as expected: No eviction, everything there.
  EXPECT_EQ(3, index()->GetEntryCount());
  EXPECT_EQ(0, doom_entries_calls());
  EXPECT_TRUE(index()->Has(hashes_.at<1>()));
  EXPECT_TRUE(index()->Has(hashes_.at<2>()));
  EXPECT_TRUE(index()->Has(hashes_.at<3>()));

  // Trigger an eviction, and make sure the right things are tossed.
  // TODO(morlovich): This is dependent on the innards of the implementation
  // as to at exactly what point we trigger eviction. Not sure how to fix
  // that.
  index()->UpdateEntrySize(hashes_.at<3>(), 40000u);
  EXPECT_EQ(1, doom_entries_calls());
  EXPECT_EQ(2, index()->GetEntryCount());
  EXPECT_TRUE(index()->Has(hashes_.at<1>()));
  EXPECT_FALSE(index()->Has(hashes_.at<2>()));
  EXPECT_TRUE(index()->Has(hashes_.at<3>()));
  ASSERT_EQ(1u, last_doom_entry_hashes().size());
}

// Same as test above, but using much older entries to make sure that small
// things eventually get evictied.
TEST_F(SimpleIndexTest, EvictBySize2) {
  base::Time now(base::Time::Now());
  index()->SetMaxSize(50000);
  InsertIntoIndexFileReturn(hashes_.at<1>(),
                            now - base::TimeDelta::FromDays(200), 475u);
  InsertIntoIndexFileReturn(hashes_.at<2>(), now - base::TimeDelta::FromDays(1),
                            40000u);
  ReturnIndexFile();
  WaitForTimeChange();

  index()->Insert(hashes_.at<3>());
  // Confirm index is as expected: No eviction, everything there.
  EXPECT_EQ(3, index()->GetEntryCount());
  EXPECT_EQ(0, doom_entries_calls());
  EXPECT_TRUE(index()->Has(hashes_.at<1>()));
  EXPECT_TRUE(index()->Has(hashes_.at<2>()));
  EXPECT_TRUE(index()->Has(hashes_.at<3>()));

  // Trigger an eviction, and make sure the right things are tossed.
  // TODO(morlovich): This is dependent on the innards of the implementation
  // as to at exactly what point we trigger eviction. Not sure how to fix
  // that.
  index()->UpdateEntrySize(hashes_.at<3>(), 40000u);
  EXPECT_EQ(1, doom_entries_calls());
  EXPECT_EQ(1, index()->GetEntryCount());
  EXPECT_FALSE(index()->Has(hashes_.at<1>()));
  EXPECT_FALSE(index()->Has(hashes_.at<2>()));
  EXPECT_TRUE(index()->Has(hashes_.at<3>()));
  ASSERT_EQ(2u, last_doom_entry_hashes().size());
}

// Confirm all the operations queue a disk write at some point in the
// future.
TEST_F(SimpleIndexTest, DiskWriteQueued) {
  index()->SetMaxSize(1000);
  ReturnIndexFile();

  EXPECT_FALSE(index()->HasPendingWrite());

  const uint64_t kHash1 = hashes_.at<1>();
  index()->Insert(kHash1);
  EXPECT_TRUE(index()->HasPendingWrite());
  index()->write_to_disk_timer_.Stop();
  EXPECT_FALSE(index()->HasPendingWrite());

  // Attempting to insert a hash that already exists should not queue the
  // write timer.
  index()->Insert(kHash1);
  EXPECT_FALSE(index()->HasPendingWrite());

  index()->UseIfExists(kHash1);
  EXPECT_TRUE(index()->HasPendingWrite());
  index()->write_to_disk_timer_.Stop();

  index()->UpdateEntrySize(kHash1, 20u);
  EXPECT_TRUE(index()->HasPendingWrite());
  index()->write_to_disk_timer_.Stop();

  // Updating to the same size should not queue the write timer.
  index()->UpdateEntrySize(kHash1, 20u);
  EXPECT_FALSE(index()->HasPendingWrite());

  index()->Remove(kHash1);
  EXPECT_TRUE(index()->HasPendingWrite());
  index()->write_to_disk_timer_.Stop();

  // Removing a non-existent hash should not queue the write timer.
  index()->Remove(kHash1);
  EXPECT_FALSE(index()->HasPendingWrite());
}

TEST_F(SimpleIndexTest, DiskWriteExecuted) {
  index()->SetMaxSize(1000);
  ReturnIndexFile();

  EXPECT_FALSE(index()->HasPendingWrite());

  const uint64_t kHash1 = hashes_.at<1>();
  index()->Insert(kHash1);
  index()->UpdateEntrySize(kHash1, 20u);
  EXPECT_TRUE(index()->HasPendingWrite());

  EXPECT_EQ(0, index_file_->disk_writes());
  index()->write_to_disk_timer_.FireNow();
  EXPECT_EQ(1, index_file_->disk_writes());
  SimpleIndex::EntrySet entry_set;
  index_file_->GetAndResetDiskWriteEntrySet(&entry_set);

  uint64_t hash_key = kHash1;
  base::Time now(base::Time::Now());
  ASSERT_EQ(1u, entry_set.size());
  EXPECT_EQ(hash_key, entry_set.begin()->first);
  const EntryMetadata& entry1(entry_set.begin()->second);
  EXPECT_LT(now - base::TimeDelta::FromMinutes(1), entry1.GetLastUsedTime());
  EXPECT_GT(now + base::TimeDelta::FromMinutes(1), entry1.GetLastUsedTime());
  EXPECT_EQ(RoundSize(20u), entry1.GetEntrySize());
}

TEST_F(SimpleIndexTest, DiskWritePostponed) {
  index()->SetMaxSize(1000);
  ReturnIndexFile();

  EXPECT_FALSE(index()->HasPendingWrite());

  index()->Insert(hashes_.at<1>());
  index()->UpdateEntrySize(hashes_.at<1>(), 20u);
  EXPECT_TRUE(index()->HasPendingWrite());
  base::TimeTicks expected_trigger(
      index()->write_to_disk_timer_.desired_run_time());

  WaitForTimeChange();
  EXPECT_EQ(expected_trigger, index()->write_to_disk_timer_.desired_run_time());
  index()->Insert(hashes_.at<2>());
  index()->UpdateEntrySize(hashes_.at<2>(), 40u);
  EXPECT_TRUE(index()->HasPendingWrite());
  EXPECT_LT(expected_trigger, index()->write_to_disk_timer_.desired_run_time());
  index()->write_to_disk_timer_.Stop();
}

// net::APP_CACHE mode should not need to queue disk writes in as many places
// as the default net::DISK_CACHE mode.
TEST_F(SimpleIndexAppCacheTest, DiskWriteQueued) {
  index()->SetMaxSize(1000);
  ReturnIndexFile();

  EXPECT_FALSE(index()->HasPendingWrite());

  const uint64_t kHash1 = hashes_.at<1>();
  index()->Insert(kHash1);
  EXPECT_TRUE(index()->HasPendingWrite());
  index()->write_to_disk_timer_.Stop();
  EXPECT_FALSE(index()->HasPendingWrite());

  // Attempting to insert a hash that already exists should not queue the
  // write timer.
  index()->Insert(kHash1);
  EXPECT_FALSE(index()->HasPendingWrite());

  // Since net::APP_CACHE does not evict or track access times using an
  // entry should not queue the write timer.
  index()->UseIfExists(kHash1);
  EXPECT_FALSE(index()->HasPendingWrite());

  index()->UpdateEntrySize(kHash1, 20u);
  EXPECT_TRUE(index()->HasPendingWrite());
  index()->write_to_disk_timer_.Stop();

  // Updating to the same size should not queue the write timer.
  index()->UpdateEntrySize(kHash1, 20u);
  EXPECT_FALSE(index()->HasPendingWrite());

  index()->Remove(kHash1);
  EXPECT_TRUE(index()->HasPendingWrite());
  index()->write_to_disk_timer_.Stop();

  // Removing a non-existent hash should not queue the write timer.
  index()->Remove(kHash1);
  EXPECT_FALSE(index()->HasPendingWrite());
}

}  // namespace disk_cache
