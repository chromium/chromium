// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/dom_storage/dom_storage_area.h"
#include "content/browser/dom_storage/dom_storage_database_adapter.h"
#include "content/browser/dom_storage/dom_storage_task_runner.h"
#include "content/browser/dom_storage/session_storage_database.h"
#include "content/common/dom_storage/dom_storage_types.h"
#include "content/public/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

using base::ASCIIToUTF16;

namespace content {

class DOMStorageAreaTest : public testing::Test {
 public:
  DOMStorageAreaTest()
      : kOrigin(url::Origin::Create(GURL("http://dom_storage/"))),
        kKey(ASCIIToUTF16("key")),
        kValue(ASCIIToUTF16("value")),
        kKey2(ASCIIToUTF16("key2")),
        kValue2(ASCIIToUTF16("value2")) {}

  const url::Origin kOrigin;
  const base::string16 kKey;
  const base::string16 kValue;
  const base::string16 kKey2;
  const base::string16 kValue2;

  // Method used in the CommitTasks test case.
  void InjectedCommitSequencingTask1(
      const scoped_refptr<DOMStorageArea>& area) {
    // At this point the StartCommitTimer task has run and
    // the OnCommitTimer task is queued. We want to inject after
    // that.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&DOMStorageAreaTest::InjectedCommitSequencingTask2,
                       base::Unretained(this), area));
  }

  void InjectedCommitSequencingTask2(
      const scoped_refptr<DOMStorageArea>& area) {
    // At this point the OnCommitTimer has run.
    // Verify that it put a commit in flight.
    EXPECT_TRUE(area->HasCommitBatchInFlight());
    EXPECT_FALSE(area->GetCurrentCommitBatch());
    EXPECT_TRUE(area->HasUncommittedChanges());
    // Make additional change and verify that a new commit batch
    // is created for that change.
    base::NullableString16 old_value;
    EXPECT_TRUE(area->SetItem(kKey2, kValue2, old_value, &old_value));
    EXPECT_TRUE(area->GetCurrentCommitBatch());
    EXPECT_TRUE(area->HasCommitBatchInFlight());
    EXPECT_TRUE(area->HasUncommittedChanges());
  }

 private:
  base::test::ScopedTaskEnvironment task_environment_;
};

class DOMStorageAreaParamTest : public DOMStorageAreaTest,
                                public testing::WithParamInterface<bool> {
 public:
  DOMStorageAreaParamTest() {}
  ~DOMStorageAreaParamTest() override {}
};

INSTANTIATE_TEST_CASE_P(_, DOMStorageAreaParamTest, ::testing::Bool());

TEST_P(DOMStorageAreaParamTest, DOMStorageAreaBasics) {
  const std::string kFirstNamespaceId = "id1";
  const std::string kSecondNamespaceId = "id2";
  scoped_refptr<DOMStorageArea> area(
      new DOMStorageArea(kFirstNamespaceId, std::vector<std::string>(), kOrigin,
                         nullptr, nullptr));
  const bool values_cached = GetParam();
  area->SetCacheOnlyKeys(!values_cached);
  base::string16 old_value;
  base::NullableString16 old_nullable_value;
  scoped_refptr<DOMStorageArea> copy;

  // We don't focus on the underlying DOMStorageMap functionality
  // since that's covered by seperate unit tests.
  EXPECT_EQ(kOrigin, area->origin());
  EXPECT_EQ(kFirstNamespaceId, area->namespace_id());
  EXPECT_EQ(0u, area->Length());
  EXPECT_TRUE(
      area->SetItem(kKey, kValue, old_nullable_value, &old_nullable_value));
  EXPECT_TRUE(
      area->SetItem(kKey2, kValue2, old_nullable_value, &old_nullable_value));
  EXPECT_FALSE(area->HasUncommittedChanges());

  // Verify that a copy shares the same map.
  copy = area->ShallowCopy(kSecondNamespaceId);
  EXPECT_EQ(kOrigin, copy->origin());
  EXPECT_EQ(kSecondNamespaceId, copy->namespace_id());
  EXPECT_EQ(area->Length(), copy->Length());
  if (values_cached)
    EXPECT_EQ(area->GetItem(kKey).string(), copy->GetItem(kKey).string());
  EXPECT_EQ(area->Key(0).string(), copy->Key(0).string());
  EXPECT_EQ(copy->map_.get(), area->map_.get());
  copy->ClearShallowCopiedCommitBatches();

  // But will deep copy-on-write as needed.
  old_nullable_value = base::NullableString16(kValue, false);
  EXPECT_TRUE(area->RemoveItem(kKey, old_nullable_value, &old_value));
  EXPECT_EQ(kValue, old_value);
  EXPECT_NE(copy->map_.get(), area->map_.get());
  copy = area->ShallowCopy(kSecondNamespaceId);
  EXPECT_EQ(copy->map_.get(), area->map_.get());
  EXPECT_TRUE(
      area->SetItem(kKey, kValue, old_nullable_value, &old_nullable_value));
  EXPECT_NE(copy->map_.get(), area->map_.get());
  copy = area->ShallowCopy(kSecondNamespaceId);
  EXPECT_EQ(copy->map_.get(), area->map_.get());
  EXPECT_NE(0u, area->Length());
  EXPECT_TRUE(area->Clear());
  EXPECT_EQ(0u, area->Length());
  EXPECT_NE(copy->map_.get(), area->map_.get());

  // Verify that once Shutdown(), behaves that way.
  area->Shutdown();
  EXPECT_TRUE(area->is_shutdown_);
  EXPECT_FALSE(area->map_.get());
  EXPECT_EQ(0u, area->Length());
  EXPECT_TRUE(area->Key(0).is_null());
  if (values_cached)
    EXPECT_TRUE(area->GetItem(kKey).is_null());
  EXPECT_FALSE(
      area->SetItem(kKey, kValue, old_nullable_value, &old_nullable_value));
  EXPECT_FALSE(area->RemoveItem(kKey, old_nullable_value, &old_value));
  EXPECT_FALSE(area->Clear());
}

