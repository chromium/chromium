// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// Tests for the QueryTracker.

#include "gpu/command_buffer/client/query_tracker.h"

#include <GLES2/gl2ext.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "gpu/command_buffer/client/client_test_helper.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/mapped_memory.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AnyNumber;

namespace gpu {
namespace gles2 {

class QuerySyncManagerTest : public testing::Test {
 protected:
  static const int32_t kNumCommandEntries = 400;
  static const int32_t kCommandBufferSizeBytes =
      kNumCommandEntries * sizeof(CommandBufferEntry);

  void SetUp() override {
    command_buffer_ = std::make_unique<MockClientCommandBuffer>();
    helper_ = std::make_unique<GLES2CmdHelper>(command_buffer_.get());
    helper_->Initialize(kCommandBufferSizeBytes);
    mapped_memory_ = std::make_unique<MappedMemoryManager>(
        helper_.get(), MappedMemoryManager::kNoLimit);
    sync_manager_ = std::make_unique<QuerySyncManager>(mapped_memory_.get());
  }

  void TearDown() override {
    EXPECT_CALL(*command_buffer_, DestroyTransferBuffer(_)).Times(AnyNumber());
    sync_manager_.reset();
    mapped_memory_.reset();
    helper_.reset();
    command_buffer_.reset();
  }