TEST_F(DOMStorageAreaTest, BackingDatabaseOpened) {
  const std::string kSessionStorageNamespaceId = "id1";
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath kExpectedOriginFilePath = temp_dir.GetPath().Append(
      DOMStorageArea::DatabaseFileNameFromOrigin(kOrigin));

  // Valid directory and origin but no session storage backing. Backing should
  // be null.
  {
    scoped_refptr<DOMStorageArea> area(new DOMStorageArea(
        kSessionStorageNamespaceId, std::vector<std::string>(), kOrigin,
        nullptr, nullptr));
    EXPECT_EQ(nullptr, area->backing_.get());

    base::NullableString16 old_value;
    EXPECT_TRUE(area->SetItem(kKey, kValue, old_value, &old_value));
    ASSERT_TRUE(old_value.is_null());

    // Check that saving a value has still left us without a backing database.
    EXPECT_EQ(nullptr, area->backing_.get());
    EXPECT_FALSE(base::PathExists(kExpectedOriginFilePath));
  }
}

TEST_P(DOMStorageAreaParamTest, ShallowCopyWithBacking) {
  const std::string kFirstNamespaceId = "id1";
  const std::string kSecondNamespaceId = "id2";
  const std::string kThirdNamespaceId = "id3";
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  scoped_refptr<SessionStorageDatabase> db = new SessionStorageDatabase(
      temp_dir.GetPath(), base::ThreadTaskRunnerHandle::Get());
  scoped_refptr<DOMStorageArea> area(new DOMStorageArea(
      kFirstNamespaceId, std::vector<std::string>(), kOrigin, db.get(),
      new MockDOMStorageTaskRunner(base::ThreadTaskRunnerHandle::Get().get())));
  EXPECT_TRUE(area->backing_.get());
  EXPECT_TRUE(area->session_storage_backing_);
  const bool values_cached = GetParam();
  area->SetCacheOnlyKeys(!values_cached);

  scoped_refptr<DOMStorageArea> temp_copy;
  temp_copy = area->ShallowCopy(kSecondNamespaceId);
  EXPECT_TRUE(temp_copy->commit_batches_.empty());
  temp_copy->ClearShallowCopiedCommitBatches();

  // Check if shallow copy is consistent.
  base::string16 old_value;
  base::NullableString16 old_nullable_value;
  scoped_refptr<DOMStorageArea> copy;
  EXPECT_TRUE(
      area->SetItem(kKey, kValue, old_nullable_value, &old_nullable_value));
  EXPECT_TRUE(area->HasUncommittedChanges());
  EXPECT_EQ(DOMStorageArea::CommitBatchHolder::TYPE_CURRENT_BATCH,
            area->commit_batches_.front().type);
  copy = area->ShallowCopy(kThirdNamespaceId);
  EXPECT_EQ(copy->map_.get(), area->map_.get());
  EXPECT_EQ(1u, copy->original_namespace_ids_.size());
  EXPECT_EQ(kFirstNamespaceId, copy->original_namespace_ids_[0]);
  if (!values_cached) {
    EXPECT_EQ(area->commit_batches_.front().batch,
              copy->commit_batches_.front().batch);
    EXPECT_EQ(DOMStorageArea::CommitBatchHolder::TYPE_IN_FLIGHT,
              area->commit_batches_.front().type);
    EXPECT_EQ(DOMStorageArea::CommitBatchHolder::TYPE_CLONE,
              copy->commit_batches_.front().type);
  } else {
    EXPECT_TRUE(copy->commit_batches_.empty());
  }

  DOMStorageValuesMap map;
  copy->ExtractValues(&map);
  EXPECT_EQ(1u, map.size());
  EXPECT_EQ(kValue, map[kKey].string());

  // Check if copy on write works.
  EXPECT_TRUE(
      copy->SetItem(kKey2, kValue2, old_nullable_value, &old_nullable_value));
  EXPECT_TRUE(copy->GetCurrentCommitBatch());
  EXPECT_FALSE(copy->commit_batches_.front().type);
  if (!values_cached)
    EXPECT_EQ(DOMStorageArea::CommitBatchHolder::TYPE_CLONE,
              copy->commit_batches_.back().type);
  else
    EXPECT_FALSE(copy->HasCommitBatchInFlight());
  EXPECT_EQ(1u, area->Length());

  // Check clearing of cloned batches.
  area->ClearShallowCopiedCommitBatches();
  copy->ClearShallowCopiedCommitBatches();
  EXPECT_EQ(DOMStorageArea::CommitBatchHolder::TYPE_IN_FLIGHT,
            area->commit_batches_.front().type);
  EXPECT_FALSE(copy->HasCommitBatchInFlight());
}

TEST_F(DOMStorageAreaTest, SetCacheOnlyKeysWithoutBacking) {
  const std::string kFirstNamespaceId = "id1";
  scoped_refptr<DOMStorageArea> area(
      new DOMStorageArea(kFirstNamespaceId, std::vector<std::string>(), kOrigin,
                         nullptr, nullptr));
  EXPECT_EQ(DOMStorageArea::LOAD_STATE_KEYS_AND_VALUES,
            area->desired_load_state_);
  EXPECT_FALSE(area->map_->has_only_keys());
  base::NullableString16 old_value;
  EXPECT_TRUE(area->SetItem(kKey, kValue, old_value, &old_value));
  DOMStorageValuesMap map;
  area->ExtractValues(&map);
  EXPECT_EQ(1u, map.size());

  area->SetCacheOnlyKeys(true);
  EXPECT_EQ(DOMStorageArea::LOAD_STATE_KEYS_AND_VALUES,
            area->desired_load_state_);  // cannot be disabled without backing.
  area->SetItem(kKey, kValue2, old_value, &old_value);
  EXPECT_FALSE(area->map_->has_only_keys());
  EXPECT_EQ(kValue, old_value.string());
  area->ExtractValues(&map);
  EXPECT_EQ(kValue2, map[kKey].string());
  EXPECT_EQ(1u, map.size());
}