  std::unique_ptr<MockClientCommandBuffer> command_buffer_;
  std::unique_ptr<GLES2CmdHelper> helper_;
  std::unique_ptr<MappedMemoryManager> mapped_memory_;
  std::unique_ptr<QuerySyncManager> sync_manager_;
};

TEST_F(QuerySyncManagerTest, Basic) {
  QuerySyncManager::QueryInfo infos[4];
  memset(&infos, 0xBD, sizeof(infos));

  for (size_t ii = 0; ii < std::size(infos); ++ii) {
    EXPECT_TRUE(sync_manager_->Alloc(&infos[ii]));
    ASSERT_TRUE(infos[ii].sync != nullptr);
    EXPECT_EQ(0, base::subtle::Atomic32{infos[ii].sync->process_count});
    EXPECT_EQ(0u, uint64_t{infos[ii].sync->result});
    EXPECT_EQ(0, infos[ii].submit_count);
  }

  for (size_t ii = 0; ii < std::size(infos); ++ii) {
    sync_manager_->Free(infos[ii]);
  }
}

TEST_F(QuerySyncManagerTest, DontFree) {
  QuerySyncManager::QueryInfo infos[4];
  memset(&infos, 0xBD, sizeof(infos));

  for (size_t ii = 0; ii < std::size(infos); ++ii) {
    EXPECT_TRUE(sync_manager_->Alloc(&infos[ii]));
  }
}

TEST_F(QuerySyncManagerTest, FreePendingSyncs) {
  QuerySyncManager::QueryInfo info;
  EXPECT_TRUE(sync_manager_->Alloc(&info));
  QuerySyncManager::Bucket* bucket = info.bucket;

  // Mark the query as in-use.
  ++info.submit_count;

  // Freeing the QueryInfo should keep the QuerySync busy as it's still in-use,
  // but should be tracked in pending_syncs.
  sync_manager_->Free(info);
  EXPECT_FALSE(bucket->pending_syncs.empty());
  EXPECT_TRUE(bucket->in_use_query_syncs.any());

  // FreePendingSyncs should not free in-use QuerySync.
  bucket->FreePendingSyncs();
  EXPECT_FALSE(bucket->pending_syncs.empty());
  EXPECT_TRUE(bucket->in_use_query_syncs.any());

  // Mark the query as completed.
  info.sync->process_count = info.submit_count;

  // FreePendingSyncs should free the QuerySync.
  bucket->FreePendingSyncs();
  EXPECT_TRUE(bucket->pending_syncs.empty());
  EXPECT_FALSE(bucket->in_use_query_syncs.any());

  // Allocate a new Query, mark it in-use
  EXPECT_TRUE(sync_manager_->Alloc(&info));
  bucket = info.bucket;
  ++info.submit_count;

  // Mark the query as completed
  info.sync->process_count = info.submit_count;

  // FreePendingSyncs should not free the QuerySync. Even though the query is
  // completed, is has not been deleted yet.
  bucket->FreePendingSyncs();
  EXPECT_TRUE(bucket->in_use_query_syncs.any());

  // Free the QueryInfo, it should be immediately freed.
  sync_manager_->Free(info);
  EXPECT_TRUE(bucket->pending_syncs.empty());
  EXPECT_FALSE(bucket->in_use_query_syncs.any());
}

TEST_F(QuerySyncManagerTest, Shrink) {
  QuerySyncManager::QueryInfo info;
  EXPECT_TRUE(sync_manager_->Alloc(&info));
  QuerySyncManager::Bucket* bucket = info.bucket;
  QuerySync* syncs = bucket->syncs;

  FencedAllocator::State state =
      mapped_memory_->GetPointerStatusForTest(syncs, nullptr);
  EXPECT_EQ(FencedAllocator::IN_USE, state);

  // Shrink while a query is allocated - should not release anything.
  sync_manager_->Shrink(helper_.get());
  state = mapped_memory_->GetPointerStatusForTest(syncs, nullptr);
  EXPECT_EQ(FencedAllocator::IN_USE, state);

  // Free query that was never submitted.
  sync_manager_->Free(info);
  EXPECT_TRUE(bucket->pending_syncs.empty());
  EXPECT_FALSE(bucket->in_use_query_syncs.any());

  // Shrink should release the memory immediately.
  sync_manager_->Shrink(helper_.get());
  EXPECT_TRUE(sync_manager_->buckets_.empty());
  state = mapped_memory_->GetPointerStatusForTest(syncs, nullptr);
  EXPECT_EQ(FencedAllocator::FREE, state);

  EXPECT_TRUE(sync_manager_->Alloc(&info));
  bucket = info.bucket;
  syncs = bucket->syncs;

  state = mapped_memory_->GetPointerStatusForTest(syncs, nullptr);
  EXPECT_EQ(FencedAllocator::IN_USE, state);

  // Free a query that was submitted, but not completed.
  ++info.submit_count;
  sync_manager_->Free(info);
  EXPECT_FALSE(bucket->pending_syncs.empty());
  EXPECT_TRUE(bucket->in_use_query_syncs.any());

  int32_t last_token = helper_->InsertToken();

  // Shrink should release the memory, pending a new token.
  sync_manager_->Shrink(helper_.get());
  EXPECT_TRUE(sync_manager_->buckets_.empty());
  int32_t token = 0;
  state = mapped_memory_->GetPointerStatusForTest(syncs, &token);
  EXPECT_EQ(FencedAllocator::FREE_PENDING_TOKEN, state);
  EXPECT_EQ(last_token + 1, token);

  EXPECT_TRUE(sync_manager_->Alloc(&info));
  bucket = info.bucket;
  syncs = bucket->syncs;

  state = mapped_memory_->GetPointerStatusForTest(syncs, nullptr);
  EXPECT_EQ(FencedAllocator::IN_USE, state);

  // Free a query that was submitted, but not completed yet.
  ++info.submit_count;
  int32_t submit_count = info.submit_count;
  QuerySync* sync = info.sync;
  sync_manager_->Free(info);
  EXPECT_FALSE(bucket->pending_syncs.empty());
  EXPECT_TRUE(bucket->in_use_query_syncs.any());

  // Complete the query after Free.
  sync->process_count = submit_count;

  // Shrink should free the memory immediately since the query is completed.
  sync_manager_->Shrink(helper_.get());
  EXPECT_TRUE(sync_manager_->buckets_.empty());
  state = mapped_memory_->GetPointerStatusForTest(syncs, nullptr);
  EXPECT_EQ(FencedAllocator::FREE, state);
}

class QueryTrackerTest : public testing::Test {
 protected:
  static const int32_t kNumCommandEntries = 400;
  static const int32_t kCommandBufferSizeBytes =
      kNumCommandEntries * sizeof(CommandBufferEntry);

  void SetUp() override {
    command_buffer_ = std::make_unique<MockClientCommandBuffer>();
    helper_ = std::make_unique<GLES2CmdHelper>(command_buffer_.get());
    helper_->Initialize(kCommandBufferSizeBytes);
    mapped_memory_ = std::make_unique<MappedMemoryManager>(
        helper_.get(), MappedMemoryManager::kNoLimit);
    query_tracker_ = std::make_unique<QueryTracker>(mapped_memory_.get());
  }

  void TearDown() override {
    helper_->CommandBufferHelper::Flush();
    EXPECT_CALL(*command_buffer_, DestroyTransferBuffer(_)).Times(AnyNumber());
    query_tracker_.reset();
    mapped_memory_.reset();
    helper_.reset();
    command_buffer_.reset();
  }

  QuerySync* GetSync(QueryTracker::Query* query) {
    return query->info_.sync;
  }

  QuerySyncManager::Bucket* GetBucket(QueryTracker::Query* query) {
    return query->info_.bucket;
  }