TEST_F(DOMStorageAreaTest, SetCacheOnlyKeysWithBacking) {
  const std::string kNamespaceId = "id1";
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  scoped_refptr<SessionStorageDatabase> db = new SessionStorageDatabase(
      temp_dir.GetPath(), base::ThreadTaskRunnerHandle::Get());
  scoped_refptr<DOMStorageArea> area(new DOMStorageArea(
      kNamespaceId, std::vector<std::string>(), kOrigin, db.get(),
      new MockDOMStorageTaskRunner(base::ThreadTaskRunnerHandle::Get().get())));

  EXPECT_TRUE(area->backing_.get());
  EXPECT_EQ(DOMStorageArea::LOAD_STATE_UNLOADED, area->load_state_);

#if !defined(OS_ANDROID)
  EXPECT_EQ(DOMStorageArea::LOAD_STATE_KEYS_AND_VALUES,
            area->desired_load_state_);
  area->SetCacheOnlyKeys(true);
#endif
  EXPECT_EQ(DOMStorageArea::LOAD_STATE_KEYS_ONLY, area->desired_load_state_);
  base::NullableString16 old_value;
  EXPECT_TRUE(area->SetItem(kKey, kValue, old_value, &old_value));
  EXPECT_EQ(DOMStorageArea::LOAD_STATE_KEYS_ONLY, area->load_state_);
  EXPECT_TRUE(area->GetCurrentCommitBatch());

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(area->GetCurrentCommitBatch());
  EXPECT_EQ(1u, area->Length());

  // Fill the current batch and in flight batch.
  EXPECT_TRUE(area->SetItem(kKey2, kValue, old_value, &old_value));
  area->PostCommitTask();
  EXPECT_FALSE(area->GetCurrentCommitBatch());
  EXPECT_TRUE(area->HasCommitBatchInFlight());
  EXPECT_TRUE(area->SetItem(kKey2, kValue2, old_value, &old_value));
  EXPECT_TRUE(area->GetCurrentCommitBatch());

  // The values must be imported from the backing, and from the commit batches.
  area->SetCacheOnlyKeys(false);
  EXPECT_EQ(2u, area->Length());
  EXPECT_EQ(kValue, area->GetItem(kKey).string());
  EXPECT_EQ(kValue2, area->GetItem(kKey2).string());

  // Check if disabling cache clears the cache after committing only.
  area->SetCacheOnlyKeys(true);
  EXPECT_EQ(DOMStorageArea::LOAD_STATE_KEYS_ONLY, area->desired_load_state_);
  EXPECT_EQ(DOMStorageArea::LOAD_STATE_KEYS_AND_VALUES, area->load_state_);
  ASSERT_FALSE(area->map_->has_only_keys());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DOMStorageArea::LOAD_STATE_KEYS_ONLY, area->load_state_);
  EXPECT_TRUE(area->map_->has_only_keys());
  EXPECT_FALSE(area->HasCommitBatchInFlight());

  // Check if Clear() works as expected when values are desired.
  area->Clear();
  EXPECT_TRUE(area->SetItem(kKey2, kValue2, old_value, &old_value));
  area->PostCommitTask();
  EXPECT_TRUE(area->SetItem(kKey, kValue, old_value, &old_value));
  EXPECT_EQ(2u, area->Length());
  area->Clear();
  EXPECT_TRUE(area->SetItem(kKey2, kValue, old_value, &old_value));
  EXPECT_TRUE(area->GetCurrentCommitBatch()->batch->clear_all_first);
  EXPECT_TRUE(area->commit_batches_.back().batch->clear_all_first);
  area->SetCacheOnlyKeys(false);
  EXPECT_EQ(DOMStorageArea::LOAD_STATE_KEYS_ONLY, area->load_state_);

  // Unload only after commit.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DOMStorageArea::LOAD_STATE_UNLOADED, area->load_state_);
  EXPECT_EQ(1u, area->Length());
  EXPECT_EQ(kValue, area->GetItem(kKey2).string());
  EXPECT_EQ(DOMStorageArea::LOAD_STATE_KEYS_AND_VALUES, area->load_state_);
}

TEST_P(DOMStorageAreaParamTest, CommitTasks) {
  const std::string kNamespaceId = "id1";
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  scoped_refptr<SessionStorageDatabase> db = new SessionStorageDatabase(
      temp_dir.GetPath(), base::ThreadTaskRunnerHandle::Get());
  scoped_refptr<DOMStorageArea> area(new DOMStorageArea(
      kNamespaceId, std::vector<std::string>(), kOrigin, db.get(),
      new MockDOMStorageTaskRunner(base::ThreadTaskRunnerHandle::Get().get())));

  area->SetCacheOnlyKeys(GetParam());

  // Unrelated to commits, but while we're here, see that querying Length()
  // causes the backing database to be opened and presumably read from.
  EXPECT_EQ(DOMStorageArea::LOAD_STATE_UNLOADED, area->load_state_);
  EXPECT_EQ(0u, area->Length());
  EXPECT_EQ(area->desired_load_state_, area->load_state_);

  DOMStorageValuesMap values;
  base::NullableString16 old_value;

  // See that changes are batched up.
  EXPECT_FALSE(area->GetCurrentCommitBatch());
  EXPECT_TRUE(area->SetItem(kKey, kValue, old_value, &old_value));
  EXPECT_TRUE(area->HasUncommittedChanges());
  EXPECT_TRUE(area->GetCurrentCommitBatch());
  EXPECT_FALSE(area->GetCurrentCommitBatch()->batch->clear_all_first);
  EXPECT_EQ(1u, area->GetCurrentCommitBatch()->batch->changed_values.size());
  EXPECT_TRUE(area->SetItem(kKey2, kValue2, old_value, &old_value));
  EXPECT_FALSE(area->GetCurrentCommitBatch()->batch->clear_all_first);
  EXPECT_EQ(2u, area->GetCurrentCommitBatch()->batch->changed_values.size());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(area->HasUncommittedChanges());
  EXPECT_FALSE(area->GetCurrentCommitBatch());
  EXPECT_FALSE(area->HasCommitBatchInFlight());
  // Verify the changes made it to the database.
  values.clear();
  area->backing_->ReadAllValues(&values);
  EXPECT_EQ(2u, values.size());
  EXPECT_EQ(kValue, values[kKey].string());
  EXPECT_EQ(kValue2, values[kKey2].string());

  // See that clear is handled properly.
  EXPECT_TRUE(area->Clear());
  EXPECT_TRUE(area->GetCurrentCommitBatch());
  EXPECT_TRUE(area->GetCurrentCommitBatch()->batch->clear_all_first);
  EXPECT_TRUE(area->GetCurrentCommitBatch()->batch->changed_values.empty());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(area->GetCurrentCommitBatch());
  EXPECT_FALSE(area->HasCommitBatchInFlight());
  // Verify the changes made it to the database.
  values.clear();
  area->backing_->ReadAllValues(&values);
  EXPECT_TRUE(values.empty());

  // See that if changes accrue while a commit is "in flight"
  // those will also get committed.
  EXPECT_TRUE(area->SetItem(kKey, kValue, old_value, &old_value));
  EXPECT_TRUE(area->HasUncommittedChanges());
  // At this point the StartCommitTimer task has been posted to the after
  // startup task queue. We inject another task in the queue that will
  // execute when the CommitChanges task is inflight. From within our
  // injected task, we'll make an additional SetItem() call and verify
  // that a new commit batch is created for that additional change.
  BrowserThread::PostAfterStartupTask(
      FROM_HERE, base::ThreadTaskRunnerHandle::Get(),
      base::BindOnce(&DOMStorageAreaTest::InjectedCommitSequencingTask1,
                     base::Unretained(this), area));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(area->HasOneRef());
  EXPECT_FALSE(area->HasUncommittedChanges());
  // Verify the changes made it to the database.
  values.clear();
  area->backing_->ReadAllValues(&values);
  EXPECT_EQ(2u, values.size());
  EXPECT_EQ(kValue, values[kKey].string());
  EXPECT_EQ(kValue2, values[kKey2].string());
}

TEST_P(DOMStorageAreaParamTest, CommitChangesAtShutdown) {
  const std::string kNamespaceId = "id1";
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  scoped_refptr<SessionStorageDatabase> db = new SessionStorageDatabase(
      temp_dir.GetPath(), base::ThreadTaskRunnerHandle::Get());
  scoped_refptr<DOMStorageArea> area(new DOMStorageArea(
      kNamespaceId, std::vector<std::string>(), kOrigin, db.get(),
      new MockDOMStorageTaskRunner(base::ThreadTaskRunnerHandle::Get().get())));

  area->SetCacheOnlyKeys(GetParam());

  DOMStorageValuesMap values;
  base::NullableString16 old_value;
  EXPECT_TRUE(area->SetItem(kKey, kValue, old_value, &old_value));
  EXPECT_TRUE(area->HasUncommittedChanges());
  area->backing_->ReadAllValues(&values);
  EXPECT_TRUE(values.empty());  // not committed yet
  area->Shutdown();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(area->HasOneRef());
  EXPECT_FALSE(area->backing_.get());

  // Verify changes were committed.
  db->ReadAreaValues(kNamespaceId, std::vector<std::string>(), kOrigin,
                     &values);
  EXPECT_EQ(1u, values.size());
  EXPECT_EQ(kValue, values[kKey].string());

  // A second Shutdown call should be safe.
  area->Shutdown();
}