  uint32_t GetBucketUsedCount(QuerySyncManager::Bucket* bucket) {
    return bucket->in_use_query_syncs.count();
  }

  uint32_t GetFlushGeneration() { return helper_->flush_generation(); }

  std::unique_ptr<MockClientCommandBuffer> command_buffer_;
  std::unique_ptr<GLES2CmdHelper> helper_;
  std::unique_ptr<MappedMemoryManager> mapped_memory_;
  std::unique_ptr<QueryTracker> query_tracker_;
};

TEST_F(QueryTrackerTest, Basic) {
  const GLuint kId1 = 123;
  const GLuint kId2 = 124;

  // Check we can create a Query.
  QueryTracker::Query* query = query_tracker_->CreateQuery(
      kId1, GL_ANY_SAMPLES_PASSED_EXT);
  ASSERT_TRUE(query != nullptr);
  // Check we can get the same Query.
  EXPECT_EQ(query, query_tracker_->GetQuery(kId1));
  // Check we get nothing for a non-existent query.
  EXPECT_TRUE(query_tracker_->GetQuery(kId2) == nullptr);
  // Check we can delete the query.
  query_tracker_->RemoveQuery(kId1);
  // Check we get nothing for a non-existent query.
  EXPECT_TRUE(query_tracker_->GetQuery(kId1) == nullptr);
}

TEST_F(QueryTrackerTest, Query) {
  const GLuint kId1 = 123;
  const int32_t kToken = 46;
  const uint32_t kResult = 456;

  // Create a Query.
  QueryTracker::Query* query = query_tracker_->CreateQuery(
      kId1, GL_ANY_SAMPLES_PASSED_EXT);
  ASSERT_TRUE(query != nullptr);
  EXPECT_TRUE(query->NeverUsed());
  EXPECT_FALSE(query->Pending());
  EXPECT_EQ(0, query->token());
  EXPECT_EQ(0, query->submit_count());

  // Check MarkAsActive.
  query->MarkAsActive();
  EXPECT_FALSE(query->NeverUsed());
  EXPECT_FALSE(query->Pending());
  EXPECT_EQ(0, query->token());
  EXPECT_EQ(0, query->submit_count());
  EXPECT_EQ(1, query->NextSubmitCount());

  // Check MarkAsPending.
  query->MarkAsPending(kToken, query->NextSubmitCount());
  EXPECT_FALSE(query->NeverUsed());
  EXPECT_TRUE(query->Pending());
  EXPECT_EQ(kToken, query->token());
  EXPECT_EQ(1, query->submit_count());

  // Flush only once if no more flushes happened between a call to
  // EndQuery command and CheckResultsAvailable
  // Advance put_ so flush calls in CheckResultsAvailable go through
  // and updates flush_generation count
  helper_->Noop(1);

  // Store FlushGeneration count after EndQuery is called
  uint32_t gen1 = GetFlushGeneration();

  bool flush_if_pending = false;
  EXPECT_FALSE(query->CheckResultsAvailable(helper_.get(), flush_if_pending));
  EXPECT_FALSE(query->NeverUsed());
  EXPECT_TRUE(query->Pending());

  // No flush should happen if |flush_if_pending| is false.
  uint32_t gen2 = GetFlushGeneration();
  EXPECT_EQ(gen1, gen2);

  flush_if_pending = true;

  // Check CheckResultsAvailable.
  EXPECT_FALSE(query->CheckResultsAvailable(helper_.get(), flush_if_pending));
  EXPECT_FALSE(query->NeverUsed());
  EXPECT_TRUE(query->Pending());

  gen2 = GetFlushGeneration();
  EXPECT_NE(gen1, gen2);

  // Repeated calls to CheckResultsAvailable should not flush unnecessarily
  EXPECT_FALSE(query->CheckResultsAvailable(helper_.get(), flush_if_pending));
  gen1 = GetFlushGeneration();
  EXPECT_EQ(gen1, gen2);
  EXPECT_FALSE(query->CheckResultsAvailable(helper_.get(), flush_if_pending));
  gen1 = GetFlushGeneration();
  EXPECT_EQ(gen1, gen2);

  // Simulate GPU process marking it as available.
  QuerySync* sync = GetSync(query);
  sync->process_count = query->submit_count();
  sync->result = kResult;

  // Check CheckResultsAvailable.
  EXPECT_TRUE(query->CheckResultsAvailable(helper_.get(), flush_if_pending));
  EXPECT_EQ(kResult, query->GetResult());
  EXPECT_FALSE(query->NeverUsed());
  EXPECT_FALSE(query->Pending());
}

TEST_F(QueryTrackerTest, Remove) {
  const GLuint kId1 = 123;
  const int32_t kToken = 46;
  const uint32_t kResult = 456;

  // Create a Query.
  QueryTracker::Query* query = query_tracker_->CreateQuery(
      kId1, GL_ANY_SAMPLES_PASSED_EXT);
  ASSERT_TRUE(query != nullptr);

  QuerySyncManager::Bucket* bucket = GetBucket(query);
  EXPECT_EQ(1u, GetBucketUsedCount(bucket));

  query->MarkAsActive();
  int32_t submit_count = query->NextSubmitCount();
  query->MarkAsPending(kToken, submit_count);
  QuerySync* sync = GetSync(query);

  query_tracker_->RemoveQuery(kId1);
  // Check we get nothing for a non-existent query.
  EXPECT_TRUE(query_tracker_->GetQuery(kId1) == nullptr);

  // Check that memory was not freed.
  EXPECT_EQ(1u, GetBucketUsedCount(bucket));
  EXPECT_EQ(1u, bucket->pending_syncs.size());

  // Simulate GPU process marking it as available.
  sync->result = kResult;
  sync->process_count = submit_count;

  // Check FreePendingSyncs.
  bucket->FreePendingSyncs();
  EXPECT_EQ(0u, GetBucketUsedCount(bucket));
}

TEST_F(QueryTrackerTest, RemoveActive) {
  const GLuint kId1 = 123;

  // Create a Query.
  QueryTracker::Query* query =
      query_tracker_->CreateQuery(kId1, GL_ANY_SAMPLES_PASSED_EXT);
  ASSERT_TRUE(query != nullptr);

  QuerySyncManager::Bucket* bucket = GetBucket(query);
  EXPECT_EQ(1u, GetBucketUsedCount(bucket));

  query->MarkAsActive();

  query_tracker_->RemoveQuery(kId1);
  // Check we get nothing for a non-existent query.
  EXPECT_TRUE(query_tracker_->GetQuery(kId1) == nullptr);

  // Check that memory was freed.
  EXPECT_EQ(0u, GetBucketUsedCount(bucket));
  EXPECT_EQ(0u, bucket->pending_syncs.size());
}

TEST_F(QueryTrackerTest, ManyQueries) {
  const GLuint kId1 = 123;
  const int32_t kToken = 46;
  const uint32_t kResult = 456;

  const uint32_t kTestSize = 4000;
  static_assert(kTestSize > QuerySyncManager::kSyncsPerBucket,
                "We want to use more than one bucket");
  // Create lots of queries.
  std::vector<QueryTracker::Query*> queries;
  for (size_t i = 0; i < kTestSize; i++) {
    QueryTracker::Query* query =
        query_tracker_->CreateQuery(kId1 + i, GL_ANY_SAMPLES_PASSED_EXT);
    ASSERT_TRUE(query != nullptr);
    queries.push_back(query);
    QuerySyncManager::Bucket* bucket = GetBucket(query);
    EXPECT_LE(1u, GetBucketUsedCount(bucket));
  }

  QuerySyncManager::Bucket* query_0_bucket = GetBucket(queries[0]);
  uint32_t expected_use_count = QuerySyncManager::kSyncsPerBucket;
  EXPECT_EQ(expected_use_count, GetBucketUsedCount(query_0_bucket));

  while (!queries.empty()) {
    QueryTracker::Query* query = queries.back();
    queries.pop_back();
    GLuint query_id = kId1 + queries.size();
    EXPECT_EQ(query_id, query->id());
    query->MarkAsActive();
    int32_t submit_count = query->NextSubmitCount();
    query->MarkAsPending(kToken, submit_count);
    QuerySync* sync = GetSync(query);

    QuerySyncManager::Bucket* bucket = GetBucket(query);
    uint32_t use_count_before_remove = GetBucketUsedCount(bucket);
    bucket->FreePendingSyncs();
    EXPECT_EQ(use_count_before_remove, GetBucketUsedCount(bucket));
    query_tracker_->RemoveQuery(query_id);
    // Check we get nothing for a non-existent query.
    EXPECT_TRUE(query_tracker_->GetQuery(query_id) == nullptr);

    // Check that memory was not freed since it was not completed.
    EXPECT_EQ(use_count_before_remove, GetBucketUsedCount(bucket));

    // Simulate GPU process marking it as available.
    sync->process_count = submit_count;
    sync->result = kResult;

    // Check FreeCompletedQueries.
    bucket->FreePendingSyncs();
    EXPECT_EQ(use_count_before_remove - 1, GetBucketUsedCount(bucket));
  }
}

}  // namespace gles2
}  // namespace gpu