TEST_P(DOMStorageAreaParamTest, PurgeMemory) {
  const std::string kNamespaceId = "id1";
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  scoped_refptr<SessionStorageDatabase> db = new SessionStorageDatabase(
      temp_dir.GetPath(), base::ThreadTaskRunnerHandle::Get());
  scoped_refptr<DOMStorageArea> area(new DOMStorageArea(
      kNamespaceId, std::vector<std::string>(), kOrigin, db.get(),
      new MockDOMStorageTaskRunner(base::ThreadTaskRunnerHandle::Get().get())));
  area->SetCacheOnlyKeys(GetParam());

  // Unowned ptr we use to verify that 'purge' has happened.
  DOMStorageMap* original_map = area->map_.get();

  // Should do no harm when called on a newly constructed object.
  EXPECT_EQ(DOMStorageArea::LOAD_STATE_UNLOADED, area->load_state_);
  area->PurgeMemory();
  EXPECT_EQ(DOMStorageArea::LOAD_STATE_UNLOADED, area->load_state_);
  EXPECT_EQ(original_map, area->map_.get());

  // Should not do anything when commits are pending.
  base::NullableString16 old_value;
  area->SetItem(kKey, kValue, old_value, &old_value);
  original_map = area->map_.get();  // importing creates new map.
  EXPECT_EQ(area->desired_load_state_, area->load_state_);
  EXPECT_TRUE(area->HasUncommittedChanges());
  area->PurgeMemory();
  EXPECT_EQ(area->desired_load_state_, area->load_state_);
  EXPECT_TRUE(area->HasUncommittedChanges());
  EXPECT_EQ(original_map, area->map_.get());

  // Commit the changes from above,
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(area->HasUncommittedChanges());
  EXPECT_EQ(original_map, area->map_.get());

  // Should drop caches and reset database connections
  // when invoked on an area that's loaded up primed.
  area->PurgeMemory();
  EXPECT_EQ(DOMStorageArea::LOAD_STATE_UNLOADED, area->load_state_);
  EXPECT_NE(original_map, area->map_.get());
}

TEST_F(DOMStorageAreaTest, DatabaseFileNames) {
  struct {
    const char* origin;
    const char* file_name;
  } kCases[] = {
      {"https://www.google.com/", "https_www.google.com_0.localstorage"},
      {"http://www.google.com:8080/", "http_www.google.com_8080.localstorage"},
      {"file:///", "file__0.localstorage"},
  };

  for (size_t i = 0; i < arraysize(kCases); ++i) {
    url::Origin origin =
        url::Origin::Create(GURL(kCases[i].origin).GetOrigin());
    base::FilePath file_name =
        base::FilePath().AppendASCII(kCases[i].file_name);

    EXPECT_EQ(file_name,
              DOMStorageArea::DatabaseFileNameFromOrigin(origin));
    EXPECT_EQ(origin,
              DOMStorageArea::OriginFromDatabaseFileName(file_name));
  }
}

TEST_F(DOMStorageAreaTest, RateLimiter) {
  // Limit to 1000 samples per second
  DOMStorageArea::RateLimiter rate_limiter(
      1000, base::TimeDelta::FromSeconds(1));

  // No samples have been added so no time/delay should be needed.
  EXPECT_EQ(base::TimeDelta(),
            rate_limiter.ComputeTimeNeeded());
  EXPECT_EQ(base::TimeDelta(),
            rate_limiter.ComputeDelayNeeded(base::TimeDelta()));
  EXPECT_EQ(base::TimeDelta(),
            rate_limiter.ComputeDelayNeeded(base::TimeDelta::FromDays(1)));

  // Add a seconds worth of samples.
  rate_limiter.add_samples(1000);
  EXPECT_EQ(base::TimeDelta::FromSeconds(1),
            rate_limiter.ComputeTimeNeeded());
  EXPECT_EQ(base::TimeDelta::FromSeconds(1),
            rate_limiter.ComputeDelayNeeded(base::TimeDelta()));
  EXPECT_EQ(base::TimeDelta(),
            rate_limiter.ComputeDelayNeeded(base::TimeDelta::FromSeconds(1)));
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(250),
            rate_limiter.ComputeDelayNeeded(
                base::TimeDelta::FromMilliseconds(750)));
  EXPECT_EQ(base::TimeDelta(),
            rate_limiter.ComputeDelayNeeded(
                base::TimeDelta::FromDays(1)));

  // And another half seconds worth.
  rate_limiter.add_samples(500);
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1500),
            rate_limiter.ComputeTimeNeeded());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1500),
            rate_limiter.ComputeDelayNeeded(base::TimeDelta()));
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(500),
            rate_limiter.ComputeDelayNeeded(base::TimeDelta::FromSeconds(1)));
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(750),
            rate_limiter.ComputeDelayNeeded(
                base::TimeDelta::FromMilliseconds(750)));
  EXPECT_EQ(base::TimeDelta(),
            rate_limiter.ComputeDelayNeeded(
                base::TimeDelta::FromMilliseconds(1500)));
  EXPECT_EQ(base::TimeDelta(),
            rate_limiter.ComputeDelayNeeded(
                base::TimeDelta::FromDays(1)));
}

}  // namespace content
